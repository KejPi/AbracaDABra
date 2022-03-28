#include <QDebug>
#include <QThread>
#if QT_VERSION >= 0x060000
#include <QAudioSink>
#include <QAudioDevice>
#endif

#include "audiooutput.h"

audioFifo_t audioBuffer;

#ifdef AUDIOOUTPUT_USE_PORTAUDIO

int portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                 const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    m_inFifoPtr = buffer;
    m_outStream = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    m_dbgTimer = nullptr;
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
    if (nullptr != m_outStream)
    {
        if (Pa_IsStreamActive(m_outStream))
        {
            Pa_AbortStream(m_outStream);
        }
        Pa_CloseStream(m_outStream);
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

    m_sampleRate_kHz = sRate/1000;
    m_numChannels = numCh;

    m_bytesPerFrame = numCh * sizeof(int16_t);
    m_bufferFrames = AUDIO_FIFO_CHUNK_MS * m_sampleRate_kHz;   // 120 ms (FIFO size should be integer multiple of this)

    /* Open an audio I/O stream. */
    PaError err = Pa_OpenDefaultStream( &m_outStream,
                                        0,              /* no input channels */
                                        numCh,          /* stereo output */
                                        paInt16,        /* 16 bit floating point output */
                                        sRate,
                                        m_bufferFrames,   /* frames per buffer, i.e. the number
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

    m_playbackState = StateMuted;

    err = Pa_StartStream(m_outStream);
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
    }

#if AUDIOOUTPUT_DBG_TIMER
    // DBG counter for buffer monitoring
    if (nullptr == m_dbgTimer)
    {
        m_dbgTimer = new QTimer(this);
        m_dbgTimer->setInterval(1000);
        m_minCount = INT64_MAX;
        m_maxCount = INT64_MIN;
        m_sum = 0;
        memset((uint8_t *)m_dbgBuf, 0, AUDIOOUTPUT_DBG_AVRG_SIZE*sizeof(int64_t));
        connect(m_dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    m_dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = bytesAvailable();
    if (count > m_maxCount) m_maxCount = count;
    if (count < m_minCount) m_minCount = count;
    m_sum -= m_dbgBuf[m_cntr];
    m_dbgBuf[m_cntr++] = count;
    m_cntr = m_cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    m_sum+= count;
    float avrgMs = m_sum / (sizeof(int16_t)*m_numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * m_sampleRate_kHz);

    qDebug("Buffer monitor [%lld - %lld] : %4lld [avrg %.2f] ms", m_minCount, m_maxCount, (count/m_bytesPerFrame)/m_sampleRate_kHz, avrgMs);
}
#endif

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != m_outStream)
    {
        if (Pa_IsStreamActive(m_outStream))
        {
            Pa_StopStream(m_outStream);
        }
        Pa_CloseStream(m_outStream);
        m_outStream = nullptr;
    }

#if AUDIOOUTPUT_DBG_TIMER
    delete m_dbgTimer;
    m_dbgTimer = nullptr;
#endif
}

int64_t AudioOutput::bytesAvailable() const
{
    m_inFifoPtr->mutex.lock();
    int64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    return count;
}

void AudioOutput::mute(bool on)
{
    m_muteMutex.lock();
    m_muteFlag = on;
    m_muteMutex.unlock();
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
    if (statusFlags & paOutputUnderflow)
    {
        qDebug() << "Port Audio underflow!";
    }

    // read samples from input buffer
    m_inFifoPtr->mutex.lock();
    uint64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    m_muteMutex.lock();
    bool mute = m_muteFlag;
    m_muteMutex.unlock();

    uint64_t bytesToRead = m_bytesPerFrame * nBufferFrames;
    uint32_t availableSamples = nBufferFrames;

    if (StateMuted == m_playbackState)
    {   // muted
        // condition to unmute is enough samples && !muteFlag
        if (count > 6*bytesToRead)
        {   // enough samples => reading data from input fifo
            if (mute)
            {   // staying muted -> setting output buffer to 0
                memset(outputBuffer, 0, bytesToRead);

                // shifting buffer pointers
                m_inFifoPtr->tail = (m_inFifoPtr->tail + bytesToRead) % AUDIO_FIFO_SIZE;
                m_inFifoPtr->mutex.lock();
                m_inFifoPtr->count -= bytesToRead;
                m_inFifoPtr->countChanged.wakeAll();
                m_inFifoPtr->mutex.unlock();

                // done
                return paContinue;
            }

            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                m_inFifoPtr->tail = bytesToRead - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                m_inFifoPtr->tail += bytesToRead;
            }

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            // unmute request
            m_playbackState = StateDoUnmute;
        }
        else
        {   // not enough samples ==> inserting silence
            //qDebug("Muted: Inserting silence [%lu ms]", nBufferFrames/m_sampleRate_kHz);
            memset(outputBuffer, 0, bytesToRead);

            // done
            return paContinue;
        }
    }
    //if (StatePlaying == playbackState)
    else  // cannot be anything else than StateMuted or StatePlaying
    {   // playing

        Q_ASSERT(StatePlaying == m_playbackState);

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {   // not enough samples -> reading what we have and filling rest with zeros
            if (0 == count)
            {   // nothing to play
                qDebug("Hard mute [no samples available]");
                memset(outputBuffer, 0, bytesToRead);
                m_playbackState = StateMuted;
                return paContinue;
            }

            // there are some samples available
            availableSamples = count/m_bytesPerFrame;

            Q_ASSERT(count == availableSamples*m_bytesPerFrame);

            // copy all available samples to output buffer
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < count)
            {
                memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (count - bytesToEnd));
                m_inFifoPtr->tail = count - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, count);
                m_inFifoPtr->tail += count;
            }
            // set rest of the samples to be 0
            memset((uint8_t*)outputBuffer+count, 0, bytesToRead-count);

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= count;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            m_playbackState = StateDoMute;  // mute
        }
        else
        {   // reading samples
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                m_inFifoPtr->tail = bytesToRead - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                m_inFifoPtr->tail += bytesToRead;
            }

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            if (mute)
            {   // mute requested
                m_playbackState = StateDoMute;
            }
            else
            {   // done
               return paContinue;
            }
        }
    }

    // at this point we have buffer that needs to be muted or unmuted
    // it is indicated by playbackState variable

    // unmute
    if (StateDoUnmute == m_playbackState)
    {   // unmute can be requested only when there is enough samples
        qDebug() << "Unmuting audio";
        uint32_t fadeSamples = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;
        float gInc = 1.0/fadeSamples;
        float gain = 0;

        int16_t * dataPtr = (int16_t *) outputBuffer;
        for (uint32_t n = 0; n < fadeSamples; ++n)
        {
            for (int c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain+gInc;
        }

        m_playbackState = StatePlaying; // playing

        // done
        return paContinue;
    }

    // mute
    if (StateDoMute == m_playbackState)
    {   // mute can be requested when there is not enough samples or from HMI
        qDebug("Muting... [available %u samples]", availableSamples);

        uint32_t fadeSamples = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;
        if (availableSamples < fadeSamples)
        {
            fadeSamples = availableSamples;
        }

        float gDec = 1.0/fadeSamples;
        float gain = 1.0;

        int16_t * dataPtr = (int16_t *) ((uint8_t * ) outputBuffer + (availableSamples - fadeSamples)*m_bytesPerFrame);
        for (uint32_t n = 0; n < fadeSamples; ++n)
        {
            for (int c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain-gDec;
        }

        m_playbackState = StateMuted; // muted
    }

    return paContinue;
}


#else // !def AUDIOOUTPUT_USE_PORTAUDIO

#if QT_VERSION < 0x060000  // Qt5
AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    ioDevice = new AudioIODevice(buffer, this);
    audioOutput = nullptr;
    audioStartTimer = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    m_dbgTimer = nullptr;
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
    if (nullptr != m_dbgTimer)
    {
        m_dbgTimer->stop();
        delete m_dbgTimer;
    }
#endif
    ioDevice->close();
    delete ioDevice;
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    stop();

    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    m_sampleRate_kHz = sRate/1000;
    m_numChannels = numCh;

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
    if (nullptr == m_dbgTimer)
    {
        m_dbgTimer = new QTimer(this);
        m_dbgTimer->setInterval(1000);
        m_minCount = INT64_MAX;
        m_maxCount = INT64_MIN;
        connect(m_dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    m_dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = ioDevice->bytesAvailable();

    if (count > m_maxCount) m_maxCount = count;
    if (count < m_minCount) m_minCount = count;
    m_sum -= m_dbgBuf[m_cntr];
    m_dbgBuf[m_cntr++] = count;
    m_cntr = m_cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    m_sum+= count;
    float avrgMs = m_sum / (sizeof(int16_t)*m_numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * m_sampleRate_kHz);

    qDebug("Buffer monitor [%lld - %lld] : %lld bytes => %lld samples => %lld [avrg %.2f] ms", m_minCount, m_maxCount,
           count, count >> 2, (count>>2)/m_sampleRate_kHz, avrgMs);
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

void AudioOutput::mute(bool on)
{
    if (on)
    {
        audioOutput->setVolume(0);
    }
    else
    {
        audioOutput->setVolume(1.0);
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
    delete m_dbgTimer;
    m_dbgTimer = nullptr;
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
    if (count > 8 * AUDIO_FIFO_CHUNK_MS * m_sampleRate_kHz * m_numChannels * sizeof(int16_t))
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

#else  // Qt6

AudioOutput::AudioOutput(audioFifo_t * buffer)
{
    devices = new  QMediaDevices(this);
    ioDevice = new AudioIODevice(buffer, this);
    audioOutput = nullptr;
#if AUDIOOUTPUT_DBG_TIMER
    m_dbgTimer = nullptr;
#endif
}

AudioOutput::~AudioOutput()
{
    if (nullptr != audioOutput)
    {
        audioOutput->stop();
        delete audioOutput;
    }
#if AUDIOOUTPUT_DBG_TIMER
    if (nullptr != m_dbgTimer)
    {
        m_dbgTimer->stop();
        delete m_dbgTimer;
    }
#endif
    ioDevice->close();
    delete ioDevice;
}

void AudioOutput::start(uint32_t sRate, uint8_t numCh)
{
    stop();

    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    QAudioFormat format;
    format.setSampleRate(sRate);
    format.setChannelCount(numCh);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);

    const QAudioDevice &defaultDeviceInfo = devices->defaultAudioOutput();
    if (!defaultDeviceInfo.isFormatSupported(format))
    {
        qWarning() << "Default format not supported - trying to use preferred";
        format = defaultDeviceInfo.preferredFormat();
    }
    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    audioOutput = new QAudioSink(defaultDeviceInfo, format, this);

    // set buffer size to AUDIO_FIFO_MS/2 ms
    audioOutput->setBufferSize(AUDIO_FIFO_CHUNK_MS*2 * sRate/1000 * numCh * sizeof(int16_t));
    connect(audioOutput, &QAudioSink::stateChanged, this, &AudioOutput::handleStateChanged);

    // start IO device
    ioDevice->setFormat(format);
#if 0
    ioDevice->open(QIODevice::ReadOnly);
    initTimer();
#else
    ioDevice->start();
    audioOutput->start(ioDevice);
#endif


#if AUDIOOUTPUT_DBG_TIMER
    // DBG counter for buffer monitoring
    if (nullptr == m_dbgTimer)
    {
        m_dbgTimer = new QTimer(this);
        m_dbgTimer->setInterval(1000);
        m_minCount = INT64_MAX;
        m_maxCount = INT64_MIN;
        connect(m_dbgTimer, &QTimer::timeout, this, &AudioOutput::bufferMonitor);
    }
    m_dbgTimer->start();
#endif
}

#if AUDIOOUTPUT_DBG_TIMER
void AudioOutput::bufferMonitor()
{
    int64_t count = ioDevice->bytesAvailable();

    if (count > m_maxCount) m_maxCount = count;
    if (count < m_minCount) m_minCount = count;
    m_sum -= m_dbgBuf[m_cntr];
    m_dbgBuf[m_cntr++] = count;
    m_cntr = m_cntr % AUDIOOUTPUT_DBG_AVRG_SIZE;
    m_sum+= count;
    float avrgMs = m_sum / (sizeof(int16_t)*m_numChannels * AUDIOOUTPUT_DBG_AVRG_SIZE * m_sampleRate_kHz);

    qDebug("Buffer monitor [%lld - %lld] : %lld bytes => %lld samples => %lld [avrg %.2f] ms", m_minCount, m_maxCount,
           count, count >> 2, (count>>2)/m_sampleRate_kHz, avrgMs);
}
#endif

void AudioOutput::mute(bool on)
{
    if (on)
    {
        audioOutput->setVolume(0);
    }
    else
    {
        audioOutput->setVolume(1.0);
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
    delete m_dbgTimer;
    m_dbgTimer = nullptr;
#endif
}

void AudioOutput::handleStateChanged(QAudio::State newState)
{
    qDebug() << newState;
    switch (newState)
    {
        case QAudio::ActiveState:
            //destroyTimer();
            break;
        case QAudio::IdleState:
            // no more data
            break;

        case QAudio::StoppedState:
            // Stopped for other reasons
            break;

        default:
            // ... other cases as appropriate
            break;
    }
}

AudioIODevice::AudioIODevice(audioFifo_t *buffer, QObject *parent) : QIODevice(parent)
{
    m_inFifoPtr = buffer;
}

void AudioIODevice::start()
{
    m_playbackState = StateMuted;
    open(QIODevice::ReadOnly);
}

void AudioIODevice::stop()
{
    close();
}

int64_t AudioIODevice::readData(char *data, int64_t len)
{
    if (0 == len)
    {
        return len;
    }

    // read samples from input buffer
    m_inFifoPtr->mutex.lock();
    uint64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    //qDebug() << Q_FUNC_INFO << len << count << m_playbackState;

    uint64_t bytesToRead = len;
    uint32_t availableSamples = len / m_bytesPerFrame;

    if (StateMuted == m_playbackState)
    {   // muted
        // condition to unmute is enough samples
        if (count > 6*bytesToRead)
        {   // enough samples => reading data from input fifo
            Q_ASSERT((bytesToRead >= AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz * m_bytesPerFrame));

            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                memcpy((uint8_t*)data, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)data + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                m_inFifoPtr->tail = bytesToRead - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) data, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                m_inFifoPtr->tail += bytesToRead;
            }

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            // unmute request
            m_playbackState = StateDoUnmute;
        }
        else
        {   // not enough samples ==> inserting silence
            //qDebug("Muted: Inserting silence [%lu ms]", nBufferFrames/m_sampleRate_kHz);
            memset(data, 0, bytesToRead);

            // done
            return bytesToRead;
        }
    }
    else  // cannot be anything else than StateMuted or StatePlaying
    {   // playing

        Q_ASSERT(StatePlaying == m_playbackState);

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {   // not enough samples -> reading what we have and filling rest with zeros
            if (0 == count)
            {   // nothing to play
                qDebug("Hard mute [no samples available]");
                memset(data, 0, bytesToRead);
                m_playbackState = StateMuted;
                return bytesToRead;
            }

            // there are some samples available
            availableSamples = count/m_bytesPerFrame;

            Q_ASSERT(count == availableSamples*m_bytesPerFrame);

            // copy all available samples to output buffer
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < count)
            {
                memcpy((uint8_t*)data, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)data + bytesToEnd, m_inFifoPtr->buffer, (count - bytesToEnd));
                m_inFifoPtr->tail = count - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) data, m_inFifoPtr->buffer+m_inFifoPtr->tail, count);
                m_inFifoPtr->tail += count;
            }
            // set rest of the samples to be 0
            memset((uint8_t*)data+count, 0, bytesToRead-count);

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= count;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            m_playbackState = StateDoMute;  // mute
        }
        else
        {   // reading samples
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                memcpy((uint8_t*)data, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                memcpy((uint8_t*)data + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                m_inFifoPtr->tail = bytesToRead - bytesToEnd;
            }
            else
            {
                memcpy((uint8_t*) data, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                m_inFifoPtr->tail += bytesToRead;
            }

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            return bytesToRead;
        }
    }

    // at this point we have buffer that needs to be muted or unmuted
    // it is indicated by playbackState variable

    // unmute
    if (StateDoUnmute == m_playbackState)
    {   // unmute can be requested only when there is enough samples
        qDebug() << "Unmuting audio";
        uint32_t fadeSamples = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;
        float gInc = 1.0/fadeSamples;
        float gain = 0;

        int16_t * dataPtr = (int16_t *) data;
        for (uint32_t n = 0; n < fadeSamples; ++n)
        {
            for (int c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain+gInc;
        }

        m_playbackState = StatePlaying; // playing

        return bytesToRead;
    }

    // mute
    if (StateDoMute == m_playbackState)
    {   // mute can be requested when there is not enough samples or from HMI
        qDebug("Muting... [available %u samples]", availableSamples);

        uint32_t fadeSamples = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;
        if (availableSamples < fadeSamples)
        {
            fadeSamples = availableSamples;
        }

        float gDec = 1.0/fadeSamples;
        float gain = 1.0;

        int16_t * dataPtr = (int16_t *) ((uint8_t * ) data + (availableSamples - fadeSamples)*m_bytesPerFrame);
        for (uint32_t n = 0; n < fadeSamples; ++n)
        {
            for (int c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain-gDec;
        }

        m_playbackState = StateMuted; // muted

        return bytesToRead;
    }

    return bytesToRead;
}

int64_t AudioIODevice::writeData(const char *data, int64_t len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

int64_t AudioIODevice::bytesAvailable() const
{
    m_inFifoPtr->mutex.lock();
    int64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    //qDebug() << Q_FUNC_INFO << count;

    return count; // + QIODevice::bytesAvailable();
}

void AudioIODevice::setFormat(const QAudioFormat & format)
{
    m_sampleRate_kHz = format.sampleRate() / 1000;
    m_numChannels = format.channelCount();
    m_bytesPerFrame = m_numChannels * sizeof(int16_t);
}

#endif

#endif // def AUDIOOUTPUT_USE_PORTAUDIO
