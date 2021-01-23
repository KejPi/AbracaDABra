#include <QDebug>
#include <QThread>
#include "audiooutput.h"

audioFifo_t audioBuffer;



#ifdef AUDIOOUTPUT_USE_PORTAUDIO
//#define AUDIOOUTPUT_PORTAUDIO_NO_STARTUP_TIMER

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    inFifoPtr = buffer;
    audioStartTimer = nullptr;
    audioOutput = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    dbgTimer = nullptr;
#endif

    PaError err = Pa_Initialize();
    if( err != paNoError )
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }
}

AudioOutput::~AudioOutput()
{
    if (nullptr != audioOutput)
    {
        Pa_CloseStream(audioOutput);
    }

    PaError err = Pa_Terminate();
    if( err != paNoError )
    {
       qDebug("PortAudio Pa_Terminate() error: %s", Pa_GetErrorText(err));
    }

    destroyTimer();
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    stop();

    sampleRate = sRate;
    numChannels = numCh;

    inFifoPtr->bytesPerFrame = numCh * sizeof(int16_t);
    bufferFrames = AUDIO_FIFO_CHUNK_MS/2 * sampleRate/1000;   // 120 ms (FIFO size should be integer multiple of this)

    /* Open an audio I/O stream. */
    PaError err = Pa_OpenDefaultStream( &audioOutput,
                                        0,              /* no input channels */
                                        numCh,          /* stereo output */
                                        paInt16,        /* 16 bit floating point output */
                                        sRate,
                                        bufferFrames,   /* frames per buffer, i.e. the number
                                                           of sample frames that PortAudio will
                                                           request from the callback. Many apps
                                                           may want to use
                                                           paFramesPerBufferUnspecified, which
                                                           tells PortAudio to pick the best,
                                                           possibly changing, buffer size.*/
                                        AudioOutput::portAudioCb,    /* callback */
                                        (void *) inFifoPtr );
    if (err!=paNoError )
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }

    qDebug() << Q_FUNC_INFO << "bufferFrames =" << bufferFrames;

#if (AUDIOOUTPUT_PORTAUDIO_NO_STARTUP_TIMER)
    PaError err = Pa_StartStream(audioOutput);
    if( err != paNoError )
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }
#else
    initTimer();
#endif

#if AUDIOOUTPUT_DBG_TIMER
    // DBG counter for buffer monitoring
    if (nullptr == dbgTimer)
    {
        dbgTimer = new QTimer(this);
        dbgTimer->setInterval(1000);
        minCount = INT64_MAX;
        maxCount = INT64_MIN;
        sum = 0;
        memset((uint8_t *)buf, 0, AUDIOOUTPUT_DBG_AVRG_SIZE*sizeof(int64_t));
        connect(dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = bytesAvailable();
    if (count > maxCount) maxCount = count;
    if (count < minCount) minCount = count;
    sum -= buf[cntr];
    buf[cntr++] = count;
    cntr = cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    sum+= count;
    float avrgMs = sum * 1000.0 / (sizeof(int16_t)*numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * sampleRate);

    qDebug("Buffer monitor [%lld - %lld] : %6lld bytes => %6lld samples => %d [avrg %.2f] ms", minCount, maxCount,
           count, count >> 2, qRound((count>>2)*1000.0/sampleRate), avrgMs);
}
#endif

void AudioOutput::initTimer()
{
    if (nullptr != audioStartTimer)
    {
        delete audioStartTimer;
    }
    audioStartTimer = new QTimer(this);
    audioStartTimer->setInterval(AUDIO_FIFO_CHUNK_MS/4);
    connect(audioStartTimer, &QTimer::timeout, this, &AudioOutput::checkInputBuffer);

    // timer starts output when buffer is full enough
    audioStartTimer->start();
}

void AudioOutput::destroyTimer()
{
    if (nullptr != audioStartTimer)
    {
        audioStartTimer->stop();
        delete audioStartTimer;
        audioStartTimer = nullptr;
    }
}

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != audioOutput)
    {
        Pa_CloseStream(audioOutput);
        audioOutput = nullptr;
    }

#if AUDIOOUTPUT_DBG_TIMER
    delete dbgTimer;
    dbgTimer = nullptr;
#endif
    destroyTimer();
}

void AudioOutput::checkInputBuffer()
{
    uint64_t count = bytesAvailable();

    qDebug() << Q_FUNC_INFO << count;

    // waiting for >4x AUDIO_FIFO_CHUNK_MS in buffer
    if (count > 4 * AUDIO_FIFO_CHUNK_MS * sampleRate/1000 * numChannels * sizeof(int16_t))
    {
        if (1 == Pa_IsStreamActive(audioOutput))
        {
            destroyTimer();
        }
        else
        {
            PaError err = Pa_StartStream(audioOutput);
            if( err != paNoError )
            {
                throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
            }
            destroyTimer();
        }
    }
}

int64_t AudioOutput::bytesAvailable() const
{
    inFifoPtr->mutex.lock();
    int64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    return count;
}

int AudioOutput::portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                 const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *buffer)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(timeInfo);

    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    if (statusFlags & paOutputUnderflow)
    {
        qDebug() << "Port Audio underflow!";
    }

    // read samples from input buffer
    audioFifo_t * inFifoPtr = (audioFifo_t *) buffer;
    uint64_t bytesToRead = inFifoPtr->bytesPerFrame * nBufferFrames;

    inFifoPtr->mutex.lock();
    uint64_t count = inFifoPtr->count;

#if (AUDIOOUTPUT_PORTAUDIO_NO_STARTUP_TIMER)
    if (count < 3*bytesToRead)
    {
        qDebug() << "No enough samples in buffer - trying to refill";
        while (count < 6*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#else
//    while (count < bytesToRead)
//    {
//        inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
//        count = inFifoPtr->count;
//    }
    if (count < bytesToRead)
    {
        qDebug() << "Not enough samples in buffer - trying to refill";
        while (count < 4*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#endif

    inFifoPtr->mutex.unlock();

    uint64_t bytesToEnd = AUDIO_FIFO_SIZE - inFifoPtr->tail;
    if (bytesToEnd < bytesToRead)
    {
        memcpy((uint8_t*)outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToEnd);
        memcpy((uint8_t*)outputBuffer + bytesToEnd, inFifoPtr->buffer, (bytesToRead - bytesToEnd));
        inFifoPtr->tail = bytesToRead - bytesToEnd;
    }
    else
    {
        memcpy((uint8_t*) outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToRead);
        inFifoPtr->tail += bytesToRead;
    }

    inFifoPtr->mutex.lock();
    inFifoPtr->count -= bytesToRead;
    inFifoPtr->countChanged.wakeAll();
    inFifoPtr->mutex.unlock();

    return 0;
}

#if 0
// RT audio callback function
// static private function
int AudioOutput::rtAudioCb(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                              double streamTime, RtAudioStreamStatus status, void *buffer)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(streamTime);

    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    if (status)
    {
        qDebug() << "RT Audio underflow!";
    }

    // read samples from input buffer
    audioFifo_t * inFifoPtr = (audioFifo_t *) buffer;
    uint64_t bytesToRead = inFifoPtr->bytesPerFrame * nBufferFrames;

    inFifoPtr->mutex.lock();
    uint64_t count = inFifoPtr->count;

#if (AUDIOOUTPUT_RTAUDIO_NO_STARTUP_TIMER)
    if (count < 3*bytesToRead)
    {
        qDebug() << "No enough samples in buffer - trying to refill";
        while (count < 6*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#else
//    while (count < bytesToRead)
//    {
//        inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
//        count = inFifoPtr->count;
//    }
    if (count < bytesToRead)
    {
        qDebug() << "Not enough samples in buffer - trying to refill";
        while (count < 4*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#endif

    inFifoPtr->mutex.unlock();

    uint64_t bytesToEnd = AUDIO_FIFO_SIZE - inFifoPtr->tail;
    if (bytesToEnd < bytesToRead)
    {
        memcpy((uint8_t*)outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToEnd);
        memcpy((uint8_t*)outputBuffer + bytesToEnd, inFifoPtr->buffer, (bytesToRead - bytesToEnd));
        inFifoPtr->tail = bytesToRead - bytesToEnd;
    }
    else
    {
        memcpy((uint8_t*) outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToRead);
        inFifoPtr->tail += bytesToRead;
    }

    inFifoPtr->mutex.lock();
    inFifoPtr->count -= bytesToRead;
    inFifoPtr->countChanged.wakeAll();
    inFifoPtr->mutex.unlock();

    return 0;
}

void AudioOutput::rtAudioErrorCb(RtAudioError::Type type, const std::string &errorText)
{
    Q_UNUSED(type);
    qDebug("RT Audio ERROR: %s", errorText.c_str());
}
#endif


#else // def AUDIOOUTPUT_USE_PORTAUDIO
#ifdef AUDIOOUTPUT_USE_RTAUDIO
#define AUDIOOUTPUT_RTAUDIO_NO_STARTUP_TIMER 0

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    inFifoPtr = buffer;
    audioOutput = nullptr;
    audioStartTimer = nullptr;
    audioOutput = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    dbgTimer = nullptr;
#endif
}

AudioOutput::~AudioOutput()
{
    if (nullptr != audioOutput)
    {
        if (audioOutput->isStreamRunning())
        {
            try
            {   // Stop the stream
                audioOutput->abortStream();
            }
            catch (RtAudioError& e)
            {
                e.printMessage();
            }
        }
        if ( audioOutput->isStreamOpen() )
        {
            try
            {
                audioOutput->closeStream();
            }
            catch (RtAudioError& e)
            {
                e.printMessage();
            }
        }
        delete audioOutput;
    }
    destroyTimer();
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    stop();

    sampleRate = sRate;
    numChannels = numCh;

    audioOutput = new RtAudio();
    if ( audioOutput->getDeviceCount() < 1 )
    {
        qDebug() << "No audio devices found!";
        throw 0;
    }

    RtAudio::StreamParameters parameters;
    parameters.deviceId = audioOutput->getDefaultOutputDevice();
    parameters.nChannels = numCh;
    parameters.firstChannel = 0;

    inFifoPtr->bytesPerFrame = numCh * sizeof(int16_t);
    bufferFrames = AUDIO_FIFO_CHUNK_MS/2 * sampleRate/1000;   // 120 ms (FIFO size should be integer multiple of this)
    try
    {
        audioOutput->openStream( &parameters, nullptr, RTAUDIO_SINT16, sampleRate,
                                 &bufferFrames, &AudioOutput::rtAudioCb, (void *) inFifoPtr,
                                 nullptr, &AudioOutput::rtAudioErrorCb);

    }
    catch (RtAudioError& e)
    {
        e.printMessage();
    }
    qDebug() << Q_FUNC_INFO << "bufferFrames =" << bufferFrames;

#if (AUDIOOUTPUT_RTAUDIO_NO_STARTUP_TIMER)
    try
    {
        audioOutput->startStream();
    }
    catch ( RtAudioError& e )
    {
        e.printMessage();
    }

#else
    initTimer();
#endif

#if AUDIOOUTPUT_DBG_TIMER
    // DBG counter for buffer monitoring
    if (nullptr == dbgTimer)
    {
        dbgTimer = new QTimer(this);
        dbgTimer->setInterval(1000);
        minCount = INT64_MAX;
        maxCount = INT64_MIN;
        sum = 0;
        memset((uint8_t *)buf, 0, AUDIOOUTPUT_DBG_AVRG_SIZE*sizeof(int64_t));
        connect(dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = bytesAvailable();
    if (count > maxCount) maxCount = count;
    if (count < minCount) minCount = count;
    sum -= buf[cntr];
    buf[cntr++] = count;
    cntr = cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    sum+= count;
    float avrgMs = sum * 1000.0 / (sizeof(int16_t)*numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * sampleRate);

    qDebug("Buffer monitor [%lld - %lld] : %6lld bytes => %6lld samples => %d [avrg %.2f] ms", minCount, maxCount,
           count, count >> 2, qRound((count>>2)*1000.0/sampleRate), avrgMs);
}
#endif

void AudioOutput::initTimer()
{
    if (nullptr != audioStartTimer)
    {
        delete audioStartTimer;
    }
    audioStartTimer = new QTimer(this);
    audioStartTimer->setInterval(AUDIO_FIFO_CHUNK_MS/4);
    connect(audioStartTimer, &QTimer::timeout, this, &AudioOutput::checkInputBuffer);

    // timer starts output when buffer is full enough
    audioStartTimer->start();
}

void AudioOutput::destroyTimer()
{
    if (nullptr != audioStartTimer)
    {
        audioStartTimer->stop();
        delete audioStartTimer;
        audioStartTimer = nullptr;
    }
}

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;

    if (nullptr != audioOutput)
    {
        try {
          // Stop the stream
          audioOutput->stopStream();
        }
        catch (RtAudioError& e) {
          e.printMessage();
        }
        delete audioOutput;
        audioOutput = nullptr;
    }

#if AUDIOOUTPUT_DBG_TIMER
    delete dbgTimer;
    dbgTimer = nullptr;
#endif
    destroyTimer();
}

void AudioOutput::checkInputBuffer()
{
    uint64_t count = bytesAvailable();

    qDebug() << Q_FUNC_INFO << count;

    // waiting for >4x AUDIO_FIFO_CHUNK_MS in buffer
    if (count > 4 * AUDIO_FIFO_CHUNK_MS * sampleRate/1000 * numChannels * sizeof(int16_t))
    {
        if (audioOutput->isStreamRunning())
        {
            destroyTimer();
        }
        else
        {
            try
            {
                audioOutput->startStream();
            }
            catch ( RtAudioError& e )
            {
                e.printMessage();
            }
            destroyTimer();
        }
    }
}

int64_t AudioOutput::bytesAvailable() const
{
    inFifoPtr->mutex.lock();
    int64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    return count;
}

// RT audio callback function
// static private function
int AudioOutput::rtAudioCb(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                              double streamTime, RtAudioStreamStatus status, void *buffer)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(streamTime);

    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    if (status)
    {
        qDebug() << "RT Audio underflow!";
    }

    // read samples from input buffer
    audioFifo_t * inFifoPtr = (audioFifo_t *) buffer;
    uint64_t bytesToRead = inFifoPtr->bytesPerFrame * nBufferFrames;

    inFifoPtr->mutex.lock();
    uint64_t count = inFifoPtr->count;

#if (AUDIOOUTPUT_RTAUDIO_NO_STARTUP_TIMER)
    if (count < 3*bytesToRead)
    {
        qDebug() << "No enough samples in buffer - trying to refill";
        while (count < 6*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#else
//    while (count < bytesToRead)
//    {
//        inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
//        count = inFifoPtr->count;
//    }
    if (count < bytesToRead)
    {
        qDebug() << "Not enough samples in buffer - trying to refill";
        while (count < 4*bytesToRead)
        {
            inFifoPtr->countChanged.wait(&inFifoPtr->mutex);
            count = inFifoPtr->count;
        }
    }
#endif

    inFifoPtr->mutex.unlock();

    uint64_t bytesToEnd = AUDIO_FIFO_SIZE - inFifoPtr->tail;
    if (bytesToEnd < bytesToRead)
    {
        memcpy((uint8_t*)outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToEnd);
        memcpy((uint8_t*)outputBuffer + bytesToEnd, inFifoPtr->buffer, (bytesToRead - bytesToEnd));
        inFifoPtr->tail = bytesToRead - bytesToEnd;
    }
    else
    {
        memcpy((uint8_t*) outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToRead);
        inFifoPtr->tail += bytesToRead;
    }

    inFifoPtr->mutex.lock();
    inFifoPtr->count -= bytesToRead;
    inFifoPtr->countChanged.wakeAll();
    inFifoPtr->mutex.unlock();

    return 0;
}

void AudioOutput::rtAudioErrorCb(RtAudioError::Type type, const std::string &errorText)
{
    Q_UNUSED(type);
    qDebug("RT Audio ERROR: %s", errorText.c_str());
}

#else

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    ioDevice = new AudioIODevice(buffer, this);
    audioOutput = nullptr;
    audioStartTimer = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    dbgTimer = nullptr;
#endif
}

AudioOutput::~AudioOutput()
{
    if (nullptr != audioOutput)
    {
        audioOutput->stop();
        delete audioOutput;
    }
    destroyTimer();
#if AUDIOOUTPUT_DBG_TIMER
    if (nullptr != dbgTimer)
    {
        dbgTimer->stop();
        delete dbgTimer;
    }
#endif
    ioDevice->close();
    delete ioDevice;
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    stop();

    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    sampleRate = sRate;
    numChannels = numCh;

    QAudioFormat format;
    format.setSampleRate(sRate);
    format.setChannelCount(numCh);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo deviceInfo = QAudioDeviceInfo::defaultOutputDevice();
    if (!deviceInfo.isFormatSupported(format))
    {
        qWarning() << "Default format not supported - trying to use nearest";
        format = deviceInfo.nearestFormat(format);
    }
    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    audioOutput = new QAudioOutput(deviceInfo, format, this);

    // set buffer size to AUDIO_FIFO_MS/2 ms
    audioOutput->setBufferSize(AUDIO_FIFO_CHUNK_MS * sRate/1000 * numCh * sizeof(uint16_t));
    connect(audioOutput, &QAudioOutput::stateChanged, this, &AudioOutput::handleStateChanged);

    // start IO device
    ioDevice->open(QIODevice::ReadOnly);

    initTimer();

#if AUDIOOUTPUT_DBG_TIMER
    // DBG counter for buffer monitoring
    if (nullptr == dbgTimer)
    {
        dbgTimer = new QTimer(this);
        dbgTimer->setInterval(1000);
        minCount = INT64_MAX;
        maxCount = INT64_MIN;
        connect(dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = ioDevice->bytesAvailable();

    if (count > maxCount) maxCount = count;
    if (count < minCount) minCount = count;
    sum -= buf[cntr];
    buf[cntr++] = count;
    cntr = cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    sum+= count;
    float avrgMs = sum * 1000.0 / (sizeof(int16_t)*numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * sampleRate);

    qDebug("Buffer monitor [%lld - %lld] : %lld bytes => %lld samples => %d [avrg %.2f] ms", minCount, maxCount,
           count, count >> 2, qRound((count>>2)*1000.0/sampleRate), avrgMs);
}
#endif

void AudioOutput::initTimer()
{
    if (nullptr != audioStartTimer)
    {
        delete audioStartTimer;
    }
    audioStartTimer = new QTimer(this);
    audioStartTimer->setInterval(AUDIO_FIFO_CHUNK_MS/2);
    connect(audioStartTimer, &QTimer::timeout, this, &AudioOutput::checkInputBuffer);

    // timer starts output when buffer is full enough
    audioStartTimer->start();
}

void AudioOutput::destroyTimer()
{
    if (nullptr != audioStartTimer)
    {
        audioStartTimer->stop();
        delete audioStartTimer;
        audioStartTimer = nullptr;
    }
}

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != audioOutput)
    {
        audioOutput->stop();
        audioOutput->disconnect(this);
        delete audioOutput;
        audioOutput = nullptr;
    }

    ioDevice->stop();

#if AUDIOOUTPUT_DBG_TIMER
    delete dbgTimer;
    dbgTimer = nullptr;
#endif
    destroyTimer();
}

void AudioOutput::handleStateChanged(QAudio::State newState)
{
    qDebug() << newState;
    switch (newState)
    {
        case QAudio::ActiveState:
            destroyTimer();
            break;
        case QAudio::IdleState:
            // no more data
            audioOutput->stop();
            initTimer();
            break;

        case QAudio::StoppedState:
            // Stopped for other reasons
            break;

        default:
            // ... other cases as appropriate
            break;
    }
}

void AudioOutput::checkInputBuffer()
{
    uint64_t count = ioDevice->bytesAvailable();

    qDebug() << Q_FUNC_INFO << count;

    // waiting for 1/2 buffer full
    if (count > 4 * AUDIO_FIFO_CHUNK_MS * sampleRate/1000 * numChannels * sizeof(int16_t))
    {
        switch (audioOutput->state())
        {
            case QAudio::IdleState:
                // Do nothing, shoud restart automatically
                break;

            case QAudio::StoppedState:
                // Stopped for other reasons
                audioOutput->start(ioDevice);
                qDebug() << "Current buffer size is" << audioOutput->bufferSize();
                break;

            default:
                // do nothing
                qDebug() << Q_FUNC_INFO << "unexpected state" << audioOutput->state();
                break;
        }
    }
}
#endif // def AUDIOOUTPUT_USE_RTAUDIO
#endif // def AUDIOOUTPUT_USE_PORTAUDIO

AudioIODevice::AudioIODevice(audioFifo_t *buffer, QObject *parent) : QIODevice(parent)
{
    inFifoPtr = buffer;
}

void AudioIODevice::start()
{
    open(QIODevice::ReadOnly);
}

void AudioIODevice::stop()
{
    close();
}

int64_t AudioIODevice::readData(char *data, int64_t len)
{
    inFifoPtr->mutex.lock();
    int64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    //qDebug() << Q_FUNC_INFO << len << count;
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    // samples to be read
    if (count < len)
    {   // only count samples are available
        len = count;
        if (0 == count)
        {
            return 0;
        }
    }

    int64_t bytesToEnd = AUDIO_FIFO_SIZE - inFifoPtr->tail;
    if (bytesToEnd < len)
    {
        memcpy(data, inFifoPtr->buffer+inFifoPtr->tail, bytesToEnd);
        memcpy(data+bytesToEnd, inFifoPtr->buffer, (len - bytesToEnd));
        inFifoPtr->tail = len - bytesToEnd;
    }
    else
    {
        memcpy(data, inFifoPtr->buffer+inFifoPtr->tail, len);
        inFifoPtr->tail += len;
    }

    inFifoPtr->mutex.lock();
    inFifoPtr->count -= len;
    inFifoPtr->countChanged.wakeAll();
    inFifoPtr->mutex.unlock();

    return len;
}

int64_t AudioIODevice::writeData(const char *data, int64_t len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

int64_t AudioIODevice::bytesAvailable() const
{
    inFifoPtr->mutex.lock();
    int64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    //qDebug() << Q_FUNC_INFO << count;

    return count; // + QIODevice::bytesAvailable();
}
