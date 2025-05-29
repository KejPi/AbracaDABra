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
    m_aacDecoderHandle = nullptr;
    m_mp2DecoderHandle = nullptr;

    Q_ASSERT(recorder != nullptr);
    m_recorder = recorder;

    m_playbackState = PlaybackState::Stopped;

#if HAVE_FDKAAC
    Q_ASSERT(sizeof(int16_t) == sizeof(INT_PCM));
#endif
    m_outBufferPtr = new int16_t[AUDIO_DECODER_BUFFER_SIZE];

#if !HAVE_FDKAAC
#if AUDIO_DECODER_NOISE_CONCEALMENT
    m_noiseBufferPtr = new int16_t[AUDIO_DECODER_BUFFER_SIZE];
    m_noiseFile = nullptr;
    memset(m_noiseBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
#endif
#endif
}

AudioDecoder::~AudioDecoder()
{
    if (nullptr != m_aacDecoderHandle)
    {
#if HAVE_FDKAAC
        aacDecoder_Close(m_aacDecoderHandle);
#else
        NeAACDecClose(m_aacDecoderHandle);
#endif
    }
    delete[] m_outBufferPtr;

#if !HAVE_FDKAAC
#if AUDIO_DECODER_NOISE_CONCEALMENT
    if (nullptr != m_noiseFile)
    {
        m_noiseFile->close();
        delete m_noiseFile;
    }
    delete[] m_noiseBufferPtr;
#endif
#endif

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
     *
     *  00010 = AudioObjectType 2 (AAC LC)
     *  xxxx  = (core) sample rate index
     *  xxxx  = (core) channel config
     *  100   = GASpecificConfig with 960 transform
     *
     * SBR: explicit signaling (backwards-compatible), adding:
     *  01010110111 = sync extension for SBR
     *  00101       = AudioObjectType 5 (SBR)
     *  1           = SBR present flag
     *  xxxx        = extension sample rate index
     *
     * PS:  explicit signaling (backwards-compatible), adding:
     *  10101001000 = sync extension for PS
     *  1           = PS present flag
     *
     * Note:
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

#if HAVE_FDKAAC
void AudioDecoder::initAACDecoder()
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
    setOutput(m_aacHeader.bits.dac_rate ? 48000 : 32000, channels);
}

#else   // HAVE_FDKAAC
void AudioDecoder::initAACDecoder()
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
#endif  // HAVE_FDKAAC

void AudioDecoder::deinitAACDecoder()
{
    if (nullptr != m_aacDecoderHandle)
    {
#if HAVE_FDKAAC
        aacDecoder_Close(m_aacDecoderHandle);
#else
        NeAACDecClose(m_aacDecoderHandle);
#endif
        m_aacDecoderHandle = nullptr;
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
    if (nullptr != m_aacDecoderHandle)
    {
        readAACHeader();
        return;
    }
    if (nullptr != m_mp2DecoderHandle)
    {
        getFormatMP2();
    }
}

void AudioDecoder::setNoiseConcealment(int level)
{
#if !HAVE_FDKAAC && AUDIO_DECODER_NOISE_CONCEALMENT
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

void AudioDecoder::processAAC(RadioControlAudioData *inData)
{
    dabsdrAudioFrameHeader_t header;

    header = inData->header;
    if (nullptr == m_aacDecoderHandle)
    {  // this can happen when format changes from MP2 to AAC or during init

        // this is the case when subchannel with all zeros is transmitted
        if (inData->data.size() == 1)
        {  // wrong data
            return;
        }

#if !HAVE_FDKAAC
        // not necessary -> will be set in init state
        // memset(m_outBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_state = OutputState::Init;
#endif
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

#if HAVE_FDKAAC
        // clear concealment bit
#if AUDIO_DECODER_FDKAAC_CONCEALMENT
        // supposing that header is the same
        header.raw = m_aacHeader.raw;
#else
        // concealment not supported => discarding
        return;
#endif
#else
        // concealment not supported
        // supposing that header is the same and setting buffer to 0
        inData->data.assign(inData->data.size(), 0);
        header = m_aacHeader;
#endif
    }
    else if (m_streamDropout)
    {
        m_streamDropout = false;
        emit audioParametersInfo(m_audioParameters);
    }

    if ((header.raw != m_aacHeader.raw && inData->data.size() > 10) || (inData->id != m_inputDataDecoderId))
    {  // this is stream reconfiguration or announcement (different instance)
#if !HAVE_FDKAAC
       // not necessary -> will be set in init state
        // memset(m_outBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_state = OutputState::Init;
#endif

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
#if HAVE_FDKAAC
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
    {  // no output
        return;
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
#else   // HAVE_FDKAAC
    uint8_t *outputFrame = (uint8_t *)NeAACDecDecode(m_aacDecoderHandle, &m_aacDecFrameInfo, &inData->data[0], inData->data.size());

    handleAudioOutputFAAD(m_aacDecFrameInfo, outputFrame);
#endif  // HAVE_FDKAAC
}

#if !HAVE_FDKAAC
void AudioDecoder::handleAudioOutputFAAD(const NeAACDecFrameInfo &frameInfo, const uint8_t *inFramePtr)
{
    if (frameInfo.samples != m_outputBufferSamples)
    {
        if (OutputState::Unmuted == m_state)
        {  // do mute
#if AUDIO_DECODER_NOISE_CONCEALMENT
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
#if AUDIO_DECODER_NOISE_CONCEALMENT
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
#if AUDIO_DECODER_NOISE_CONCEALMENT
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
            qCInfo(audioDecoder) << "Unmuting audio";
#if AUDIO_DECODER_NOISE_CONCEALMENT
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
#if AUDIO_DECODER_NOISE_CONCEALMENT
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
#endif  // HAVE_FDKAAC

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
