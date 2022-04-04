#include <QDebug>
#include <QThread>
#include <QAudioSink>
#include <QAudioDevice>

#include "audiooutput.h"

audioFifo_t audioBuffer;

#ifdef AUDIOOUTPUT_USE_PORTAUDIO

int portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                 const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);

AudioOutput::AudioOutput(audioFifo_t * buffer, QObject *parent) : QObject(parent)
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
    m_bufferFrames = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;  // 120 ms (FIFO size should be integer multiple of this)

    // mute ramp is exponential
    // value are precalculated to save MIPS in runtime
    // unmute ramp is then calculated as 2.0 - m_muteFactor in runtime
    // m_muteFactor is calculated for change from 0dB to AUDIOOUTPUT_FADE_MIN_DB in AUDIOOUTPUT_FADE_TIME_MS
    m_muteFactor = powf(10, AUDIOOUTPUT_FADE_MIN_DB/(20.0*AUDIOOUTPUT_FADE_TIME_MS*m_sampleRate_kHz));

    /* Open an audio I/O stream. */
    PaError err = Pa_OpenDefaultStream( &m_outStream,
                                        0,              /* no input channels */
                                        numCh,          /* stereo output */
                                        paInt16,        /* 16 bit floating point output */
                                        sRate,
                                        m_bufferFrames, /* frames per buffer, i.e. the number
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

    m_playbackState = AudioOutputPlaybackState::StateMuted;

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
        if (m_muteFlag != true)
        {
            m_stopFlag = true;
            QTimer::singleShot(3*AUDIOOUTPUT_FADE_TIME_MS, this, &AudioOutput::doStop);
        }
        else
        {   // already muted - stop immediately
            doStop();
        }
    }

#if AUDIOOUTPUT_DBG_TIMER
    delete m_dbgTimer;
    m_dbgTimer = nullptr;
#endif
}

void AudioOutput::doStop()
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
        m_stopFlag = false;
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
    m_muteFlag = on;
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
    if (statusFlags)
    {
        qDebug() << "Port Audio statusFlags =" << statusFlags;
    }

    return static_cast<AudioOutput*>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames);
#endif
}

int AudioOutput::portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames)
{
    // read samples from input buffer
    m_inFifoPtr->mutex.lock();
    uint64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    bool mute = m_muteFlag | m_stopFlag;

    uint64_t bytesToRead = m_bytesPerFrame * nBufferFrames;
    uint32_t availableSamples = nBufferFrames;

    if (AudioOutputPlaybackState::StateMuted == m_playbackState)
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

                if (m_stopFlag)
                {
                    return paComplete;
                }

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
            m_playbackState = AudioOutputPlaybackState::StateDoUnmute;
        }
        else
        {   // not enough samples ==> inserting silence
            //qDebug("Muted: Inserting silence [%lu ms]", nBufferFrames/m_sampleRate_kHz);                       
            memset(outputBuffer, 0, bytesToRead);

            if (m_stopFlag)
            {
                return paComplete;
            }

            // done
            return paContinue;
        }
    }
    //if (StatePlaying == playbackState)
    else  // cannot be anything else than StateMuted or StatePlaying
    {   // playing

        Q_ASSERT(AudioOutputPlaybackState::StatePlaying == m_playbackState);

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {   // not enough samples -> reading what we have and filling rest with zeros
            // minimum mute time is 1ms (m_sampleRate_kHz samples) , if less then hard mute
            if (m_sampleRate_kHz*m_bytesPerFrame > count)
            {   // nothing to play (cannot apply mute ramp)
                qDebug("Hard mute [no samples available]");
                memset(outputBuffer, 0, bytesToRead);
                m_playbackState = AudioOutputPlaybackState::StateMuted;
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

            m_playbackState = AudioOutputPlaybackState::StateDoMute;  // mute
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
                m_playbackState = AudioOutputPlaybackState::StateDoMute;
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
    if (AudioOutputPlaybackState::StateDoUnmute == m_playbackState)
    {   // unmute can be requested only when there is enough samples
        qDebug() << "Unmuting audio";

        float coe = 2.0 - m_muteFactor;
        float gain = AUDIOOUTPUT_FADE_MIN_LIN;
        int16_t * dataPtr = (int16_t *) outputBuffer;
        for (uint_fast32_t n = 0; n < availableSamples; ++n)
        {
            for (uint_fast8_t c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain * coe;  // after by purpose
        }

        m_playbackState = AudioOutputPlaybackState::StatePlaying; // playing

        // done
        return paContinue;
    }

    // mute
    if (AudioOutputPlaybackState::StateDoMute == m_playbackState)
    {   // mute can be requested when there is not enough samples or from HMI
        qDebug("Muting... [available %u samples]", availableSamples);
        float coe = m_muteFactor;
        if (availableSamples < AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz)
        {   // less samples than expected available => need to calculate new coef
            coe = powf(10, AUDIOOUTPUT_FADE_MIN_DB/(20.0*availableSamples));
        }

        float gain = 1.0;

        int16_t * dataPtr = (int16_t *) outputBuffer;
        for (uint_fast32_t n = 0; n < availableSamples; ++n)
        {
            gain = gain * coe;  // before by purpose
            for (uint_fast8_t c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr++;
            }
        }

        m_playbackState = AudioOutputPlaybackState::StateMuted; // muted

        if (m_stopFlag)
        {
            //qDebug() << Q_FUNC_INFO << "paCompete" << fadeSamples;
            return paComplete;
        }
    }

    return paContinue;
}


#else // !def AUDIOOUTPUT_USE_PORTAUDIO

AudioOutput::AudioOutput(audioFifo_t * buffer, QObject *parent) : QObject(parent)
{
    devices = new  QMediaDevices(this);    
    audioSink = nullptr;

    // initialize mute ramp
    // mute ramp is cos^2 that changes from 1 to 0 during AUDIOOUTPUT_FADE_TIME_MS
    // value are precalculated to save MIPS in runtime
    // unmute ramp is then calculated as 1.0 - muteRamp in runtime
    float coe = M_PI/(2.0 * AUDIOOUTPUT_FADE_TIME_MS);
    for (int n = 0; n < AUDIOOUTPUT_FADE_TIME_MS; ++n)
    {
        float g = cosf(n*coe);
        m_muteRamp.push_back(g*g);
    }
    ioDevice = new AudioIODevice(buffer, m_muteRamp, this);


#if AUDIOOUTPUT_DBG_TIMER
    m_dbgTimer = nullptr;
#endif    
}

AudioOutput::~AudioOutput()
{
    if (nullptr != audioSink)
    {
        audioSink->stop();
        delete audioSink;
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
    //stop();

    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    QAudioFormat format;
    format.setSampleRate(sRate);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelCount(numCh);
    if (numCh > 1)
    {
        format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    }
    else
    {
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    }

    const QAudioDevice &defaultDeviceInfo = devices->defaultAudioOutput();
    if (!defaultDeviceInfo.isFormatSupported(format))
    {
        qWarning() << "Default format not supported - trying to use preferred";
        format = defaultDeviceInfo.preferredFormat();
    }

    if (nullptr != audioSink)
    {
        // audioSink exists
        if (format != audioSink->format())
        {   // need to create new audio sink
            delete audioSink;
            audioSink = nullptr;
        }
        else
        { /* do nothing - format is the same */ }
    }

    if (nullptr == audioSink)
    {
        // create audio sink
        audioSink = new QAudioSink(defaultDeviceInfo, format, this);

        // set buffer size to 2* AUDIO_FIFO_CHUNK_MS ms
        audioSink->setBufferSize(2 * AUDIO_FIFO_CHUNK_MS * sRate/1000 * numCh * sizeof(int16_t));

        connect(audioSink, &QAudioSink::stateChanged, this, &AudioOutput::handleStateChanged);
    }
    else
    {  /* do nothing */ }

    // start muted if mute was requested
    audioSink->setVolume(!m_muteFlag);

    // start IO device
    ioDevice->setFormat(format);
    ioDevice->start();
    audioSink->start(ioDevice);

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
    m_muteFlag = on;
    if (on)
    {
        muteStep(0);    // this starts mute ramp
    }
    else
    {
        unmuteStep(0);  // this starts unmute ramp
    }
}

void AudioOutput::muteStep(int n)
{
    if (nullptr != audioSink)
    {
        //qDebug() << Q_FUNC_INFO << n;
        if (n < AUDIOOUTPUT_FADE_TIME_MS)
        {
            audioSink->setVolume(m_muteRamp.at(n));
            QTimer::singleShot(1, this, [this, n](){ muteStep(n+1); } );
            return;
        }

        if (m_stopFlag)
        {
            //doStop();
            // this is to avoid audio artifacts
            QTimer::singleShot(100, this, &AudioOutput::doStop );
        }
    }
}

void AudioOutput::unmuteStep(int n)
{
    if (nullptr != audioSink)
    {
        if (n < AUDIOOUTPUT_FADE_TIME_MS)
        {
            audioSink->setVolume(1-m_muteRamp.at(n));
            QTimer::singleShot(1, this, [this, n](){ unmuteStep(n+1); } );
            return;
        }
    }
}

void AudioOutput::stop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != audioSink)
    {
        if (m_muteFlag != true)
        {
            m_stopFlag = true;
            muteStep(0);  // this starts mute ramp
            return;
        }
        else
        { /* were are already muted - doStop now */ }
    }

    doStop();

#if AUDIOOUTPUT_DBG_TIMER
    delete m_dbgTimer;
    m_dbgTimer = nullptr;
#endif
}

void AudioOutput::doStop()
{
    qDebug() << Q_FUNC_INFO;
    if (nullptr != audioSink)
    {
        m_stopFlag = false;
        audioSink->stop();
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


AudioIODevice::AudioIODevice(audioFifo_t *buffer, const std::vector<float> &ramp, QObject *parent) :
    QIODevice(parent),
    m_muteRamp(ramp)
{
    m_inFifoPtr = buffer;
}

void AudioIODevice::start()
{
    m_playbackState = AudioOutputPlaybackState::StateMuted;
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

    //qDebug() << Q_FUNC_INFO << len << count << static_cast<int>(m_playbackState);

    //uint64_t bytesToRead = len;
    uint64_t bytesToRead = qMin(uint64_t(len), AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz * m_bytesPerFrame);
    uint32_t availableSamples = len / m_bytesPerFrame;

    if (AudioOutputPlaybackState::StateMuted == m_playbackState)
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
            m_playbackState = AudioOutputPlaybackState::StateDoUnmute;
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

        Q_ASSERT(AudioOutputPlaybackState::StatePlaying == m_playbackState);

        if (count < 1.5*bytesToRead)
        {
            qDebug("Bytes available %llu", count);
        }

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {   // not enough samples -> reading what we have and filling rest with zeros
            if (AUDIOOUTPUT_FADE_TIME_MS*m_bytesPerFrame > count)
            {   // nothing to play
                qDebug("Hard mute [no samples available]");
                memset(data, 0, bytesToRead);
                m_playbackState = AudioOutputPlaybackState::StateMuted;
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

            m_playbackState = AudioOutputPlaybackState::StateDoMute;  // mute
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
    if (AudioOutputPlaybackState::StateDoUnmute == m_playbackState)
    {   // unmute can be requested only when there is enough samples
        qDebug() << "Unmuting audio";
        int16_t * dataPtr = (int16_t *) data;
        for (uint_fast32_t n = 0; n < AUDIOOUTPUT_FADE_TIME_MS; ++n)
        {
            float gain = 1.0 - m_muteRamp.at(n);
            for (uint_fast8_t s = 0; s < m_sampleRate_kHz; ++s)
            {  // 1 ms samples
                for (uint_fast8_t c = 0; c < m_numChannels; ++c)
                {
                    *dataPtr = int16_t(qRound(gain * *dataPtr));
                    dataPtr++;
                }
            }
        }

        m_playbackState = AudioOutputPlaybackState::StatePlaying; // playing

        return bytesToRead;
    }

    // mute
    if (AudioOutputPlaybackState::StateDoMute == m_playbackState)
    {   // mute can be requested when there is not enough samples or from HMI
        qDebug("Muting... [available %u samples]", availableSamples);

        uint32_t fadeSamples = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;
        uint_fast8_t muteChunk = m_sampleRate_kHz;   // 1 ms
        if (availableSamples < fadeSamples)
        {   // number of samples that are available is not enough to apply normal mute ramp
            // buffer is [(availableSamples) 0 0 0 0 0 0]
            // but we know that availableSamples >= AUDIOOUTPUT_FADE_TIME_MS
            muteChunk = availableSamples / AUDIOOUTPUT_FADE_TIME_MS;
            fadeSamples = muteChunk * AUDIOOUTPUT_FADE_TIME_MS;      // this will make mute shorter than availableSamples in general
        }

        int16_t * dataPtr = (int16_t *) ((uint8_t * ) data + (availableSamples - fadeSamples)*m_bytesPerFrame);
        for (uint_fast32_t n = 0; n < AUDIOOUTPUT_FADE_TIME_MS; ++n)
        {
            float gain = m_muteRamp.at(n);
            for (uint_fast8_t s = 0; s < muteChunk; ++s)
            {  // 1 ms samples
                for (uint_fast8_t c = 0; c < m_numChannels; ++c)
                {
                    *dataPtr = int16_t(qRound(gain * *dataPtr));
                    dataPtr++;
                }
            }
        }

        m_playbackState = AudioOutputPlaybackState::StateMuted; // muted
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

#endif // def AUDIOOUTPUT_USE_PORTAUDIO
