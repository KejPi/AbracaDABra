#include <QDebug>
#include <QFile>
#include <math.h>

#include "audiodecoder.h"

audioFifo_t audioFifo[2];

AudioDecoder::AudioDecoder(QObject *parent) : QObject(parent)
{
    m_outFifoIdx = 0;
    m_outFifoPtr = &audioFifo[m_outFifoIdx];
    m_aacDecoderHandle = nullptr;
    m_mp2DecoderHandle = nullptr;

    m_playbackState = PlaybackState::Stopped;

#if AUDIO_DECODER_MUTE_CONCEALMENT
    m_outBufferPtr = new int16_t[AUDIO_DECODER_BUFFER_SIZE];

    m_muteRamp.clear();
    // mute ramp is sin^2 that changes from 0 to 1 during AUDIOOUTPUT_FADE_TIME_MS
    // values are precalculated for 48 kHz to save MIPS in runtime
    float coe = M_PI / (2.0 * AUDIO_DECODER_FADE_TIME_MS * 48);
    for (int n = 0; n < AUDIO_DECODER_FADE_TIME_MS * 48; ++n)
    {
        float g = sinf(n * coe);
        m_muteRamp.push_back(g * g);
    }
#endif

#ifdef AUDIO_DECODER_RAW_OUT
    m_rawOut = fopen("audio.raw", "wb");
    if (!m_rawOut)
    {
        qDebug("Unable to open file: audio.raw");
    }
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    m_aacOut = fopen("audio.aac", "wb");
    if (!m_aacOut)
    {
        qDebug("Unable to open file: audio.aac");
    }
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    m_mp2Out = fopen("audio.mp2", "wb");
    if (!m_mp2Out)
    {
        qDebug("Unable to open file: audio.mp2");
    }
#endif
}

AudioDecoder::~AudioDecoder()
{
    if (nullptr != m_aacDecoderHandle)
    {
#if HAVE_FDKAAC
        aacDecoder_Close(m_aacDecoderHandle);
        if (nullptr != m_outputFrame)
        {
            delete [] m_outputFrame;
        }
#else
        NeAACDecClose(m_aacDecoderHandle);
#endif
    }
#if AUDIO_DECODER_MUTE_CONCEALMENT
    delete [] m_outBufferPtr;
#endif

    if (nullptr != m_mp2DecoderHandle)
    {
        int res = mpg123_close(m_mp2DecoderHandle);
        if (MPG123_OK != res)
        {
            qDebug("%s: error while mpg123_close: %s\n", Q_FUNC_INFO, mpg123_plain_strerror(res));
        }

        mpg123_delete(m_mp2DecoderHandle);
        mpg123_exit();
    }
#ifdef AUDIO_DECODER_RAW_OUT
    if (m_rawOut)
    {
        fclose(m_rawOut);
    }
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    if (m_aacOut)
    {
        fclose(m_aacOut);
    }
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    if (m_mp2Out)
    {
        fclose(m_mp2Out);
    }
#endif
}

void AudioDecoder::start(const RadioControlServiceComponent&s)
{
    //qDebug() << Q_FUNC_INFO;
    if (s.isAudioService())
    {
        if (PlaybackState::Stopped != m_playbackState)
        {   // if running, then stop it first
            stop();
        }

#if AUDIO_DECODER_MUTE_CONCEALMENT
        memset(m_outBufferPtr, 0, AUDIO_DECODER_BUFFER_SIZE * sizeof(int16_t));
        m_state = OutputState::Init;
#endif
//        m_outFifoIdx = 0;
//        m_outFifoPtr = &audioFifo[m_outFifoIdx];
        m_playbackState = PlaybackState::WaitForInit;
    }
    else
    {   // no audio service -> can happen during reconfiguration
        stop();
    }
}

void AudioDecoder::stop()
{
    //qDebug() << Q_FUNC_INFO;
    m_playbackState = PlaybackState::Stopped;

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
            qDebug("%s: error while mpg123_close: %s\n", Q_FUNC_INFO, mpg123_plain_strerror(res));
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

    emit audioParametersInfo(m_audioParameters);

    qDebug("%s %d kHz %s",
           (m_aacHeader.bits.sbr_flag ? (m_aacHeader.bits.ps_flag ? "HE-AAC v2" : "HE-AAC") : "AAC-LC"),
           (m_aacHeader.bits.dac_rate ? 48 : 32),
           (m_aacHeader.bits.aac_channel_mode || m_aacHeader.bits.ps_flag ? "stereo" : "mono")
           );

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
        //audioFs = 16*2;
        coreSrIndex = 0x8;  // 16 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 1))
    {
        //audioFs = 24*2;
        coreSrIndex = 0x6;  // 24 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 0) && (m_aacHeader.bits.sbr_flag == 0))
    {
        //audioFs = 32;
        coreSrIndex = 0x5;  // 32 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 0))
    {
        //audioFs = 48;
        coreSrIndex = 0x3;  // 48 kHz
    }

    uint8_t coreChannelConfig = m_aacHeader.bits.aac_channel_mode + 1;
    uint8_t extensionSrIndex = 5 - 2 * m_aacHeader.bits.dac_rate; // dac_rate ? 3 : 5

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
#if !defined (AACDECODER_LIB_VL0) && !defined (AACDECODER_LIB_VL1) && !defined (AACDECODER_LIB_VL2)
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#else
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_MIN_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_MIN_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
    init_result = aacDecoder_SetParam(m_aacDecoderHandle, AAC_PCM_MAX_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_MAX_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#endif

    uint8_t * asc_array[1] { m_asc };
    const unsigned int asc_sizeof_array[1] { (unsigned int)m_ascLen };
    init_result = aacDecoder_ConfigRaw(m_aacDecoderHandle, asc_array, asc_sizeof_array);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_ConfigRaw: " + std::to_string(init_result));
    }

    m_outputFrameLen = 960 * sizeof(INT_PCM) * channels * (m_aacHeader.bits.sbr_flag ? 2 : 1);

    if (nullptr != m_outputFrame)
    {
        delete [] m_outputFrame;
    }
    m_outputFrame = new uint8_t[m_outputFrameLen];

    qDebug("Output SR = %d, channels = %d", m_aacHeader.bits.dac_rate ? 48000 : 32000, channels);

//    m_outFifoPtr->numChannels = uint8_t(channels);
//    m_outFifoPtr->sampleRate = uint32_t(m_aacHeader.bits.dac_rate ? 48000 : 32000);
//    emit startAudio(m_outFifoPtr);
    setOutput(m_aacHeader.bits.dac_rate ? 48000 : 32000, channels);
}

#else // HAVE_FDKAAC
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

#if AUDIO_DECODER_MUTE_CONCEALMENT
    m_numChannels = numChannels;
    m_muteRampDsFactor = 48000 / sampleRate;
    m_outputBufferSamples = 960 * numChannels * (m_aacHeader.bits.sbr_flag ? 2 : 1);
#endif

    qDebug("Output SR = %lu, channels = %d", sampleRate, numChannels);

//    m_outFifoPtr->numChannels = uint8_t(outputCh);
//    m_outFifoPtr->sampleRate = uint32_t(outputSr);

//    if (PlaybackState::Running == m_playbackState)
//    {
//        qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> switch";
//    }
//    else
//    {
//        qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> start";
//    }

//    emit startAudio(m_outFifoPtr);
    setOutput(sampleRate, numChannels);
}

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

#endif // HAVE_FDKAAC

void AudioDecoder::decodeData(QByteArray *inData)
{
    if (PlaybackState::Stopped == m_playbackState)
    {   // do nothing if not running
        delete inData;  // free input buffer
        return;
    }

    switch (static_cast<DabAudioDataSCty>(inData->at(0)))
    {
        case DabAudioDataSCty::DAB_AUDIO:
            processMP2(inData);
            break;

        case DabAudioDataSCty::DABPLUS_AUDIO:
            processAAC(inData);
            break;

        default:
            ; // do nothing
    }

    // free input buffer
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

void AudioDecoder::processMP2(QByteArray *inData)
{
    #define MP2_FRAME_PCM_SAMPLES (2 * 1152)
    #define MP2_DRC_ENABLE        1

    if (nullptr == m_mp2DecoderHandle)
    {   // this can happen when audio changes from AAC to MP2
        initMPG123();
    }

#ifdef AUDIO_DECODER_MP2_OUT
    writeMP2Output(inData->data() + 2, inData->size() - 2);
#endif

    if (inData->size() > 2)
    {
        // input data
        const char * mp2Data = inData->data() + 2;

        // [ETSI TS 103 466 V1.2.1 (2019-09)]
        // 5.2.9 Formatting of the audio bit stream
        // It shall further divide this bit stream into audio frames, each corresponding to 1152 PCM audio samples,
        // which is equivalent to a duration of 24 ms in the case of 48 kHz sampling frequency
        // and 48 ms in the case of 24 kHz sampling frequency.
        int16_t * outBuf = new int16_t[MP2_FRAME_PCM_SAMPLES];

        /* Feed input chunk and get first chunk of decoded audio. */
        size_t size;
        int ret = mpg123_decode(m_mp2DecoderHandle, (const unsigned char *)mp2Data, (inData->size() - 2), outBuf, MP2_FRAME_PCM_SAMPLES * sizeof(int16_t), &size);
        if (MPG123_NEW_FORMAT == ret)
        {
            long sampleRate;
            int numChannels, enc;
            mpg123_getformat(m_mp2DecoderHandle, &sampleRate, &numChannels, &enc);

            qDebug("New MP2 format: %ld Hz, %d channels, encoding %d", sampleRate, numChannels, enc);

//            // reset FIFO
//            m_outFifoPtr->reset();
//            m_outFifoPtr->numChannels = uint8_t(channels);
//            m_outFifoPtr->sampleRate = uint32_t(rate);

//            if (PlaybackState::Running == m_playbackState)
//            {
//                qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> switch";
//            }
//            else
//            {
//                qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> start";
//            }

//            emit startAudio(m_outFifoPtr);

//            m_playbackState = PlaybackState::Running;

            setOutput(sampleRate, numChannels);

            getFormatMP2();

            m_mp2DRC = 0;
        }

        // lets store data to FIFO
        m_outFifoPtr->mutex.lock();
        uint64_t count = m_outFifoPtr->count;
        while ((AUDIO_FIFO_SIZE - count) < size)
        {
            m_outFifoPtr->countChanged.wait(&m_outFifoPtr->mutex);
            count = m_outFifoPtr->count;
        }
        m_outFifoPtr->mutex.unlock();

        // we know that size is available in buffer
        uint64_t bytesToEnd = AUDIO_FIFO_SIZE - m_outFifoPtr->head;

#if MP2_DRC_ENABLE
        if (m_mp2DRC != 0)
        {   // multiply buffer by gain
            float gain = pow(10, m_mp2DRC * 0.0125);    // 0.0125 = 1/(4*20)
            //qDebug("DRC = %fdB => %f", mp2DRC*0.25, gain);
            for (int n = 0; n < int (size >> 1); ++n)     // considering int16_t data => 2 bytes
            {   // multiply all samples by gain
                outBuf[n] = int16_t(qRound(outBuf[n] * gain));
            }
        }
#endif // MP2_DRC_ENABLE
        if (bytesToEnd < size)
        {
            memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, outBuf, bytesToEnd);
            memcpy(m_outFifoPtr->buffer, outBuf + bytesToEnd, size - bytesToEnd);
            m_outFifoPtr->head = (size - bytesToEnd);
        }
        else
        {
            memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, outBuf, size);
            m_outFifoPtr->head += size;
        }

        m_outFifoPtr->mutex.lock();
        m_outFifoPtr->count += size;
        m_outFifoPtr->mutex.unlock();

        // there should be nothing more to decode, but try to be sure
        while (ret != MPG123_ERR && ret != MPG123_NEED_MORE)
        {   // Get all decoded audio that is available now before feeding more input
            ret = mpg123_decode(m_mp2DecoderHandle, NULL, 0, outBuf, MP2_FRAME_PCM_SAMPLES * sizeof(int16_t), &size);

            if (0 == size)
            {
                break;
            }

            bytesToEnd = AUDIO_FIFO_SIZE - m_outFifoPtr->head;
            if (bytesToEnd < size)
            {
                memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, outBuf, bytesToEnd);
                memcpy(m_outFifoPtr->buffer, outBuf + bytesToEnd, size - bytesToEnd);
                m_outFifoPtr->head = (size - bytesToEnd);
            }
            else
            {
                memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, outBuf, size);
                m_outFifoPtr->head += size;
            }

            m_outFifoPtr->mutex.lock();
            m_outFifoPtr->count += size;
            m_outFifoPtr->mutex.unlock();
        }

        delete [] outBuf;
    }
    // store DRC for next frame
    m_mp2DRC = inData->at(1);
}

void AudioDecoder::getFormatMP2()
{
    mpg123_frameinfo info;
    int res = mpg123_info(m_mp2DecoderHandle, &info);

    if (MPG123_OK != res)
    {
        qDebug("%s: error while mpg123_info: %s", Q_FUNC_INFO, mpg123_plain_strerror(res));
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
    emit audioParametersInfo(m_audioParameters);
}

void AudioDecoder::processAAC(QByteArray *inData)
{      
    dabsdrAudioFrameHeader_t header;

    header.raw = inData->at(1);

    if (nullptr == m_aacDecoderHandle)
    {   // this can happen when format changes from MP2 to AAC or during init
        m_aacHeader.raw = header.raw;
        readAACHeader();
        initAACDecoder();        

        // reset FIFO
        m_outFifoPtr->reset();

        m_playbackState = PlaybackState::Running;
    }

    uint8_t conceal = header.bits.conceal;
    if (conceal)
    {
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
        inData->fill(0);
        header.raw = m_aacHeader.raw;
#endif
    }    

    if (header.raw != m_aacHeader.raw)
    {   // this is stream reconfiguration
        m_aacHeader.raw = header.raw;
        readAACHeader();
        initAACDecoder();

        // reset FIFO
        m_outFifoPtr->reset();
    }

    // decode audio
#if HAVE_FDKAAC
    uint8_t * aacData[1] = { (uint8_t *)inData->data() + 2 };
    unsigned int len[1];
    len[0] = inData->size() - 2;
    unsigned int bytesValid = len[0];

    // fill internal input buffer
    AAC_DECODER_ERROR result = aacDecoder_Fill(m_aacDecoderHandle, aacData, len, &bytesValid);

#ifdef AUDIO_DECODER_AAC_OUT
    writeAACOutput((char *)aacData[0], len[0]);
#endif

    if (result != AAC_DEC_OK)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_Fill: " + std::to_string(result));
    }
    if (bytesValid)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": aacDecoder_Fill did not consume all bytes");
    }

    // decode audio
    result = aacDecoder_DecodeFrame(m_aacDecoderHandle, (INT_PCM *)m_outputFrame, m_outputFrameLen / sizeof(INT_PCM), AACDEC_CONCEAL * conceal);
    if (AAC_DEC_OK != result)
    {
        qDebug() << "Error decoding AAC frame: " << result;
    }

    //qDebug("error = %d | samples = %d", !IS_OUTPUT_VALID(result), m_outputFrameLen / sizeof(INT_PCM));

    if (!IS_OUTPUT_VALID(result))
    {   // no output
        return;
    }

#ifdef AUDIO_DECODER_RAW_OUT
    if (m_rawOut)
    {
        fwrite(m_outputFrame, 1, m_outputFrameLen, m_rawOut);
    }
#endif

    int64_t bytesToWrite = m_outputFrameLen;

    // wait for space in ouput buffer
    m_outFifoPtr->mutex.lock();
    uint64_t count = m_outFifoPtr->count;
    while (int64_t(AUDIO_FIFO_SIZE - count) < bytesToWrite)
    {
        //qDebug("%s %lld: %lld < %lld", Q_FUNC_INFO, AUDIO_FIFO_SIZE, int64_t(AUDIO_FIFO_SIZE - count), bytesToWrite);
        m_outFifoPtr->countChanged.wait(&m_outFifoPtr->mutex);
        count = m_outFifoPtr->count;
    }
    m_outFifoPtr->mutex.unlock();

    int64_t bytesToEnd = AUDIO_FIFO_SIZE - m_outFifoPtr->head;
    if (bytesToEnd < bytesToWrite)
    {
        memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, m_outputFrame, bytesToEnd);
        memcpy(m_outFifoPtr->buffer, m_outputFrame + bytesToEnd, bytesToWrite - bytesToEnd);
        m_outFifoPtr->head = bytesToWrite - bytesToEnd;
    }
    else
    {
        memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, m_outputFrame, bytesToWrite);
        m_outFifoPtr->head += bytesToWrite;
    }

    m_outFifoPtr->mutex.lock();
    m_outFifoPtr->count += bytesToWrite;
    m_outFifoPtr->mutex.unlock();
#else // HAVE_FDKAAC
    const char * aacData = inData->data() + 2;
    unsigned long len = inData->size() - 2;

    uint8_t * outputFrame = (uint8_t *)NeAACDecDecode(m_aacDecoderHandle, &m_aacDecFrameInfo, (unsigned char *)aacData, len);

#ifdef AUDIO_DECODER_AAC_OUT
    writeAACOutput(aacData, len);
#endif
    handleAudioOutputFAAD(m_aacDecFrameInfo, outputFrame);
#endif // HAVE_FDKAAC
}

#if !HAVE_FDKAAC
void AudioDecoder::handleAudioOutputFAAD(const NeAACDecFrameInfo&frameInfo, const uint8_t *inFramePtr)
{
#if AUDIO_DECODER_MUTE_CONCEALMENT
    if (frameInfo.samples != m_outputBufferSamples)
    {
        if (OutputState::Unmuted == m_state)
        {   // do mute
            //qDebug() << Q_FUNC_INFO << "muting, DS =" << muteRampDsFactor;
            int16_t * dataPtr = &m_outBufferPtr[m_outputBufferSamples - 1];  // last sample
            std::vector <float>::const_iterator it = m_muteRamp.cbegin();
            while (it != m_muteRamp.end())
            {
                float gain = *it;
                it += m_muteRampDsFactor;
                for (uint_fast32_t ch = 0; ch < m_numChannels; ++ch)
                {
                    *dataPtr = int16_t(qRound(gain * *dataPtr));
                    --dataPtr;
                }
            }
        }
        else if (OutputState::Init == m_state)
        {   // do nothing and return -> decoder is initializing
            return;
        }
        else // muted
        { /* do nothing */ }
    }

    if (OutputState::Init == m_state)
    {   // only copy to internal buffer -> this is the first buffer
        memcpy(m_outBufferPtr, inFramePtr, m_outputBufferSamples * sizeof(int16_t));
        m_state = OutputState::Unmuted;
        return;
    }

    // copy data to output FIFO
    int64_t bytesToWrite = m_outputBufferSamples * sizeof(int16_t);
#else
    if (frameInfo.error)
    {
        qDebug() << "AAC error" << NeAACDecGetErrorMessage(frameInfo.error);
    }

    if (frameInfo.bytesconsumed == 0 && frameInfo.samples == 0)
    {   // no output
        return;
    }

    int64_t bytesToWrite = frameInfo.samples * sizeof(int16_t);

    const uint8_t * outBufferPtr = inFramePtr;
#endif

#ifdef AUDIO_DECODER_RAW_OUT
    if (m_rawOut)
    {
        fwrite(m_outBufferPtr, 1, bytesToWrite, m_rawOut);
    }
#endif

    // wait for space in ouput buffer
    m_outFifoPtr->mutex.lock();
    uint64_t count = m_outFifoPtr->count;
    while (int64_t(AUDIO_FIFO_SIZE - count) < bytesToWrite)
    {
        //qDebug("%s %lld: %lld < %lld", Q_FUNC_INFO, AUDIO_FIFO_SIZE, int64_t(AUDIO_FIFO_SIZE - count), bytesToWrite);
        m_outFifoPtr->countChanged.wait(&m_outFifoPtr->mutex);
        count = m_outFifoPtr->count;
    }
    m_outFifoPtr->mutex.unlock();

    int64_t bytesToEnd = AUDIO_FIFO_SIZE - m_outFifoPtr->head;
    if (bytesToEnd < bytesToWrite)
    {
        memcpy(m_outFifoPtr->buffer + m_outFifoPtr->head, m_outBufferPtr, bytesToEnd);
        memcpy(m_outFifoPtr->buffer, m_outBufferPtr + bytesToEnd, bytesToWrite - bytesToEnd);
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

#if AUDIO_DECODER_MUTE_CONCEALMENT
    // copy new data to buffer
    if (frameInfo.samples != m_outputBufferSamples)
    {   // error
        if (OutputState::Unmuted == m_state)
        {   // copy 0
            memset(m_outBufferPtr, 0, m_outputBufferSamples * sizeof(int16_t));
            m_state = OutputState::Muted;
        }
    }
    else
    {   // OK
        memcpy(m_outBufferPtr, inFramePtr, m_outputBufferSamples * sizeof(int16_t));
        m_state = OutputState::Unmuted;
    }
#endif
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
    {   // switch audio source
        //qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> switch: idx =" << m_outFifoIdx;
        emit switchAudio(m_outFifoPtr);

    }
    else
    {   // start audio
        //qDebug() << Q_FUNC_INFO << static_cast<int>(m_playbackState) << "==> start: idx =" << m_outFifoIdx;
        m_playbackState = PlaybackState::Running;
        emit startAudio(m_outFifoPtr);
    }
}

#endif // HAVE_FDKAAC

#ifdef AUDIO_DECODER_AAC_OUT
void AudioDecoder::writeAACOutput(const char *data, uint16_t dataLen)
{
    uint8_t adts_sfreqidx;
    uint8_t audioFs;

    if ((m_aacHeader.bits.dac_rate == 0) && (m_aacHeader.bits.sbr_flag == 1))
    {
        audioFs = 16 * 2;
        adts_sfreqidx = 0x8;  // 16 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 1))
    {
        audioFs = 24 * 2;
        adts_sfreqidx = 0x6;  // 24 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 0) && (m_aacHeader.bits.sbr_flag == 0))
    {
        audioFs = 32;
        adts_sfreqidx = 0x5;  // 32 kHz
    }
    if ((m_aacHeader.bits.dac_rate == 1) && (m_aacHeader.bits.sbr_flag == 0))
    {
        audioFs = 48;
        adts_sfreqidx = 0x3;  // 48 kHz
    }
    uint16_t au_size = dataLen;

    uint8_t aac_header[20];

    // [11 bits] 0x2B7 syncword
    aac_header[0] = 0x2B7 >> 3;          // (8 bits)
    aac_header[1] = (0x2B7 << 5) & 0xE0; // (3 bits)
    // [13 bits] audioMuxLengthBytes - written later
    // aac_header[1]                   // (5 bits)
    aac_header[2] = 0;                 // (8 bits)

    // [1 bit] useSameStreamMux = 0
    // [1 bit] audioMuxVersion = 0
    // [1 bit] allStreamsSameTimeFraming = 1
    // [6 bits] numSubFrames = 000000
    // [4 bits] numProgram  = 0000
    // [3 bits] numLayer = 000
    aac_header[3] = 0b00100000;
    aac_header[4] = 0b00000000;

    uint8_t * aac_header_ptr = &aac_header[5];
    uint_fast8_t sbr_flag = m_aacHeader.bits.sbr_flag;

    if (sbr_flag)
    {
        // [5 bits] // SBR
        *aac_header_ptr = 0b00101 << 3;   // SBR
        // [4 bits] sampling freq index
        *aac_header_ptr++ |= (adts_sfreqidx >> 1) & 0x7;
        *aac_header_ptr  = (adts_sfreqidx & 0x1) << 7;
        // [4 bits] channel configuration
        *aac_header_ptr |= (m_aacHeader.bits.aac_channel_mode + 1) << 3;
        // [4 bits] extension sample rate index = 3 or 5
        *aac_header_ptr++ |= (5 - 2 * m_aacHeader.bits.dac_rate) >> 1;
        *aac_header_ptr  = ((5 - 2 * m_aacHeader.bits.dac_rate) & 0x1) << 7;
        // [5 bits] AAC LC
        *aac_header_ptr |= 0b00010 << 2;
        // [3 bits] GASpecificConfig() with 960 transform = 0b100
        *aac_header_ptr++ |= 0b10;
        //aac_header[8]  = 0b0 << 7;
        // [3 bits] frameLengthType = 0b000
        *aac_header_ptr = 0b000 << 4;
        // [8 bits] latmBufferFullness = 0xFF
        *aac_header_ptr++ |= 0x0F;
        *aac_header_ptr  = 0xF0;
        // [1 bit] otherDataPresent = 0
        // [1 bit] crcCheckPresent = 0;

        // 2 bits remainig
    }
    else
    {
        // [5 bits] AAC LC
        *aac_header_ptr = 0b00010 << 3;
        // [4 bits] samplingFrequencyIndex
        *aac_header_ptr++ |= (adts_sfreqidx >> 1) & 0x7;
        *aac_header_ptr  = (adts_sfreqidx & 0x1) << 7;
        // [4 bits] channel configuration
        *aac_header_ptr |= (m_aacHeader.bits.aac_channel_mode + 1) << 3;
        // [3 bits] GASpecificConfig() with 960 transform = 0b100
        *aac_header_ptr++ |= 0b100;
        // [3 bits] frameLengthType = 0b000
        // aac_header[7] = 0b000 << 5;
        // [8 bits] latmBufferFullness = 0xFF
        *aac_header_ptr++ = 0x1F;
        *aac_header_ptr = 0xE0;
        // [1 bit] otherDataPresent = 0
        // [1 bit] crcCheckPresent = 0;

        // 3 bits remainig
    }
    //	PayloadLengthInfo()
    // 0xFF for each 255 bytes
    int au_size_255 = au_size / 255;

    for (int i = 0; i < au_size_255; i++)
    {
        *aac_header_ptr++ |= (uint8_t)(0xFF >> (5 + sbr_flag));
        *aac_header_ptr = (uint8_t)(0xFF << (3 - sbr_flag));
    }
    // the rest is (au_size % 255);
    *aac_header_ptr++ |= (au_size % 255) >> (5 + sbr_flag);
    *aac_header_ptr = (au_size % 255) << (3 - sbr_flag);

    // total length = total size - 3 (length is in byte 1-2, thus -3 bytes)
    int len = 9 + m_aacHeader.bits.sbr_flag + au_size_255 + 1 + au_size - 3;

    aac_header[1] |= (len >> 8) & 0x1F;
    aac_header[2] = len & 0xFF;

    fwrite(aac_header, sizeof(uint8_t), 9 + m_aacHeader.bits.sbr_flag + au_size_255, m_aacOut);

    uint8_t byte = *aac_header_ptr;
    const uint8_t * auPtr = (const uint8_t *)data; //&buffer[mscDataPtr->au_start[r]];

    for (int i = 0; i < au_size; ++i)
    {
        byte |= (*auPtr) >> (5 + sbr_flag);
        fwrite(&byte, sizeof(uint8_t), 1, m_aacOut);
        byte = (uint8_t)*auPtr++ << (3 - sbr_flag);
    }
    fwrite(&byte, sizeof(uint8_t), 1, m_aacOut);
}

#endif

#ifdef AUDIO_DECODER_MP2_OUT
void AudioDecoder::writeMP2Output(const char *data, uint16_t dataLen)
{
    fwrite(data, sizeof(uint8_t), dataLen, m_mp2Out);
}

#endif
