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

#include "audiodecoder.h"

#include <math.h>

#include <QDebug>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(audioDecoder, "AudioDecoder", QtDebugMsg)

audioFifo_t audioFifo[2];

AudioDecoder::AudioDecoder(AudioRecorder *recorder, QObject *parent) : QObject(parent)
{
    m_outFifoIdx = 0;
    m_outFifoPtr = &audioFifo[m_outFifoIdx];

    m_outBufferPtr = new int16_t[AUDIO_DECODER_BUFFER_SIZE];

    m_mp2DecoderHandle = nullptr;
    m_playbackState = PlaybackState::Stopped;

    Q_ASSERT(recorder != nullptr);
    m_recorder = recorder;
}

AudioDecoder::~AudioDecoder()
{
    delete[] m_outBufferPtr;

    if (nullptr != m_mp2DecoderHandle)
    {
        int res = mpg123_close(m_mp2DecoderHandle);
        if (MPG123_OK != res)
        {
            qCCritical(audioDecoder, "error while mpg123_close: %s\n", mpg123_plain_strerror(res));
        }

        mpg123_delete(m_mp2DecoderHandle);
        mpg123_exit();
    }
}

void AudioDecoder::start(const RadioControlServiceComponent &s)
{
    if (s.isAudioService())
    {
        if (PlaybackState::Stopped != m_playbackState)
        {  // if running, then stop it first
            stop();
        }
        m_playbackState = PlaybackState::WaitForInit;
        m_recorder->setAudioService(s);
    }
    else
    {  // no audio service -> can happen during reconfiguration
        stop();
    }
}

void AudioDecoder::stop()
{
    m_playbackState = PlaybackState::Stopped;
    m_recorder->stop();

    deinitAACDecoder();
    deinitMPG123();

    emit stopAudio();
}

void AudioDecoder::initMPG123()
{
    deinitMPG123();
    deinitAACDecoder();

    int res = mpg123_init();
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_init");
    }

    m_mp2DecoderHandle = mpg123_new(nullptr, &res);
    if (nullptr == m_mp2DecoderHandle)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_new: " + std::string(mpg123_plain_strerror(res)));
    }

    // set allowed formats
    res = mpg123_format_none(m_mp2DecoderHandle);
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format_none: " + std::string(mpg123_plain_strerror(res)));
    }

    res = mpg123_format(m_mp2DecoderHandle, 48000, MPG123_STEREO, MPG123_ENC_SIGNED_16);
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format for 48KHz: " + std::string(mpg123_plain_strerror(res)));
    }

    res = mpg123_format(m_mp2DecoderHandle, 24000, MPG123_STEREO, MPG123_ENC_SIGNED_16);
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format for 24KHz: " + std::string(mpg123_plain_strerror(res)));
    }

    // disable resync limit
    res = mpg123_param(m_mp2DecoderHandle, MPG123_RESYNC_LIMIT, -1, 0);
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_param: " + std::string(mpg123_plain_strerror(res)));
    }

    res = mpg123_open_feed(m_mp2DecoderHandle);
    if (MPG123_OK != res)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_open_feed: " + std::string(mpg123_plain_strerror(res)));
    }
}

void AudioDecoder::deinitMPG123()
{
    if (nullptr != m_mp2DecoderHandle)
    {
        int res = mpg123_close(m_mp2DecoderHandle);
        if (MPG123_OK != res)
        {
            qCCritical(audioDecoder, "error while mpg123_close: %s\n", mpg123_plain_strerror(res));
        }

        mpg123_delete(m_mp2DecoderHandle);
        mpg123_exit();
        m_mp2DecoderHandle = nullptr;
    }
}

void AudioDecoder::decodeData(RadioControlAudioData *inData)
{
    if (PlaybackState::Stopped == m_playbackState)
    {                   // do nothing if not running
        delete inData;  // free input buffer
        return;
    }

    switch (inData->ASCTy)
    {
        case DabAudioDataSCty::DAB_AUDIO:
            processMP2(inData);
            break;

        case DabAudioDataSCty::DABPLUS_AUDIO:
            processAAC(inData);
            break;

        default:;  // do nothing
    }

    m_recorder->recordData(inData, m_outBufferPtr, m_outputBufferSamples);

    // free input data
    delete inData;
}

void AudioDecoder::getAudioParameters()
{
    if (isAACHandleValid())
    {
        readAACHeader();
        return;
    }
    if (nullptr != m_mp2DecoderHandle)
    {
        getFormatMP2();
    }
}

void AudioDecoder::processMP2(RadioControlAudioData *inData)
{
#define MP2_FRAME_PCM_SAMPLES (2 * 1152)
#define MP2_DRC_ENABLE 1

    if (nullptr == m_mp2DecoderHandle)
    {  // this can happen when audio changes from AAC to MP2
        m_inputDataDecoderId = inData->id;
        initMPG123();
    }

    if (inData->data.size() > 0)
    {
        // [ETSI TS 103 466 V1.2.1 (2019-09)]
        // 5.2.9 Formatting of the audio bit stream
        // It shall further divide this bit stream into audio frames, each corresponding to 1152 PCM audio samples,
        // which is equivalent to a duration of 24 ms in the case of 48 kHz sampling frequency
        // and 48 ms in the case of 24 kHz sampling frequency.

        /* Feed input chunk and get first chunk of decoded audio. */
        size_t size;
        int ret = mpg123_decode(m_mp2DecoderHandle, &inData->data[0], inData->data.size(), m_outBufferPtr,
                                AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t), &size);
        if ((MPG123_NEW_FORMAT == ret) || (inData->id != m_inputDataDecoderId))
        {  // this is stream reconfiguration or announcement (different instance)
            long sampleRate;
            int numChannels, enc;
            mpg123_getformat(m_mp2DecoderHandle, &sampleRate, &numChannels, &enc);

            qCInfo(audioDecoder, "New MP2 format: %ld Hz, %d channels, encoding %d", sampleRate, numChannels, enc);

            setOutput(sampleRate, numChannels);

            getFormatMP2();

            if (inData->id != m_inputDataDecoderId)
            {  // announcement -> should not happen
                m_recorder->stop();
            }

            m_inputDataDecoderId = inData->id;
            m_mp2DRC = 0;
        }

        m_outputBufferSamples = size / sizeof(int16_t);

        // there should be nothing more to decode, but try to be sure
        while (ret != MPG123_ERR && ret != MPG123_NEED_MORE)
        {  // Get all decoded audio that is available now before feeding more input
            ret = mpg123_decode(m_mp2DecoderHandle, NULL, 0, m_outBufferPtr + m_outputBufferSamples,
                                (AUDIO_DECODER_BUFFER_SIZE - m_outputBufferSamples) * sizeof(int16_t), &size);

            m_outputBufferSamples += size / sizeof(int16_t);

            if ((0 == size) || (m_outputBufferSamples >= AUDIO_DECODER_BUFFER_SIZE))
            {
                break;
            }
        }

        // we have m_outputBufferSamples decoded

#if MP2_DRC_ENABLE
        if (m_mp2DRC != 0)
        {                                                    // multiply buffer by gain
            float gain = pow(10, m_mp2DRC * 0.0125);         // 0.0125 = 1/(4*20)
            for (int n = 0; n < m_outputBufferSamples; ++n)  // considering int16_t data => 2 bytes
            {                                                // multiply all samples by gain
                m_outBufferPtr[n] = int16_t(qRound(m_outBufferPtr[n] * gain));
            }
        }
#endif  // MP2_DRC_ENABLE

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
    // store DRC for next frame
    m_mp2DRC = inData->header.mp2DRC;
}

void AudioDecoder::getFormatMP2()
{
    mpg123_frameinfo info;
    int res = mpg123_info(m_mp2DecoderHandle, &info);

    if (MPG123_OK != res)
    {
        qCCritical(audioDecoder, "error while mpg123_info: %s", mpg123_plain_strerror(res));
    }

    m_audioParameters.coding = AudioCoding::MP2;
    m_audioParameters.parametricStereo = false;
    switch (info.mode)
    {
        case MPG123_M_STEREO:
            m_audioParameters.stereo = true;
            m_audioParameters.sbr = false;
            break;

        case MPG123_M_JOINT:
            m_audioParameters.stereo = true;
            m_audioParameters.sbr = true;
            break;

        case MPG123_M_DUAL:
            // this should not happen -> not supported by DAB
            m_audioParameters.stereo = true;
            m_audioParameters.sbr = false;
            break;

        case MPG123_M_MONO:
            m_audioParameters.stereo = false;
            m_audioParameters.sbr = false;
            break;
    }

    m_audioParameters.sampleRateKHz = info.rate / 1000;
    m_recorder->setDataFormat(m_audioParameters.sampleRateKHz, false);
    emit audioParametersInfo(m_audioParameters);
}

void AudioDecoder::setOutput(int sampleRate, int numChannels)
{
    // toggle index 0->1 or 1->0
    m_outFifoIdx = (m_outFifoIdx + 1) & 0x1;
    m_outFifoPtr = &audioFifo[m_outFifoIdx];

    m_outFifoPtr->sampleRate = sampleRate;
    m_outFifoPtr->numChannels = numChannels;
    m_outFifoPtr->reset();

    if (PlaybackState::Running == m_playbackState)
    {  // switch audio source
        emit switchAudio(m_outFifoPtr);
    }
    else
    {  // start audio
        m_playbackState = PlaybackState::Running;
        emit startAudio(m_outFifoPtr);
    }
}

void AudioDecoder::readAACHeader()
{
    // fill the structure used to signal audio params to HMI
    if (m_aacHeader.bits.sbr_flag)
    {
        if (m_aacHeader.bits.ps_flag)
        {
            m_audioParameters.coding = AudioCoding::HEAACv2;
        }
        else
        {
            m_audioParameters.coding = AudioCoding::HEAAC;
        }
    }
    else
    {
        m_audioParameters.coding = AudioCoding::AACLC;
    }
    if (m_aacHeader.bits.aac_channel_mode || m_aacHeader.bits.ps_flag)
    {
        m_audioParameters.stereo = true;
    }
    else
    {
        m_audioParameters.stereo = false;
    }
    m_audioParameters.sampleRateKHz = (m_aacHeader.bits.dac_rate) ? (48) : (32);
    m_audioParameters.parametricStereo = m_aacHeader.bits.ps_flag;
    m_audioParameters.sbr = m_aacHeader.bits.sbr_flag;

    m_recorder->setDataFormat(m_audioParameters.sampleRateKHz, true);
    m_streamDropout = false;
    emit audioParametersInfo(m_audioParameters);

    qCInfo(audioDecoder, "%s %d kHz %s", (m_aacHeader.bits.sbr_flag ? (m_aacHeader.bits.ps_flag ? "HE-AAC v2" : "HE-AAC") : "AAC-LC"),
           (m_aacHeader.bits.dac_rate ? 48 : 32), (m_aacHeader.bits.aac_channel_mode || m_aacHeader.bits.ps_flag ? "stereo" : "mono"));

    /* AudioSpecificConfig structure (the only way to select 960 transform here!)
     *          *  00010 = AudioObjectType 2 (AAC LC)
     *  xxxx  = (core) sample rate index
     *  xxxx  = (core) channel config
     *  100   = GASpecificConfig with 960 transform
     *          * SBR: explicit signaling (backwards-compatible), adding:
     *  01010110111 = sync extension for SBR
     *  00101       = AudioObjectType 5 (SBR)
     *  1           = SBR present flag
     *  xxxx        = extension sample rate index
     *          * PS:  explicit signaling (backwards-compatible), adding:
     *  10101001000 = sync extension for PS
     *  1           = PS present flag
     *          * Note:
     * libfaad2 does not support non backwards-compatible PS signaling (AOT 29);
     * it detects PS only by implicit signaling.
     */

    uint8_t coreSrIndex = 0;
    if ((m_aacHeader.bits.dac_rate == 0) && (m_aacHeader.bits.sbr_flag == 1))
    {
        // audioFs = 16*2;
        coreSrIndex = 0x8;  // 16 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 1))
    {
        // audioFs = 24*2;
        coreSrIndex = 0x6;  // 24 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 0) && (m_aacHeader.bits.sbr_flag == 0))
    {
        // audioFs = 32;
        coreSrIndex = 0x5;  // 32 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 0))
    {
        // audioFs = 48;
        coreSrIndex = 0x3;  // 48 kHz
    }

    uint8_t coreChannelConfig = m_aacHeader.bits.aac_channel_mode + 1;
    uint8_t extensionSrIndex = 5 - 2 * m_aacHeader.bits.dac_rate;  // dac_rate ? 3 : 5

    // AAC LC
    m_ascLen = 0;
    m_asc[m_ascLen++] = 0b00010 << 3 | coreSrIndex >> 1;
    m_asc[m_ascLen++] = (coreSrIndex & 0x01) << 7 | coreChannelConfig << 3 | 0b100;

    if (m_aacHeader.bits.sbr_flag)
    {
        // add SBR
        m_asc[m_ascLen++] = 0x56;
        m_asc[m_ascLen++] = 0xE5;
        m_asc[m_ascLen++] = 0x80 | (extensionSrIndex << 3);

        if (m_aacHeader.bits.ps_flag)
        {
            // add PS
            m_asc[m_ascLen - 1] |= 0x05;
            m_asc[m_ascLen++] = 0x48;
            m_asc[m_ascLen++] = 0x80;
        }
    }
}
