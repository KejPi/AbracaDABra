#include <QDebug>
#include <QFile>

#include <complex>
#include "rawfileinput.h"

RawFileInput::RawFileInput(QObject *parent) : InputDevice(parent)
{        
    inputTimer = nullptr;
    inputFile = nullptr;
    sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8;
}

RawFileInput::~RawFileInput()
{
    if (nullptr != inputTimer)
    {
        inputTimer->stop();
        delete inputTimer;
    }

    if (nullptr != inputFile)
    {
        inputFile->close();
        delete inputFile;
    }
#if INPUT_USE_PTHREADS
    pthread_mutex_destroy(&inputBuffer.countMutex);
    pthread_cond_destroy(&inputBuffer.countCondition);
#endif
}

void RawFileInput::openDevice(const QString & fileName, const RawFileInputFormat &format)
{    
    if (nullptr != inputTimer)
    {
        inputTimer->stop();
        delete inputTimer;
        inputTimer = nullptr;
    }

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
    emit numberOfSamples(getNumSamples());
}

void RawFileInput::tune(uint32_t freq)
{
    stop();

    rewind();

    if (0 != freq)
    {
        if (NULL == inputTimer)
        {
            inputTimer = new QTimer(this);
            connect(inputTimer, &QTimer::timeout, this, &RawFileInput::readSamples);
            // input never stops
            connect(this, &RawFileInput::endOfFile, this, &RawFileInput::rewind);
        }
        inputTimer->start(INPUT_CHUNK_MS);
        qDebug() << "Timer started" << inputTimer->isActive();
    }
    emit tuned(freq);
}

void RawFileInput::rewind()
{
    // go to file beginning
    if (nullptr != inputFile)
    {
        inputFile->seek(0);
    }
}

void RawFileInput::stop()
{
    if (nullptr != inputTimer)
    {
        inputTimer->stop();
    }
}

void RawFileInput::readSamples()
{
    //qDebug() << Q_FUNC_INFO;
    uint64_t samplesRead = 0;

    // get FIFO space
#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
#endif
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    while ((INPUT_FIFO_SIZE - count) < INPUT_CHUNK_IQ_SAMPLES*sizeof(float)*2)
    {

#if INPUT_USE_PTHREADS
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
#else
        inputBuffer.countChanged.wait(&inputBuffer.mutex);
#endif
        count = inputBuffer.count;
    }
#if INPUT_USE_PTHREADS
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.unlock();
#endif

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

    switch (sampleFormat)
    {
    case RawFileInputFormat::SAMPLE_FORMAT_S16:
    {
        int16_t * tmpBuffer = new int16_t[INPUT_CHUNK_IQ_SAMPLES*2];
        uint64_t bytesRead = inputFile->read((char *) tmpBuffer, INPUT_CHUNK_IQ_SAMPLES * 2 * sizeof(int16_t));

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
        uint8_t * tmpBuffer = new uint8_t[INPUT_CHUNK_IQ_SAMPLES*2];
        uint64_t bytesRead = inputFile->read((char *) tmpBuffer, INPUT_CHUNK_IQ_SAMPLES * 2 * sizeof(uint8_t));

        samplesRead = bytesRead;  // one sample is uint8 => 1 byte

        uint8_t * inPtr = tmpBuffer;
        if (bytesTillEnd >= samplesRead * sizeof(float))
        {
            float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
            for (uint64_t k=0; k < samplesRead; k++)
            {   // convert to float
                *outPtr++ = float((*inPtr++ - 127)<<8);  // I or Q
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
                *outPtr++ = float((*inPtr++ - 127)<<8);  // I or Q
            }
        }
        delete [] tmpBuffer;
    }
        break;
    }

    //qDebug() << Q_FUNC_INFO << samplesRead;

    inputBuffer.head = (inputBuffer.head + samplesRead*sizeof(float)) % INPUT_FIFO_SIZE;
#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + samplesRead*sizeof(float);
    //qDebug() << Q_FUNC_INFO << inputBuffer.count;
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
    inputBuffer.count = inputBuffer.count + samplesRead*sizeof(float);
    inputBuffer.active = (samplesRead == INPUT_CHUNK_SAMPLES);
    inputBuffer.countChanged.wakeAll();
    inputBuffer.mutex.unlock();
#endif

    if (samplesRead < INPUT_CHUNK_IQ_SAMPLES*2)
    {
        qDebug() << "End of file";
        emit endOfFile();
        return;
    }
}

