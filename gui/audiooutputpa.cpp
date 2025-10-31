/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "audiooutputpa.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QDebug>
#include <QLoggingCategory>
#include <QThread>

Q_DECLARE_LOGGING_CATEGORY(audioOutput)

AudioOutputPa::AudioOutputPa(QObject *parent) : AudioOutput(parent)
{
    m_currentFifoPtr = nullptr;
    m_outStream = nullptr;
    m_numChannels = m_sampleRate_kHz = 0;
    m_linearVolume = 1.0;

    PaError err = Pa_Initialize();
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText(err));
    }

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    m_rawOut = fopen("audio.raw", "wb");
    if (!m_rawOut)
    {
        qCWarning(audioOutput, "Unable to open file: audio.raw");
    }
#endif

    connect(this, &AudioOutputPa::streamFinished, this, &AudioOutputPa::onStreamFinished, Qt::QueuedConnection);
}

AudioOutputPa::~AudioOutputPa()
{
    if (nullptr != m_outStream)
    {
        if (Pa_IsStreamActive(m_outStream))
        {
            // Pa_AbortStream(m_outStream);
            Pa_StopStream(m_outStream);
        }
        Pa_CloseStream(m_outStream);
    }

    PaError err = Pa_Terminate();
    if (paNoError != err)
    {
        qCCritical(audioOutput, "PortAudio Pa_Terminate() error: %s", Pa_GetErrorText(err));
    }

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    if (m_rawOut)
    {
        fclose(m_rawOut);
    }
#endif
}

void AudioOutputPa::start(audioFifo_t *buffer)
{
    int deviceIdx = getCurrentDeviceIdx();
    if (deviceIdx < 0)
    {   // no audio device was found
        m_currentFifoPtr = buffer;
        startDummyOutput();
        return;
    }

    uint32_t sRate = buffer->sampleRate;
    uint8_t numCh = buffer->numChannels;

    bool isNewStreamParams = (m_sampleRate_kHz != sRate / 1000) || (m_numChannels != numCh);
    if (isNewStreamParams || m_reloadDevice)
    {
        m_reloadDevice = false;
        if (nullptr != m_outStream)
        {
            if (!Pa_IsStreamStopped(m_outStream))
            {
                Pa_StopStream(m_outStream);
            }

            Pa_CloseStream(m_outStream);
            m_outStream = nullptr;
        }

        m_sampleRate_kHz = sRate / 1000;
        m_numChannels = numCh;

        m_bytesPerFrame = numCh * sizeof(int16_t);
        m_bufferFrames = AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz;  // 120 ms (FIFO size should be integer multiple of this)

        // mute ramp is exponential
        // value are precalculated to save MIPS in runtime
        // unmute ramp is then calculated as 2.0 - m_muteFactor in runtime
        // m_muteFactor is calculated for change from 0dB to AUDIOOUTPUT_FADE_MIN_DB in AUDIOOUTPUT_FADE_TIME_MS
        m_muteFactor = powf(10, AUDIOOUTPUT_FADE_MIN_DB / (20.0 * AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz));

#ifdef Q_OS_LINUX
        /* Open an audio I/O stream. */
        PaError err = Pa_OpenDefaultStream(&m_outStream, 0,       /* no input channels */
                                           numCh,                 /* stereo output */
                                           paInt16,               /* 16 bit floating point output */
                                           sRate, m_bufferFrames, /* frames per buffer, i.e. the number
                                                                  of sample frames that PortAudio will
                                                                  request from the callback. Many apps
                                                                  may want to use
                                                                  paFramesPerBufferUnspecified, which
                                                                  tells PortAudio to pick the best,
                                                                  possibly changing, buffer size.*/
                                           portAudioCb,           /* callback */
                                           (void *)this);
#else
        PaStreamParameters outputParameters;
        outputParameters.device = deviceIdx;
        outputParameters.channelCount = numCh;
        outputParameters.sampleFormat = paInt16;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        /* Open an audio I/O stream. */
        PaError err = Pa_OpenStream(&m_outStream,
                                    nullptr,                                  // no input
                                    &outputParameters, sRate, m_bufferFrames, /* frames per buffer, i.e. the number
                                                                                     of sample frames that PortAudio will
                                                                                     request from the callback. Many apps
                                                                                     may want to use
                                                                                     paFramesPerBufferUnspecified, which
                                                                                     tells PortAudio to pick the best,
                                                                                     possibly changing, buffer size.*/
                                    paNoFlag, portAudioCb,                    /* callback */
                                    (void *)this);

#endif
        if (paNoError != err)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText(err));
        }

        err = Pa_SetStreamFinishedCallback(m_outStream, portAudioStreamFinishedCb);
        if (paNoError != err)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText(err));
        }
    }
    else
    {  // stream parameters are the same - just starting
        // Pa_StopStream() is required event that streaming was stopped by paComplete return value from callback
        if (!Pa_IsStreamStopped(m_outStream))
        {
            Pa_StopStream(m_outStream);
        }
    }

    m_currentFifoPtr = buffer;
    m_playbackState = AudioOutputPlaybackState::Muted;
    m_cbRequest &= ~(Request::Stop | Request::Restart);  // reset stop and restart bits

    PaError err = Pa_StartStream(m_outStream);
    if (paNoError != err)
    {
        qCCritical(audioOutput) << "Pa_StartStream error:" << Pa_GetErrorText(err);
    }
}

void AudioOutputPa::restart(audioFifo_t *buffer)
{
    if (nullptr != m_outStream)
    {
        m_restartFifoPtr = buffer;
        m_cbRequest |= Request::Restart;  // set restart bit
    }
}

void AudioOutputPa::stop()
{
    if (m_dummyOutputTimer)
    {  // dummy audio output
        stopDummyOutput();
        return;
    }

    if (nullptr != m_outStream)
    {
        m_cbRequest |= Request::Stop;  // set stop bit
    }
}

void AudioOutputPa::mute(bool on)
{
    if (on)
    {  // set mute bit
        m_cbRequest |= Request::Mute;
    }
    else
    {  // reset mute bit
        m_cbRequest &= ~Request::Mute;
    }
}

void AudioOutputPa::setVolume(int value)
{
    m_linearVolume = QAudio::convertVolume(value / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
}

int AudioOutputPa::portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames, const PaStreamCallbackTimeInfo *timeInfo,
                               PaStreamCallbackFlags statusFlags, void *ctx)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(timeInfo);

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    int ret = static_cast<AudioOutputPa *>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames);
    if (static_cast<AudioOutputPa *>(ctx)->m_rawOut)
    {
        fwrite(outputBuffer, sizeof(int16_t), nBufferFrames * static_cast<AudioOutputPa *>(ctx)->m_numChannels,
               static_cast<AudioOutputPa *>(ctx)->m_rawOut);
    }
    return ret;
#else
    if (statusFlags)
    {
        qCWarning(audioOutput) << "Port Audio statusFlags =" << statusFlags;
    }

    return static_cast<AudioOutputPa *>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames);
#endif
}

int AudioOutputPa::portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames)
{
    // qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    // read samples from input buffer
    m_currentFifoPtr->mutex.lock();
    uint64_t count = m_currentFifoPtr->count;
    m_currentFifoPtr->mutex.unlock();

    uint64_t bytesToRead = m_bytesPerFrame * nBufferFrames;
    uint32_t availableSamples = nBufferFrames;

    // is any bit is set then mute is requested (mute | stop | restart)
    unsigned int request = m_cbRequest;

    if (AudioOutputPlaybackState::Muted == m_playbackState)
    {  // muted
        // condition to unmute is enough samples && !muteFlag
        if (count > 7 * bytesToRead)
        {  // enough samples => reading data from input fifo
            if (Request::None != request)
            {  // staying muted -> setting output buffer to 0
                memset(outputBuffer, 0, bytesToRead);

                // shifting buffer pointers
                m_currentFifoPtr->tail = (m_currentFifoPtr->tail + bytesToRead) % AUDIO_FIFO_SIZE;
                m_currentFifoPtr->mutex.lock();
                m_currentFifoPtr->count -= bytesToRead;
                m_currentFifoPtr->countChanged.wakeAll();
                m_currentFifoPtr->mutex.unlock();

                if (request & (Request::Stop | Request::Restart))
                {  // stop or restart requested ==> finish playback
                    return paComplete;
                }

                // done
                return paContinue;
            }
            else
            { /* no request */
            }

            // at this point we have enough sample to unmute and there is no request => preparing data
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_currentFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t *)outputBuffer + bytesToEnd, m_currentFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    m_currentFifoPtr->tail = bytesToRead - bytesToEnd;
                }
                else
                {
                    // memcpy((uint8_t*)outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, bytesToEnd);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    // memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    inDataPtr = (int16_t *)m_currentFifoPtr->buffer;
                    outDataPtr = (int16_t *)((uint8_t *)outputBuffer + bytesToEnd);
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((bytesToRead - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == (bytesToRead - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_currentFifoPtr->tail = bytesToRead - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, bytesToRead);
                    m_currentFifoPtr->tail += bytesToRead;
                }
                else
                {
                    // memcpy((uint8_t*) outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, bytesToRead);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToRead >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == bytesToRead);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    m_currentFifoPtr->tail += bytesToRead;
                }
            }
            m_currentFifoPtr->mutex.lock();
            m_currentFifoPtr->count -= bytesToRead;
            m_currentFifoPtr->countChanged.wakeAll();
            m_currentFifoPtr->mutex.unlock();

            // unmute request
            request = Request::None;
        }
        else
        {  // not enough samples ==> inserting silence
            qCDebug(audioOutput, "Muted: Inserting silence [%lu ms]", nBufferFrames / m_sampleRate_kHz);
            memset(outputBuffer, 0, bytesToRead);

            if (request & (Request::Stop | Request::Restart))
            {  // stop or restart requested ==> finish playback
                return paComplete;
            }

            // done
            return paContinue;
        }
    }
    else
    {  // (AudioOutputPlaybackState::Muted != m_playbackState)
        // cannot be anything else than Muted or Playing ==> playing

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {  // not enough samples -> reading what we have and filling rest with zeros
            // minimum mute time is 1ms (m_sampleRate_kHz samples) , if less then hard mute
            if (m_sampleRate_kHz * m_bytesPerFrame > count)
            {  // nothing to play (cannot apply mute ramp)
                qCInfo(audioOutput, "Hard mute [no samples available]");
                memset(outputBuffer, 0, bytesToRead);
                m_playbackState = AudioOutputPlaybackState::Muted;
                return paContinue;
            }

            // there are some samples available
            availableSamples = count / m_bytesPerFrame;

            Q_ASSERT(count == availableSamples * m_bytesPerFrame);

            // copy all available samples to output buffer
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_currentFifoPtr->tail;
            if (bytesToEnd < count)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t *)outputBuffer + bytesToEnd, m_currentFifoPtr->buffer, (count - bytesToEnd));
                    m_currentFifoPtr->tail = count - bytesToEnd;
                }
                else
                {
                    // memcpy((uint8_t*)outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, bytesToEnd);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    // memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (count - bytesToEnd));
                    inDataPtr = (int16_t *)m_currentFifoPtr->buffer;
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((count - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == (count - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_currentFifoPtr->tail = count - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, count);
                    m_currentFifoPtr->tail += count;
                }
                else
                {
                    // memcpy((uint8_t*) outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, count);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (count >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == count);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_currentFifoPtr->tail += count;
                }
            }
            // set rest of the samples to be 0
            memset((uint8_t *)outputBuffer + count, 0, bytesToRead - count);

            m_currentFifoPtr->mutex.lock();
            m_currentFifoPtr->count -= count;
            m_currentFifoPtr->countChanged.wakeAll();
            m_currentFifoPtr->mutex.unlock();

            // request to apply mute ramp
            request = Request::Mute;
        }
        else
        {  // enough sample available -> reading samples
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_currentFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t *)outputBuffer + bytesToEnd, m_currentFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    m_currentFifoPtr->tail = bytesToRead - bytesToEnd;
                }
                else
                {
                    // memcpy((uint8_t*)outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, bytesToEnd);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    // memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    inDataPtr = (int16_t *)m_currentFifoPtr->buffer;
                    outDataPtr = (int16_t *)((uint8_t *)outputBuffer + bytesToEnd);
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((bytesToRead - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == (bytesToRead - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_currentFifoPtr->tail = bytesToRead - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {
                    memcpy((uint8_t *)outputBuffer, m_currentFifoPtr->buffer + m_currentFifoPtr->tail, bytesToRead);
                    m_currentFifoPtr->tail += bytesToRead;
                }
                else
                {
                    // memcpy((uint8_t*) outputBuffer, m_currentFifoPtr->buffer+m_currentFifoPtr->tail, bytesToRead);
                    int16_t *inDataPtr = (int16_t *)(m_currentFifoPtr->buffer + m_currentFifoPtr->tail);
                    int16_t *outDataPtr = (int16_t *)outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToRead >> 1);

                    Q_ASSERT(numSamples * sizeof(int16_t) == bytesToRead);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_currentFifoPtr->tail += bytesToRead;
                }
            }
            m_currentFifoPtr->mutex.lock();
            m_currentFifoPtr->count -= bytesToRead;
            m_currentFifoPtr->countChanged.wakeAll();
            m_currentFifoPtr->mutex.unlock();

            //            if ((Request::Restart & request) && (count >= 4*bytesToRead))
            //            {   // removing restart flag ==> play all samples we have
            //                request &= ~Request::Restart;
            //            }

            if (Request::None == request)
            {  // done
                return paContinue;
            }
        }
    }

    // at this point we have buffer that needs to be muted or unmuted
    // it is indicated by request variable

    // unmute
    if (Request::None == request)
    {  // unmute can be requested only when there is enough samples
        qCInfo(audioOutput) << "Unmuting audio";

        float coe = 2.0 - m_muteFactor;
        float gain = AUDIOOUTPUT_FADE_MIN_LIN;
        int16_t *dataPtr = (int16_t *)outputBuffer;
        for (uint_fast32_t n = 0; n < availableSamples; ++n)
        {
            for (uint_fast8_t c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(std::lroundf(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain * coe;  // after by purpose
        }

        m_playbackState = AudioOutputPlaybackState::Playing;  // playing

        // done
        return paContinue;
    }
    else
    {  // there is so request => muting
        // mute can be requested when there is not enough samples or from HMI
        qCInfo(audioOutput, "Muting... [available %u samples]", availableSamples);
        float coe = m_muteFactor;
        if (availableSamples < AUDIOOUTPUT_FADE_TIME_MS * m_sampleRate_kHz)
        {  // less samples than expected available => need to calculate new coef
            coe = powf(10, AUDIOOUTPUT_FADE_MIN_DB / (20.0 * availableSamples));
        }

        float gain = 1.0;

        int16_t *dataPtr = (int16_t *)outputBuffer;
        for (uint_fast32_t n = 0; n < availableSamples; ++n)
        {
            gain = gain * coe;  // before by purpose
            for (uint_fast8_t c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(std::lroundf(gain * *dataPtr));
                dataPtr++;
            }
        }

        m_playbackState = AudioOutputPlaybackState::Muted;  // muted

        if (request & (Request::Stop | Request::Restart))
        {  // stop or restart requested ==> finish playback
            return paComplete;
        }
    }

    return paContinue;
}

void AudioOutputPa::portAudioStreamFinishedCb(void *ctx)
{
    // qDebug() << Q_FUNC_INFO << QThread::currentThreadId();
    static_cast<AudioOutputPa *>(ctx)->portAudioStreamFinishedPrivateCb();
}

void AudioOutputPa::onStreamFinished()
{
    if (m_cbRequest & Request::Restart)
    {  // restart was requested (flag is cleared in start routine)
        emit audioOutputRestart();
        start(m_restartFifoPtr);
    }
    else if (!(m_cbRequest & Request::Stop))
    {  // finished and not stop request ==> problem with audio device or user want to chnage device
        PaError err;
        err = Pa_IsStreamActive(m_outStream);
        if (err < 0)
        {
            qCCritical(audioOutput) << "Pa_IsStreamActive error:" << Pa_GetErrorText(err);
        }
        else if (err != 0)
        {
            err = Pa_AbortStream(m_outStream);
            if (paNoError != err)
            {
                qCCritical(audioOutput) << "Pa_AbortStream error:" << Pa_GetErrorText(err);
            }

            qCWarning(audioOutput, "Aborting stream");
            return;
        }

        qCWarning(audioOutput, "Audio stream finished");

        err = Pa_CloseStream(m_outStream);
        if (paNoError != err)
        {
            qCCritical(audioOutput) << "Pa_CloseStream error:" << Pa_GetErrorText(err);
        }
        m_outStream = nullptr;

        err = Pa_Terminate();
        if (paNoError != err)
        {
            qCCritical(audioOutput) << "Pa_Terminate error:" << Pa_GetErrorText(err);
        }

        err = Pa_Initialize();
        if (paNoError != err)
        {
            qCCritical(audioOutput) << "Pa_Initialize error:" << Pa_GetErrorText(err);
        }
        m_reloadDevice = true;
        start(m_currentFifoPtr);
    }
    else
    { /* do nothing */
    }
}

void AudioOutputPa::setAudioDevice(const QByteArray &deviceId)
{
    m_useDefaultDevice = deviceId.isEmpty();
    if (deviceId.isEmpty())
    {  // default audio device to be used
        emit audioDeviceChanged({});
        if (m_currentAudioDevice.id() == m_devices->defaultAudioOutput().id())
        {  // do nothing
            return;
        }
    }
    else if (deviceId == m_currentAudioDevice.id())
    {  // do nothing
        return;
    }

    m_currentAudioDevice = m_devices->defaultAudioOutput();
    if (m_useDefaultDevice == false)
    {
        QList<QAudioDevice> list = getAudioDevices();
        bool found = false;
        for (const auto &dev : list)
        {
            if (dev.id() == deviceId)
            {
                m_currentAudioDevice = dev;
                found = true;
                emit audioDeviceChanged(m_currentAudioDevice.id());
                break;
            }
        }
        if (!found)
        {
            m_useDefaultDevice = true;
            emit audioDeviceChanged({});  // default device
        }
    }
    else
    {
        emit audioDeviceChanged({});  // default device
    }

    // change output device to newly selected
    m_reloadDevice = true;

    if (nullptr != m_outStream)
    {
        PaError err = Pa_AbortStream(m_outStream);
        if (paNoError != err)
        {
            qCCritical(audioOutput) << "Pa_AbortStream error:" << Pa_GetErrorText(err);
        }
    }
}

PaDeviceIndex AudioOutputPa::getCurrentDeviceIdx()
{
    PaDeviceIndex idx = Pa_GetDefaultOutputDevice();
    if (idx < 0)
    {
        qCCritical(audioOutput) << "Pa_GetDefaultOutputDevice error:" << Pa_GetErrorText(idx);
        return -1;
    }

    for (int d = 0; d < Pa_GetDeviceCount(); ++d)
    {
        const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(d);
        if ((devInfo->maxOutputChannels > 1) &&
            (m_currentAudioDevice.description().startsWith(QString(devInfo->name).trimmed(), Qt::CaseInsensitive)))
        {
            qCInfo(audioOutput) << "Audio device" << m_currentAudioDevice.description() << (idx == d ? "[default]" : "");
            return d;
        }
    }

    qCWarning(audioOutput) << "Audio device" << m_currentAudioDevice.description() << "not found, trying default";
    return idx;
}
