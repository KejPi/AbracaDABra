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

#include "audiodecoderfdkaac.h"

#include <math.h>

#include <QDebug>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_DECLARE_LOGGING_CATEGORY(audioDecoder)

AudioDecoderFDKAAC::AudioDecoderFDKAAC(AudioRecorder *recorder, QObject *parent) : AudioDecoder(recorder, parent)
{
    m_aacDecoderHandle = nullptr;

    Q_ASSERT(sizeof(int16_t) == sizeof(INT_PCM));
}

AudioDecoderFDKAAC::~AudioDecoderFDKAAC()
{
    if (nullptr != m_aacDecoderHandle)
    {
        aacDecoder_Close(m_aacDecoderHandle);
    }
}

void AudioDecoderFDKAAC::initAACDecoder()
{
    deinitMPG123();
    deinitAACDecoder();

    m_aacDecoderHandle = aacDecoder_Open(TT_MP4_RAW, 1);
    if (!m_aacDecoderHandle)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_Open");
    }

    // init decoder
    int channels = m_audioParameters.stereo ? 2 : 1;
    AAC_DECODER_ERROR init_result;

    /* Restrict output channel count to actual input channel count.
     *
     * Just using the parameter value -1 (no up-/downmix) does not work, as with
     * SBR and Mono the lib assumes possibly present PS and then outputs Stereo!
     *
     * Note:
     * Older lib versions use a combined parameter for the output channel count.
     * As the headers of these didn't define the version, branch accordingly.
     */
#if !defined(AACDECODER_LIB_VL0) && !defined(AACDECODER_LIB_VL1) && !defined(AACDECODER_LIB_VL2)
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) +
                                 ": error while setting parameter AAC_PCM_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#else
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_MIN_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) +
                                 ": error while setting parameter AAC_PCM_MIN_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_MAX_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) +
                                 ": error while setting parameter AAC_PCM_MAX_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#endif

    uint8_t *asc_array[1]{m_asc};
    const unsigned int asc_sizeof_array[1]{(unsigned int)m_ascLen};
    init_result = aacDecoder_ConfigRaw(m_aacDecoderHandle, asc_array, asc_sizeof_array);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_ConfigRaw: " + std::to_string(init_result));
    }

    m_outputBufferSamples = 960 * channels * (m_aacHeader.bits.sbr_flag ? 2 : 1);

    qCInfo(audioDecoder, "Output sample rate %d Hz, channels: %d", m_aacHeader.bits.dac_rate ? 48000 : 32000, channels);

    //    m_outFifoPtr->numChannels = uint8_t(channels);
    //    m_outFifoPtr->sampleRate = uint32_t(m_aacHeader.bits.dac_rate ? 48000 : 32000);
    //    emit startAudio(m_outFifoPtr);
    AudioDecoder::setOutput(m_aacHeader.bits.dac_rate ? 48000 : 32000, channels);
}

void AudioDecoderFDKAAC::deinitAACDecoder()
{
    if (nullptr != m_aacDecoderHandle)
    {
        aacDecoder_Close(m_aacDecoderHandle);
        m_aacDecoderHandle = nullptr;
    }
}

void AudioDecoderFDKAAC::setNoiseConcealment(int level)
{}

void AudioDecoderFDKAAC::processAAC(RadioControlAudioData *inData)
{
    dabsdrAudioFrameHeader_t header;

    header = inData->header;

    if (nullptr == m_aacDecoderHandle)
    {  // this can happen when format changes from MP2 to AAC or during init
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

        // clear concealment bit
#if AUDIO_DECODER_FDKAAC_CONCEALMENT
        // supposing that header is the same
        header.raw = m_aacHeader.raw;
#else
        // concealment not supported => discarding
        return;
#endif
    }
    else if (m_streamDropout)
    {
        m_streamDropout = false;
        emit audioParametersInfo(m_audioParameters);
    }

    if ((header.raw != m_aacHeader.raw && inData->data.size() > 10) || (inData->id != m_inputDataDecoderId))
    {  // this is stream reconfiguration or announcement (different instance)
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

    uint8_t *aacData[1] = {&inData->data[0]};
    unsigned int len[1];
    len[0] = inData->data.size();
    unsigned int bytesValid = len[0];

    // fill internal input buffer
    AAC_DECODER_ERROR result = aacDecoder_Fill(m_aacDecoderHandle, aacData, len, &bytesValid);

    if (result != AAC_DEC_OK)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_Fill: " + std::to_string(result));
    }
    if (bytesValid)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": aacDecoder_Fill did not consume all bytes");
    }

    // decode audio
    result = aacDecoder_DecodeFrame(m_aacDecoderHandle, (INT_PCM *)m_outBufferPtr, m_outputBufferSamples, AACDEC_CONCEAL * conceal);
    if (AAC_DEC_OK != result)
    {
        qCWarning(audioDecoder) << "Error decoding AAC frame:" << result;
    }

    if (!IS_OUTPUT_VALID(result))
    {  // no output - fill with zeros
        memset(m_outBufferPtr, 0, m_outputBufferSamples * sizeof(int16_t));
    }

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
}
