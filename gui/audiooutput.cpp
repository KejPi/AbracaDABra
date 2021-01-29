#include <QDebug>
#include <QThread>
#include "audiooutput.h"

audioFifo_t audioBuffer;

#ifdef AUDIOOUTPUT_USE_PORTAUDIO

int portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                 const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    inFifoPtr = buffer;
    outStream = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    dbgTimer = nullptr;
#endif

    PaError err = Pa_Initialize();
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    rawOut = fopen("audio.raw", "wb");
    if (!rawOut)
    {
        qDebug("Unable to open file: audio.raw");
    }
#endif
}

AudioOutput::~AudioOutput()
{
    if (nullptr != outStream)
    {
        if (Pa_IsStreamActive(outStream))
        {
            Pa_AbortStream(outStream);
        }
        Pa_CloseStream(outStream);
    }

    PaError err = Pa_Terminate();
    if (paNoError != err)
    {
       qDebug("PortAudio Pa_Terminate() error: %s", Pa_GetErrorText(err));
    }

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    if (rawOut)
    {
        fclose(rawOut);
    }
#endif
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    stop();

    sampleRate_kHz = sRate/1000;
    numChannels = numCh;

    bytesPerFrame = numCh * sizeof(int16_t);
    bufferFrames = AUDIO_FIFO_CHUNK_MS * sampleRate_kHz;   // 120 ms (FIFO size should be integer multiple of this)

    /* Open an audio I/O stream. */
    PaError err = Pa_OpenDefaultStream( &outStream,
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
                                        portAudioCb,    /* callback */
                                        (void *) this);
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }

    isMuted = true;

    err = Pa_StartStream(outStream);
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }

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
    float avrgMs = sum / (sizeof(int16_t)*numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * sampleRate_kHz);

    qDebug("Buffer monitor [%lld - %lld] : %4lld [avrg %.2f] ms", minCount, maxCount, (count/bytesPerFrame)/sampleRate_kHz, avrgMs);
}
#endif

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != outStream)
    {
        if (Pa_IsStreamActive(outStream))
        {
            Pa_StopStream(outStream);
        }
        Pa_CloseStream(outStream);
        outStream = nullptr;
    }

#if AUDIOOUTPUT_DBG_TIMER
    delete dbgTimer;
    dbgTimer = nullptr;
#endif
}

int64_t AudioOutput::bytesAvailable() const
{
    inFifoPtr->mutex.lock();
    int64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    return count;
}

int portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                 const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(timeInfo);

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    int ret = static_cast<AudioOutput*>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames, statusFlags);
    if (static_cast<AudioOutput*>(ctx)->rawOut)
    {
        fwrite(outputBuffer, sizeof(int16_t), nBufferFrames * static_cast<AudioOutput*>(ctx)->numChannels, static_cast<AudioOutput*>(ctx)->rawOut);
    }
    return ret;
#else
    return static_cast<AudioOutput*>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames, statusFlags);
#endif
}

int AudioOutput::portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames, PaStreamCallbackFlags statusFlags)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

#define FADE_TIME_MS 40

    if (statusFlags & paOutputUnderflow)
    {
        qDebug() << "Port Audio underflow!";
    }

    // read samples from input buffer
    uint64_t bytesToRead = bytesPerFrame * nBufferFrames;

    inFifoPtr->mutex.lock();
    uint64_t count = inFifoPtr->count;
    inFifoPtr->mutex.unlock();

    if (isMuted)
    {
        if (count < 6*bytesToRead)
        {   // insert silence
            qDebug("Muted: Inserting silence [%lu ms]", nBufferFrames/sampleRate_kHz);
            memset(outputBuffer, 0, bytesToRead);
            return paContinue;
        }
    }
    else
    {
        if (count < bytesToRead)  // threshold for mute
        {   // mute

            if (0 == count)
            { // nothing to mute
                qDebug("Hard mute [no samples available]");
                memset(outputBuffer, 0, bytesToRead);
                isMuted = true;
                return paContinue;
            }

            // mute
            uint32_t availableSamples = count/bytesPerFrame;
            qDebug("Muting... [available %u samples]", availableSamples);

#if 1  // mute implementation
            Q_ASSERT(count == availableSamples*bytesPerFrame);

            // copy all available samples to output buffer
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - inFifoPtr->tail;
            if (bytesToEnd < count)
            {
                memcpy((uint8_t*)outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)outputBuffer + bytesToEnd, inFifoPtr->buffer, (count - bytesToEnd));
                inFifoPtr->tail = count - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) outputBuffer, inFifoPtr->buffer+inFifoPtr->tail, count);
                inFifoPtr->tail += count;
            }
            // set rest of the samples to be 0
            memset((uint8_t*)outputBuffer+count, 0, bytesToRead-count);

            // apply mute ramp
            uint32_t fadeSamples = FADE_TIME_MS * sampleRate_kHz;
            if (availableSamples < fadeSamples)
            {
                fadeSamples = availableSamples;
            }

            uint32_t fadeBytes = fadeSamples * bytesPerFrame;
            float gDec = 1.0/fadeSamples;
            float gain = 1.0;

            int16_t * dataPtr = (int16_t *) ((uint8_t * ) outputBuffer + (count - fadeBytes));
            for (uint32_t n = 0; n < fadeSamples; ++n)
            {
                for (int c = 0; c < numChannels; ++c)
                {
                    *dataPtr = int16_t(roundf(gain * *dataPtr));
                    dataPtr++;
                }
                gain = gain-gDec;
            }

            inFifoPtr->mutex.lock();
            inFifoPtr->count -= count;
            inFifoPtr->countChanged.wakeAll();
            inFifoPtr->mutex.unlock();
#else
            memset(outputBuffer, 0, bytesToRead);
#endif
            isMuted = true;
            return paContinue;
        }
    }

    // normal operation
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

    if (isMuted)
    {   // apply unmute ramp
        qDebug() << "Unmuting audio";
        uint32_t fadeSamples = FADE_TIME_MS * sampleRate_kHz;
        float gInc = 1.0/fadeSamples;
        float gain = 0;

        int16_t * dataPtr = (int16_t *) outputBuffer;
        for (uint32_t n = 0; n < fadeSamples; ++n)
        {
            for (int c = 0; c < numChannels; ++c)
            {
                *dataPtr = int16_t(roundf(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain+gInc;
        }
        isMuted = false;
    }

    inFifoPtr->mutex.lock();
    inFifoPtr->count -= bytesToRead;
    inFifoPtr->countChanged.wakeAll();
    inFifoPtr->mutex.unlock();

    return paContinue;
}


#else // !def AUDIOOUTPUT_USE_PORTAUDIO

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

    sampleRate_kHz = sRate/1000;
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
    audioOutput->setBufferSize(AUDIO_FIFO_CHUNK_MS*2 * sRate/1000 * numCh * sizeof(uint16_t));
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
    float avrgMs = sum / (sizeof(int16_t)*numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * sampleRate_kHz);

    qDebug("Buffer monitor [%lld - %lld] : %lld bytes => %lld samples => %lld [avrg %.2f] ms", minCount, maxCount,
           count, count >> 2, (count>>2)/sampleRate_kHz, avrgMs);
}
#endif

void AudioOutput::initTimer()
{
    if (nullptr != audioStartTimer)
    {
        delete audioStartTimer;
    }
    audioStartTimer = new QTimer(this);
    audioStartTimer->setInterval(AUDIO_FIFO_CHUNK_MS);
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

    // waiting for buffer full enough
    if (count > 8 * AUDIO_FIFO_CHUNK_MS * sampleRate_kHz * numChannels * sizeof(int16_t))
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
#endif // def AUDIOOUTPUT_USE_PORTAUDIO
