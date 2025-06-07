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

#include "audiodecoderfaad.h"

#include <math.h>

#include <QDebug>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_DECLARE_LOGGING_CATEGORY(audioDecoder)

AudioDecoderFAAD::AudioDecoderFAAD(AudioRecorder *recorder, QObject *parent) : AudioDecoder(recorder, parent)
{
    m_aacDecoderHandle = nullptr;

#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
    m_noiseBufferPtr = new int16_t[AUDIO_DECODER_BUFFER_SIZE];
    m_noiseFile = nullptr;
    memset(m_noiseBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
#endif
}

AudioDecoderFAAD::~AudioDecoderFAAD()
{
    if (nullptr != m_aacDecoderHandle)
    {
        NeAACDecClose(m_aacDecoderHandle);
    }

#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
    if (nullptr != m_noiseFile)
    {
        m_noiseFile->close();
        delete m_noiseFile;
    }
    delete[] m_noiseBufferPtr;
#endif
}

void AudioDecoderFAAD::initAACDecoder()
{
    deinitMPG123();
    deinitAACDecoder();

    m_aacDecoderHandle = NeAACDecOpen();
    if (!m_aacDecoderHandle)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecOpen");
    }

    // ensure features
    unsigned long cap = NeAACDecGetCapabilities();
    if (!(cap & LC_DEC_CAP))
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": no LC decoding support!");
    }

    // set general config
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(m_aacDecoderHandle);
    if (!config)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecGetCurrentConfiguration");
    }

    config->outputFormat = FAAD_FMT_16BIT;
    config->dontUpSampleImplicitSBR = 0;
    config->downMatrix = 1;

    if (NeAACDecSetConfiguration(m_aacDecoderHandle, config) != 1)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecSetConfiguration");
    }

    // init decoder
    unsigned long sampleRate;
    unsigned char numChannels;
    long int init_result = NeAACDecInit2(m_aacDecoderHandle, m_asc, (unsigned long)m_ascLen, &sampleRate, &numChannels);
    if (init_result != 0)
    {
        throw std::runtime_error("AACDecoderFAAD2: error while NeAACDecInit2: " + std::string(NeAACDecGetErrorMessage(-init_result)));
    }

    m_numChannels = numChannels;
    m_outputBufferSamples = 960 * numChannels * (m_aacHeader.bits.sbr_flag ? 2 : 1);

    // calculate mute ramp
    // mute ramp is sin^2 that changes from 0 to 1 during AUDIOOUTPUT_FADE_TIME_MS
    m_muteRamp.clear();
    int sampleRate_kHz = sampleRate / 1000.0;
    float coe = M_PI / (2.0 * AUDIO_DECODER_FADE_TIME_MS * sampleRate_kHz);
    for (int n = 0; n < AUDIO_DECODER_FADE_TIME_MS * sampleRate_kHz; ++n)
    {
        float g = sinf(n * coe);
        m_muteRamp.push_back(g * g);
    }

    qCInfo(audioDecoder, "Output sample rate %lu Hz, channels: %d", sampleRate, numChannels);

    setOutput(sampleRate, numChannels);
}

void AudioDecoderFAAD::deinitAACDecoder()
{
    if (nullptr != m_aacDecoderHandle)
    {
        NeAACDecClose(m_aacDecoderHandle);
        m_aacDecoderHandle = nullptr;
    }
}

void AudioDecoderFAAD::setNoiseConcealment(int level)
{
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
    if (nullptr != m_noiseFile)
    {
        m_noiseFile->close();
        delete m_noiseFile;
        m_noiseFile = nullptr;
    }

    if (level > 0)
    {
        m_noiseFile = new QFile(":/resources/noise.bin");
        if (!m_noiseFile->open(QIODevice::ReadOnly))
        {
            qCWarning(audioDecoder) << "Unable to open noise file";
            delete m_noiseFile;
            m_noiseFile = nullptr;
            memset(m_noiseBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        }
        else
        {  // OK
            m_noiseLevel = pow(10, -1.0 * level / 20);
        }
    }
    else
    {  // set buffer to zero
        memset(m_noiseBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_noiseLevel = 0.0;
    }
#endif
}

void AudioDecoderFAAD::processAAC(RadioControlAudioData *inData)
{
    dabsdrAudioFrameHeader_t header;

    header = inData->header;

    if (nullptr == m_aacDecoderHandle)
    {  // this can happen when format changes from MP2 to AAC or during init
       // not necessary -> will be set in init state
        // memset(m_outBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_state = OutputState::Init;
        m_aacHeader.raw = header.raw;
        m_inputDataDecoderId = inData->id;
        readAACHeader();
        initAACDecoder();

        m_playbackState = PlaybackState::Running;
    }

    uint8_t conceal = header.bits.conceal;
    if (conceal)
    {
        // check if this is stream dropout
        uint64_t sum = 0;
        for (auto it = inData->data.cbegin(); it != inData->data.cend(); ++it)
        {
            sum += *it;
        }
        if (sum == 0 && !m_streamDropout)  // all zero
        {
            m_streamDropout = true;
            auto audioParams = m_audioParameters;
            audioParams.coding = AudioCoding::None;
            emit audioParametersInfo(audioParams);
        }

        // concealment not supported
        // supposing that header is the same and setting buffer to 0
        inData->data.assign(inData->data.size(), 0);
        header = m_aacHeader;
    }
    else if (m_streamDropout)
    {
        m_streamDropout = false;
        emit audioParametersInfo(m_audioParameters);
    }

    if ((header.raw != m_aacHeader.raw && inData->data.size() > 10) || (inData->id != m_inputDataDecoderId))
    {  // this is stream reconfiguration or announcement (different instance)
       // not necessary -> will be set in init state
        // memset(m_outBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_state = OutputState::Init;

        if (inData->id != m_inputDataDecoderId)
        {  // announcement -> should not happen
            m_recorder->stop();
        }

        m_aacHeader.raw = header.raw;
        m_inputDataDecoderId = inData->id;
        readAACHeader();
        initAACDecoder();
    }

    // decode audio
    uint8_t *outputFrame = (uint8_t *)NeAACDecDecode(m_aacDecoderHandle, &m_aacDecFrameInfo, &inData->data[0], inData->data.size());

    handleAudioOutput(m_aacDecFrameInfo, outputFrame);
}

void AudioDecoderFAAD::handleAudioOutput(const NeAACDecFrameInfo &frameInfo, const uint8_t *inFramePtr)
{
    if (frameInfo.samples != m_outputBufferSamples)
    {
        if (OutputState::Unmuted == m_state)
        {  // do mute
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
           // read noise samples
            qCInfo(audioDecoder) << "Muting audio (decoding errors)";
            int valuesToRead = m_muteRamp.size() * m_numChannels;
            if (nullptr != m_noiseFile)
            {
                if (m_noiseFile->bytesAvailable() < valuesToRead * sizeof(int16_t))
                {
                    m_noiseFile->seek(0);
                }
                m_noiseFile->read((char *)m_noiseBufferPtr, valuesToRead * sizeof(int16_t));
            }
            int16_t *noisePtr = m_noiseBufferPtr;
#else

#endif

            int16_t *dataPtr = &m_outBufferPtr[m_outputBufferSamples - 1];  // last sample
            std::vector<float>::const_iterator it = m_muteRamp.cbegin();
            while (it != m_muteRamp.end())
            {
                float gain = *it++;
                for (uint_fast32_t ch = 0; ch < m_numChannels; ++ch)
                {
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
                    *dataPtr = int16_t(qRound(gain * *dataPtr)) + (1 - gain) * m_noiseLevel * *noisePtr++;
#else
                    *dataPtr = int16_t(qRound(gain * *dataPtr));
#endif
                    --dataPtr;
                }
            }
        }
        else if (OutputState::Init == m_state)
        {  // do nothing and return -> decoder is initializing
            return;
        }
        else  // muted
        {     /* do nothing */
        }
    }

    if (OutputState::Init == m_state)
    {  // only copy to internal buffer -> this is the first buffer
        memcpy(m_outBufferPtr, inFramePtr, m_outputBufferSamples * sizeof(int16_t));

        // apply unmute ramp
        int16_t *dataPtr = &m_outBufferPtr[0];  // first sample
        std::vector<float>::const_iterator it = m_muteRamp.cbegin();
        while (it != m_muteRamp.end())
        {
            float gain = *it++;
            for (uint_fast32_t ch = 0; ch < m_numChannels; ++ch)
            {
                *dataPtr = int16_t(qRound(gain * *dataPtr));
                dataPtr += 1;
            }
        }

        m_state = OutputState::Unmuted;
        return;
    }

    // copy data to output FIFO
    int64_t bytesToWrite = m_outputBufferSamples * sizeof(int16_t);

    // wait for space in ouput buffer
    m_outFifoPtr->mutex.lock();
    uint64_t count = m_outFifoPtr->count;
    while (int64_t(AUDIO_FIFO_SIZE - count) < bytesToWrite)
    {
        m_outFifoPtr->countChanged.wait(&m_outFifoPtr->mutex);
        count = m_outFifoPtr->count;
    }
    m_outFifoPtr->mutex.unlock();

    int64_t bytesToEnd = AUDIO_FIFO_SIZE - m_outFifoPtr->head;
    if (bytesToEnd < bytesToWrite)
    {
        memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, m_outBufferPtr, bytesToEnd);
        memcpy(m_outFifoPtr->buffer, reinterpret_cast<uint8_t *>(m_outBufferPtr) + bytesToEnd, bytesToWrite - bytesToEnd);
        m_outFifoPtr->head = bytesToWrite - bytesToEnd;
    }
    else
    {
        memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, m_outBufferPtr, bytesToWrite);
        m_outFifoPtr->head += bytesToWrite;
    }

    m_outFifoPtr->mutex.lock();
    m_outFifoPtr->count += bytesToWrite;
    m_outFifoPtr->mutex.unlock();

    // copy new data to buffer
    if (frameInfo.samples != m_outputBufferSamples)
    {  // error
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
       // read noise
        int valuesToRead = m_outputBufferSamples;
        if (nullptr != m_noiseFile)
        {
            if (m_noiseFile->bytesAvailable() < valuesToRead * sizeof(int16_t))
            {
                m_noiseFile->seek(0);
            }
            m_noiseFile->read((char *)m_noiseBufferPtr, valuesToRead * sizeof(int16_t));
        }
        // copy noise
        int16_t *dataPtr = m_outBufferPtr;
        int16_t *noisePtr = m_noiseBufferPtr;
        for (int n = 0; n < m_outputBufferSamples; ++n)
        {
            *dataPtr++ = m_noiseLevel * *noisePtr++;
        }
        m_state = OutputState::Muted;
#else
        if (OutputState::Unmuted == m_state)
        {  // copy 0
            memset(m_outBufferPtr, 0, m_outputBufferSamples * sizeof(int16_t));
            m_state = OutputState::Muted;
        }
#endif
    }
    else
    {  // OK
        memcpy(m_outBufferPtr, inFramePtr, m_outputBufferSamples * sizeof(int16_t));

        if (OutputState::Muted == m_state)
        {  // do unmute
            qCInfo(audioDecoder) << "Umuting audio";
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
            // read noise
            int valuesToRead = m_muteRamp.size() * m_numChannels;
            if (nullptr != m_noiseFile)
            {
                if (m_noiseFile->bytesAvailable() < valuesToRead * sizeof(int16_t))
                {
                    m_noiseFile->seek(0);
                }
                m_noiseFile->read((char *)m_noiseBufferPtr, valuesToRead * sizeof(int16_t));
            }
            int16_t *noisePtr = m_noiseBufferPtr;
#endif

            // apply unmute ramp
            int16_t *dataPtr = &m_outBufferPtr[0];  // first sample
            std::vector<float>::const_iterator it = m_muteRamp.cbegin();
            while (it != m_muteRamp.end())
            {
                float gain = *it++;
                for (uint_fast32_t ch = 0; ch < m_numChannels; ++ch)
                {
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
                    *dataPtr = int16_t(qRound(gain * *dataPtr)) + (1 - gain) * m_noiseLevel * *noisePtr++;
#else
                    *dataPtr = int16_t(qRound(gain * *dataPtr));
#endif
                    dataPtr += 1;
                }
            }
            m_state = OutputState::Unmuted;
        }
    }
}
