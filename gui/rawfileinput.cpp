#include <QDebug>
#include <QFile>

#include <complex>
#include "rawfileinput.h"

RawFileInput::RawFileInput(QObject *parent) : InputDevice(parent)
{
    m_id = InputDeviceId::RAWFILE;

    m_worker = nullptr;
    m_inputTimer = nullptr;
    m_inputFile = nullptr;
    m_sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8;    
}

RawFileInput::~RawFileInput()
{
    if (nullptr != m_worker)
    {
        m_worker->stop();
        m_worker->quit();
        m_worker->wait();
    }

    if (nullptr != m_inputFile)
    {
        m_inputFile->close();
        delete m_inputFile;
    }   
}

bool RawFileInput::openDevice()
{
    if (nullptr != m_inputFile)
    {
        m_inputFile->close();
        delete m_inputFile;
    }
    m_inputFile = new QFile(m_fileName);
    if (!m_inputFile->open(QIODevice::ReadOnly))
    {
        qDebug() << "RAW-FILE: Unable to open file: " << m_fileName;
        delete m_inputFile;
        m_inputFile = nullptr;

        return false;
    }

    emit deviceReady();

    return true;
}

void RawFileInput::setFile(const QString & fileName, const RawFileInputFormat &sampleFormat)
{
    m_fileName = fileName;
    setFileFormat(sampleFormat);
}

void RawFileInput::setFileFormat(const RawFileInputFormat &sampleFormat)
{
    m_sampleFormat = sampleFormat;
}

void RawFileInput::tune(uint32_t freq)
{
    stop();    
    rewind();

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (0 != freq)
    {
        m_worker = new RawFileWorker(m_inputFile, m_sampleFormat, this);
        connect(m_worker, &RawFileWorker::endOfFile, this, &RawFileInput::onEndOfFile, Qt::QueuedConnection);
        connect(m_worker, &RawFileWorker::finished, m_worker, &QObject::deleteLater);
        m_worker->start();

        m_inputTimer = new QTimer(this);
        connect(m_inputTimer, &QTimer::timeout, m_worker, &RawFileWorker::trigger);
        m_inputTimer->start(INPUT_CHUNK_MS);
    }
    emit tuned(freq);
}

void RawFileInput::rewind()
{
    if (nullptr == m_worker)
    {   // avoid multiple access - if thread is running, then no action
        // go to file beginning
        if (nullptr != m_inputFile)
        {
            m_inputFile->seek(0);
        }
    }
}

void RawFileInput::stop()
{
    if (nullptr != m_inputTimer)
    {
        m_inputTimer->stop();
    }
    if (nullptr != m_worker)
    {
        m_worker->stop();
        m_worker->wait(INPUT_CHUNK_MS*2);
        m_worker = nullptr;
    }
}


RawFileWorker::RawFileWorker(QFile *inputFile, RawFileInputFormat sampleFormat, QObject *parent)
    : QThread(parent)
    , m_sampleFormat(sampleFormat)
    , m_inputFile(inputFile)
{
    m_stopRequest = false;
    m_elapsedTimer.start();
}

void RawFileWorker::trigger()
{
    m_semaphore.release();
}

void RawFileWorker::stop()
{
    m_stopRequest = true;
    m_semaphore.release();
}

void RawFileWorker::run()
{
    while(1)
    {
        m_semaphore.acquire();

        if (m_stopRequest)
        {   // stop request
            return;
        }

        qint64 elapsed = m_elapsedTimer.elapsed();
        int period = elapsed - m_lastTriggerTime;
        m_lastTriggerTime = elapsed;

        uint64_t samplesRead = 0;
        uint64_t input_chunk_iq_samples = period * 2048;

        // get FIFO space
        pthread_mutex_lock(&inputBuffer.countMutex);
        uint64_t count = inputBuffer.count;
        Q_ASSERT(count <= INPUT_FIFO_SIZE);

        while ((INPUT_FIFO_SIZE - count) < input_chunk_iq_samples*sizeof(float)*2)
        {

            pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
            count = inputBuffer.count;
        }
        pthread_mutex_unlock(&inputBuffer.countMutex);

        // there is enough room in buffer
        uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

        switch (m_sampleFormat)
        {
        case RawFileInputFormat::SAMPLE_FORMAT_S16:
        {
            int16_t * tmpBuffer = new int16_t[input_chunk_iq_samples*2];
            uint64_t bytesRead = m_inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(int16_t));

            samplesRead = bytesRead >> 1;  // one sample is int16 (I or Q) => 2 bytes

            int16_t * inPtr = tmpBuffer;
            if (bytesTillEnd >= samplesRead * sizeof(float))
            {
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesRead; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
            }
            else
            {
                Q_ASSERT(sizeof(float) == 4);
                uint64_t samplesTillEnd = (bytesTillEnd >> 2);
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
                outPtr = (float *)(inputBuffer.buffer);
                for (uint64_t k=0; k<samplesRead-samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
            }
            delete [] tmpBuffer;
        }
        break;
        case RawFileInputFormat::SAMPLE_FORMAT_U8:
        {
            uint8_t * tmpBuffer = new uint8_t[input_chunk_iq_samples*2];
            uint64_t bytesRead = m_inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(uint8_t));

            samplesRead = bytesRead;  // one sample is uint8 => 1 byte

            uint8_t * inPtr = tmpBuffer;
            if (bytesTillEnd >= samplesRead * sizeof(float))
            {
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesRead; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
            }
            else
            {
                Q_ASSERT(sizeof(float) == 4);
                uint64_t samplesTillEnd = (bytesTillEnd >> 2);
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
                outPtr = (float *)(inputBuffer.buffer);
                for (uint64_t k=0; k<samplesRead-samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
            }
            delete [] tmpBuffer;
        }
        break;
        }

        inputBuffer.head = (inputBuffer.head + samplesRead*sizeof(float)) % INPUT_FIFO_SIZE;
        pthread_mutex_lock(&inputBuffer.countMutex);
        inputBuffer.count = inputBuffer.count + samplesRead*sizeof(float);
        pthread_cond_signal(&inputBuffer.countCondition);
        pthread_mutex_unlock(&inputBuffer.countMutex);

        if (samplesRead < input_chunk_iq_samples*2)
        {
            qDebug() << "RAW-FILE: End of file";
            m_inputFile->seek(0);
            emit endOfFile();
        }
    }
}
