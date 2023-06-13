/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QAudioSink>
#include <QAudioDevice>

#include "audiooutputpa.h"

Q_DECLARE_LOGGING_CATEGORY(audioOutput)

AudioOutputPa::AudioOutputPa( QObject *parent) : AudioOutput(parent)
{
    m_inFifoPtr = nullptr;
    m_outStream = nullptr;
    m_numChannels = m_sampleRate_kHz = 0;
    m_linearVolume = 1.0;

    PaError err = Pa_Initialize();
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
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
            //Pa_AbortStream(m_outStream);
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
    uint32_t sRate = buffer->sampleRate;
    uint8_t numCh = buffer->numChannels;

    bool isNewStreamParams = (m_sampleRate_kHz != sRate/1000) || (m_numChannels != numCh);
    if (isNewStreamParams)
    {
        if (nullptr != m_outStream)
        {
            if (!Pa_IsStreamStopped(m_outStream))
            {
                Pa_StopStream(m_outStream);
            }

            Pa_CloseStream(m_outStream);
            m_outStream = nullptr;
        }

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

        err = Pa_SetStreamFinishedCallback(m_outStream, portAudioStreamFinishedCb);
        if (paNoError != err)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
        }
    }
    else
    {   // stream parameters are the same - just starting
        // Pa_StopStream() is required event that streaming was stopped by paComplete return value from callback
        if (!Pa_IsStreamStopped(m_outStream))
        {
            Pa_StopStream(m_outStream);
        }
    }

    m_inFifoPtr = buffer;
    m_playbackState = AudioOutputPlaybackState::Muted;
    m_cbRequest &= ~(Request::Stop | Request::Restart);  // reset stop and restart bits

    PaError err = Pa_StartStream(m_outStream);
    if (paNoError != err)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
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
    if (nullptr != m_outStream)
    {
        m_cbRequest |= Request::Stop;     // set stop bit
    }
}

void AudioOutputPa::mute(bool on)
{
    if (on)
    {   // set mute bit
        m_cbRequest |= Request::Mute;
    }
    else
    {   // reset mute bit
        m_cbRequest &= ~Request::Mute;
    }
}

void AudioOutputPa::setVolume(int value)
{
    m_linearVolume = QAudio::convertVolume(value / qreal(100),
                                           QAudio::LogarithmicVolumeScale,
                                           QAudio::LinearVolumeScale);
}

int AudioOutputPa::portAudioCb( const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                             const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx)
{
    Q_UNUSED(inputBuffer);
    Q_UNUSED(timeInfo);

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    int ret = static_cast<AudioOutput*>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames);
    if (static_cast<AudioOutput*>(ctx)->m_rawOut)
    {
        fwrite(outputBuffer, sizeof(int16_t), nBufferFrames * static_cast<AudioOutput*>(ctx)->m_numChannels, static_cast<AudioOutput*>(ctx)->m_rawOut);
    }
    return ret;
#else
    if (statusFlags)
    {
        qCWarning(audioOutput) << "Port Audio statusFlags =" << statusFlags;
    }

    return static_cast<AudioOutputPa*>(ctx)->portAudioCbPrivate(outputBuffer, nBufferFrames);
#endif
}

int AudioOutputPa::portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames)
{
    // read samples from input buffer
    m_inFifoPtr->mutex.lock();
    uint64_t count = m_inFifoPtr->count;
    m_inFifoPtr->mutex.unlock();

    uint64_t bytesToRead = m_bytesPerFrame * nBufferFrames;
    uint32_t availableSamples = nBufferFrames;

    // is any bit is set then mute is requested (mute | stop | restart)
    unsigned int request = m_cbRequest;

    if (AudioOutputPlaybackState::Muted == m_playbackState)
    {   // muted
        // condition to unmute is enough samples && !muteFlag
        if (count > 6*bytesToRead)
        {   // enough samples => reading data from input fifo
            if (Request::None != request)
            {   // staying muted -> setting output buffer to 0
                memset(outputBuffer, 0, bytesToRead);

                // shifting buffer pointers
                m_inFifoPtr->tail = (m_inFifoPtr->tail + bytesToRead) % AUDIO_FIFO_SIZE;
                m_inFifoPtr->mutex.lock();
                m_inFifoPtr->count -= bytesToRead;
                m_inFifoPtr->countChanged.wakeAll();
                m_inFifoPtr->mutex.unlock();

                if (request & (Request::Stop | Request::Restart))
                {   // stop or restart requested ==> finish playback
                    return paComplete;
                }

                // done
                return paContinue;
            }
            else { /* no request */ }

            // at this point we have enough sample to unmute and there is no request => preparing data
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {   // only copy here
                    memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    m_inFifoPtr->tail = bytesToRead - bytesToEnd;
                }
                else
                {
                    //memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    //memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    inDataPtr = (int16_t *) m_inFifoPtr->buffer;
                    outDataPtr = (int16_t *) ((uint8_t*)outputBuffer + bytesToEnd);
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((bytesToRead - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == (bytesToRead - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_inFifoPtr->tail = bytesToRead - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {  // only copy here
                    memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                    m_inFifoPtr->tail += bytesToRead;
                }
                else
                {
                    //memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToRead >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == bytesToRead);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    m_inFifoPtr->tail += bytesToRead;
                }
            }
            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            // unmute request
            request = Request::None;
        }
        else
        {   // not enough samples ==> inserting silence
            qCDebug(audioOutput, "Muted: Inserting silence [%lu ms]", nBufferFrames/m_sampleRate_kHz);
            memset(outputBuffer, 0, bytesToRead);

            if (request & (Request::Stop | Request::Restart))
            {   // stop or restart requested ==> finish playback
                return paComplete;
            }

            // done
            return paContinue;
        }
    }
    else
    {   // (AudioOutputPlaybackState::Muted != m_playbackState)
        // cannot be anything else than Muted or Playing ==> playing

        // condition to mute is not enough samples || muteFlag
        if (count < bytesToRead)
        {   // not enough samples -> reading what we have and filling rest with zeros
            // minimum mute time is 1ms (m_sampleRate_kHz samples) , if less then hard mute
            if (m_sampleRate_kHz*m_bytesPerFrame > count)
            {   // nothing to play (cannot apply mute ramp)
                qCInfo(audioOutput, "Hard mute [no samples available]");
                memset(outputBuffer, 0, bytesToRead);
                m_playbackState = AudioOutputPlaybackState::Muted;
                return paContinue;
            }

            // there are some samples available
            availableSamples = count/m_bytesPerFrame;

            Q_ASSERT(count == availableSamples*m_bytesPerFrame);

            // copy all available samples to output buffer
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < count)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {   // only copy here
                    memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (count - bytesToEnd));
                    m_inFifoPtr->tail = count - bytesToEnd;
                }
                else
                {
                    //memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    //memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (count - bytesToEnd));
                    inDataPtr = (int16_t *) m_inFifoPtr->buffer;
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((count - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == (count - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_inFifoPtr->tail = count - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {   // only copy here
                    memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, count);
                    m_inFifoPtr->tail += count;
                }
                else
                {
                    //memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, count);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (count >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == count);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_inFifoPtr->tail += count;
                }
            }
            // set rest of the samples to be 0
            memset((uint8_t*)outputBuffer+count, 0, bytesToRead-count);

            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= count;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

            // request to apply mute ramp
            request = Request::Mute;
        }
        else
        {   // enough sample available -> reading samples
            uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_inFifoPtr->tail;
            if (bytesToEnd < bytesToRead)
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {   // only copy here
                    memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    m_inFifoPtr->tail = bytesToRead - bytesToEnd;
                }
                else
                {
                    //memcpy((uint8_t*)outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToEnd);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToEnd >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == bytesToEnd);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }

                    //memcpy((uint8_t*)outputBuffer + bytesToEnd, m_inFifoPtr->buffer, (bytesToRead - bytesToEnd));
                    inDataPtr = (int16_t *) m_inFifoPtr->buffer;
                    outDataPtr = (int16_t *) ((uint8_t*)outputBuffer + bytesToEnd);
                    // each sample is int16 => 2 bytes per samples
                    numSamples = ((bytesToRead - bytesToEnd) >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == (bytesToRead - bytesToEnd));

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_inFifoPtr->tail = bytesToRead - bytesToEnd;
                }
            }
            else
            {
                float volume = m_linearVolume;
                if (volume > 0.9)
                {
                    memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                    m_inFifoPtr->tail += bytesToRead;
                }
                else
                {
                    //memcpy((uint8_t*) outputBuffer, m_inFifoPtr->buffer+m_inFifoPtr->tail, bytesToRead);
                    int16_t * inDataPtr = (int16_t *) (m_inFifoPtr->buffer+m_inFifoPtr->tail);
                    int16_t * outDataPtr = (int16_t *) outputBuffer;
                    // each sample is int16 => 2 bytes per samples
                    uint_fast32_t numSamples = (bytesToRead >> 1);

                    Q_ASSERT(numSamples*sizeof(int16_t) == bytesToRead);

                    for (uint_fast32_t n = 0; n < numSamples; ++n)
                    {
#if AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND
                        *outDataPtr++ = int16_t(std::lroundf(volume * *inDataPtr++));
#else
                        *outDataPtr++ = int16_t(volume * *inDataPtr++);
#endif
                    }
                    m_inFifoPtr->tail += bytesToRead;
                }
            }
            m_inFifoPtr->mutex.lock();
            m_inFifoPtr->count -= bytesToRead;
            m_inFifoPtr->countChanged.wakeAll();
            m_inFifoPtr->mutex.unlock();

//            if ((Request::Restart & request) && (count >= 4*bytesToRead))
//            {   // removing restart flag ==> play all samples we have
//                request &= ~Request::Restart;
//            }

            if (Request::None == request)
            {   // done
                return paContinue;
            }
        }
    }

    // at this point we have buffer that needs to be muted or unmuted
    // it is indicated by request variable

    // unmute
    if (Request::None == request)
    {   // unmute can be requested only when there is enough samples
        qCInfo(audioOutput) << "Unmuting audio";

        float coe = 2.0 - m_muteFactor;
        float gain = AUDIOOUTPUT_FADE_MIN_LIN;
        int16_t * dataPtr = (int16_t *) outputBuffer;
        for (uint_fast32_t n = 0; n < availableSamples; ++n)
        {
            for (uint_fast8_t c = 0; c < m_numChannels; ++c)
            {
                *dataPtr = int16_t(std::lroundf(gain * *dataPtr));
                dataPtr++;
            }
            gain = gain * coe;  // after by purpose
        }

        m_playbackState = AudioOutputPlaybackState::Playing; // playing

        // done
        return paContinue;
    }
    else
    {   // there is so request => muting
        // mute can be requested when there is not enough samples or from HMI
        qCInfo(audioOutput, "Muting... [available %u samples]", availableSamples);
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
                *dataPtr = int16_t(std::lroundf(gain * *dataPtr));
                dataPtr++;
            }
        }

        m_playbackState = AudioOutputPlaybackState::Muted; // muted

        if (request & (Request::Stop | Request::Restart))
        {   // stop or restart requested ==> finish playback
            return paComplete;
        }
    }

    return paContinue;
}

void AudioOutputPa::portAudioStreamFinishedCb(void *ctx)
{
    static_cast<AudioOutputPa*>(ctx)->portAudioStreamFinishedPrivateCb();
}

void AudioOutputPa::onStreamFinished()
{
    if (m_cbRequest & Request::Restart)
    {   // restart was requested (flag is cleared in start routine)
        emit audioOutputRestart();
        start(m_restartFifoPtr);
    }
#ifdef Q_OS_WIN
    else if (!(m_cbRequest & Request::Stop))
    {   // finished and not stop request ==> problem with output device
        // start again
        qCWarning(audioOutput, "Current audio device probably removed, trying new default device");

        // hotplug handling
        Pa_Terminate();
        Pa_Initialize();

        /* Open an audio I/O stream. */
        PaError err = Pa_OpenDefaultStream( &m_outStream,
                                           0,              /* no input channels */
                                           m_numChannels,  /* stereo output */
                                           paInt16,        /* 16 bit floating point output */
                                           m_sampleRate_kHz*1000,
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

        err = Pa_SetStreamFinishedCallback(m_outStream, portAudioStreamFinishedCb);
        if (paNoError != err)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
        }

        m_playbackState = AudioOutputPlaybackState::Muted;
        m_cbRequest &= ~(Request::Stop | Request::Restart);  // reset stop and restart bits

        err = Pa_StartStream(m_outStream);
        if (paNoError != err)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + "PortAudio error:" + Pa_GetErrorText( err ) );
        }
    }
#endif
    else { /* do nothing */ }
}

