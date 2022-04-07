#include <QDebug>
#include <QFile>

#include <complex>
#include "rawfileinput.h"

RawFileInput::RawFileInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::RAWFILE;

    worker = nullptr;
    inputTimer = nullptr;
    inputFile = nullptr;    
    sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8;    
}

RawFileInput::~RawFileInput()
{
    if (nullptr != worker)
    {
        worker->stop();
        worker->quit();
        worker->wait();
    }

    if (nullptr != inputFile)
    {
        inputFile->close();
        delete inputFile;
    }   
}

void RawFileInput::openFile(const QString & fileName, const RawFileInputFormat &format)
{
    if (nullptr != inputFile)
    {
        inputFile->close();
        delete inputFile;
    }
    inputFile = new QFile(fileName);
    if (!inputFile->open(QIODevice::ReadOnly))
    {
        qDebug() << "Unable to open file: " << fileName;
        delete inputFile;
        inputFile = nullptr;
    }

    setFileFormat(format);

    if (getNumSamples() > 0)
    {
        emit deviceReady();
    }
}

uint64_t RawFileInput::getNumSamples()
{
    if (nullptr != inputFile)
    {
        int bytesPerSample = 1;
        switch (sampleFormat)
        {
        case RawFileInputFormat::SAMPLE_FORMAT_S16:
            bytesPerSample = sizeof(int16_t) * 2;
            break;
        case RawFileInputFormat::SAMPLE_FORMAT_U8:
            bytesPerSample = sizeof(uint16_t) * 1;
            break;
        }
        uint64_t numSamples = inputFile->size() / bytesPerSample;
        qDebug("Number of samples: %lld => %f sec\n", numSamples, numSamples/2048e3);
        qDebug("Expected NULLs: %lld\n", numSamples/(2656+2552*76));
        return numSamples;
    }
    return 0;
}

void RawFileInput::setFileFormat(const RawFileInputFormat &format)
{
    sampleFormat = format;
}

void RawFileInput::tune(uint32_t freq)
{
    stop();    
    rewind();

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (0 != freq)
    {
        worker = new RawFileWorker(inputFile, sampleFormat, this);
        connect(worker, &RawFileWorker::endOfFile, this, &RawFileInput::onEndOfFile, Qt::QueuedConnection);
        connect(worker, &RawFileWorker::finished, worker, &QObject::deleteLater);
        worker->start();

        inputTimer = new QTimer(this);
        connect(inputTimer, &QTimer::timeout, worker, &RawFileWorker::trigger);
        inputTimer->start(INPUT_CHUNK_MS);
    }
    emit tuned(freq);
}

void RawFileInput::rewind()
{
    if (nullptr == worker)
    {   // avoid multiple access - if thread is running, then no action
        // go to file beginning
        if (nullptr != inputFile)
        {
            inputFile->seek(0);
        }
    }
}

void RawFileInput::stop()
{
    if (nullptr != inputTimer)
    {
        inputTimer->stop();
    }
    if (nullptr != worker)
    {
        worker->stop();
        worker->wait(INPUT_CHUNK_MS*2);
        worker = nullptr;
    }
}


RawFileWorker::RawFileWorker(QFile *inFile, RawFileInputFormat sFormat, QObject *parent)
    : QThread(parent)
    , sampleFormat(sFormat)
    , inputFile(inFile)
{
    stopRequest = false;
    elapsedTimer.start();
}

void RawFileWorker::trigger()
{
    sem.release();
}

void RawFileWorker::stop()
{
    stopRequest = true;
    sem.release();
}

void RawFileWorker::run()
{
    while(1)
    {
        sem.acquire();

        if (stopRequest)
        {   // stop request
            return;
        }

        qint64 elapsed = elapsedTimer.elapsed();
        int period = elapsed - lastTriggerTime;
        lastTriggerTime = elapsed;

        //qDebug() << Q_FUNC_INFO << elapsed << " ==> " << period;

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

        switch (sampleFormat)
        {
        case RawFileInputFormat::SAMPLE_FORMAT_S16:
        {
            int16_t * tmpBuffer = new int16_t[input_chunk_iq_samples*2];
            uint64_t bytesRead = inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(int16_t));

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
            uint64_t bytesRead = inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(uint8_t));

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

        //qDebug() << Q_FUNC_INFO << samplesRead;

        inputBuffer.head = (inputBuffer.head + samplesRead*sizeof(float)) % INPUT_FIFO_SIZE;
        pthread_mutex_lock(&inputBuffer.countMutex);
        inputBuffer.count = inputBuffer.count + samplesRead*sizeof(float);
        //qDebug() << Q_FUNC_INFO << inputBuffer.count;
        pthread_cond_signal(&inputBuffer.countCondition);
        pthread_mutex_unlock(&inputBuffer.countMutex);

        if (samplesRead < input_chunk_iq_samples*2)
        {
            qDebug() << "End of file";
            inputFile->seek(0);
            emit endOfFile();
        }
    }
}
