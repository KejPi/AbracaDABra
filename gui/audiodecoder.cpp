#include <QDebug>
#include <QFile>
#include <math.h>

#include "audiodecoder.h"
#include "audiooutput.h"

AudioDecoder::AudioDecoder(audioFifo_t * buffer, QObject *parent) : QObject(parent), outFifoPtr(buffer)
{
    aacDecoderHandle = nullptr;    
    mp2DecoderHandle = nullptr;    

    isRunning = false;

#ifdef AUDIO_DECODER_RAW_OUT
    rawOut = fopen("audio.raw", "wb");
    if (!rawOut)
    {
        qDebug("Unable to open file: audio.raw");
    }
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    aacOut = fopen("audio.aac", "wb");
    if (!aacOut)
    {
        qDebug("Unable to open file: audio.aac");
    }
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    mp2Out = fopen("audio.mp2", "wb");
    if (!mp2Out)
    {
        qDebug("Unable to open file: audio.mp2");
    }
#endif
}

AudioDecoder::~AudioDecoder()
{
    if (nullptr != aacDecoderHandle)
    {
#if (AUDIO_DECODER_USE_FDKAAC)
        aacDecoder_Close(aacDecoderHandle);
        if (nullptr != outputFrame)
        {
            delete [] outputFrame;
        }
#else
        NeAACDecClose(aacDecoderHandle);
#endif
    }
    if (nullptr != mp2DecoderHandle)
    {
        int res = mpg123_close(mp2DecoderHandle);
        if(MPG123_OK != res)
        {
            qDebug("%s: error while mpg123_close: %s\n", Q_FUNC_INFO, mpg123_plain_strerror(res));
        }

        mpg123_delete(mp2DecoderHandle);
        mpg123_exit();
    }
#ifdef AUDIO_DECODER_RAW_OUT
    if (rawOut)
    {
        fclose(rawOut);
    }
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    if (aacOut)
    {
        fclose(aacOut);
    }
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    if (mp2Out)
    {
        fclose(mp2Out);
    }
#endif
}

void AudioDecoder::start(const RadioControlServiceComponent &s)
{
    qDebug() << Q_FUNC_INFO;
    if (s.isAudioService())
    {
        isRunning = true;
        mode = s.streamAudioData.scType;
    }
}

void AudioDecoder::stop()
{
    qDebug() << Q_FUNC_INFO;
    isRunning = false;

    if (nullptr != aacDecoderHandle)
    {
#if (AUDIO_DECODER_USE_FDKAAC)
        aacDecoder_Close(aacDecoderHandle);
#else
        NeAACDecClose(aacDecoderHandle);
#endif
        aacDecoderHandle = nullptr;
    }

    if (nullptr != mp2DecoderHandle)
    {
        int res = mpg123_close(mp2DecoderHandle);
        if(MPG123_OK != res)
        {
            qDebug("%s: error while mpg123_close: %s\n", Q_FUNC_INFO, mpg123_plain_strerror(res));
        }

        mpg123_delete(mp2DecoderHandle);
        mpg123_exit();
        mp2DecoderHandle = nullptr;
    }
    emit stopAudio();
}

void AudioDecoder::initMPG123()
{
    if (nullptr == mp2DecoderHandle)
    {
        int res = mpg123_init();
        if (MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_init");
        }

        mp2DecoderHandle = mpg123_new(nullptr, &res);
        if(nullptr == mp2DecoderHandle)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_new: " + std::string(mpg123_plain_strerror(res)));
        }

        // set allowed formats
        res = mpg123_format_none(mp2DecoderHandle);
        if(MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format_none: " + std::string(mpg123_plain_strerror(res)));
        }

#ifdef AUDIOOUTPUT_USE_RTAUDIO
        res = mpg123_format(mp2DecoderHandle, 48000, MPG123_STEREO, MPG123_ENC_SIGNED_16);
#else
        res = mpg123_format(mp2DecoderHandle, 48000, MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16);
#endif
        if(MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format for 48KHz: " + std::string(mpg123_plain_strerror(res)));
        }

#ifdef AUDIOOUTPUT_USE_RTAUDIO
        res = mpg123_format(mp2DecoderHandle, 24000, MPG123_STEREO, MPG123_ENC_SIGNED_16);
#else
        res = mpg123_format(mp2DecoderHandle, 24000, MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16);
#endif
        if(MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_format for 24KHz: " + std::string(mpg123_plain_strerror(res)));
        }        

        // disable resync limit
        res = mpg123_param(mp2DecoderHandle, MPG123_RESYNC_LIMIT, -1, 0);
        if(MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_param: " + std::string(mpg123_plain_strerror(res)));
        }

        res = mpg123_open_feed(mp2DecoderHandle);
        if(MPG123_OK != res)
        {
            throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while mpg123_open_feed: " + std::string(mpg123_plain_strerror(res)));
        }
    }
}

void AudioDecoder::readAACHeader(const uint8_t header)
{
    aacHeader.raw = header;

    // fill the structure used to signal audio params to HMI
    if (aacHeader.bits.sbr_flag)
    {
        if (aacHeader.bits.ps_flag)
        {
            audioParameters.coding = AudioCoding::HEAACv2;
        }
        else
        {
            audioParameters.coding = AudioCoding::HEAAC;
        }
    }
    else
    {
        audioParameters.coding = AudioCoding::AACLC;
    }
    if (aacHeader.bits.aac_channel_mode || aacHeader.bits.ps_flag)
    {
        audioParameters.stereo = true;
    }
    else
    {
        audioParameters.stereo = false;
    }
    audioParameters.sampleRateKHz = (aacHeader.bits.dac_rate) ? (48) : (32);
    audioParameters.parametricStereo = aacHeader.bits.ps_flag;
    audioParameters.sbr = aacHeader.bits.sbr_flag;

    emit audioParametersInfo(audioParameters);

    qDebug("%s %d kHz %s",
           (aacHeader.bits.sbr_flag ? (aacHeader.bits.ps_flag ? "HE-AAC v2" : "HE-AAC") : "AAC-LC"),
           (aacHeader.bits.dac_rate ? 48 : 32),
           (aacHeader.bits.aac_channel_mode || aacHeader.bits.ps_flag ? "stereo" : "mono")
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
    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 1))
    {
        //audioFs = 16*2;
        coreSrIndex = 0x8;  // 16 kHz
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 1))
    {
        //audioFs = 24*2;
        coreSrIndex = 0x6;  // 24 kHz
    }
    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 0))
    {
        //audioFs = 32;
        coreSrIndex = 0x5;  // 32 kHz
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 0))
    {
        //audioFs = 48;
        coreSrIndex = 0x3;  // 48 kHz
    }

    uint8_t coreChannelConfig = aacHeader.bits.aac_channel_mode+1;
    uint8_t extensionSrIndex = 5 - 2*aacHeader.bits.dac_rate; // dac_rate ? 3 : 5

    // AAC LC
    ascLen = 0;
    asc[ascLen++] = 0b00010 << 3 | coreSrIndex >> 1;
    asc[ascLen++] = (coreSrIndex & 0x01) << 7 | coreChannelConfig << 3 | 0b100;

    if(aacHeader.bits.sbr_flag) {
        // add SBR
        asc[ascLen++] = 0x56;
        asc[ascLen++] = 0xE5;
        asc[ascLen++] = 0x80 | (extensionSrIndex << 3);

        if(aacHeader.bits.ps_flag) {
            // add PS
            asc[ascLen - 1] |= 0x05;
            asc[ascLen++] = 0x48;
            asc[ascLen++] = 0x80;
        }
    }
}

#if (AUDIO_DECODER_USE_FDKAAC)
void AudioDecoder::initAACDecoder()
{
    if (nullptr != aacDecoderHandle)
    {
        aacDecoder_Close(aacDecoderHandle);
    }

    aacDecoderHandle = aacDecoder_Open(TT_MP4_RAW, 1);
    if(!aacDecoderHandle)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_Open");
    }

    // init decoder
    int channels = audioParameters.stereo ? 2 : 1;
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
    init_result = aacDecoder_SetParam(aacDecoderHandle, AAC_PCM_OUTPUT_CHANNELS, channels);
    if(AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#else
    init_result = aacDecoder_SetParam(aacDecoderHandle, AAC_PCM_MIN_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_MIN_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
    init_result = aacDecoder_SetParam(aacDecoderHandle, AAC_PCM_MAX_OUTPUT_CHANNELS, channels);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while setting parameter AAC_PCM_MAX_OUTPUT_CHANNELS: " + std::to_string(init_result));
    }
#endif

    uint8_t* asc_array[1] {asc};
    const unsigned int asc_sizeof_array[1] {(unsigned int) ascLen};
    init_result = aacDecoder_ConfigRaw(aacDecoderHandle, asc_array, asc_sizeof_array);
    if (AAC_DEC_OK != init_result)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_ConfigRaw: " + std::to_string(init_result));
    }

    outputFrameLen = 960 * sizeof(INT_PCM) * channels * (aacHeader.bits.sbr_flag ? 2 : 1);

    if (nullptr != outputFrame)
    {
        delete [] outputFrame;
    }
    outputFrame = new uint8_t[outputFrameLen];

    qDebug("Output SR = %d, channels = %d", aacHeader.bits.dac_rate ? 48000 : 32000, channels);

    emit startAudio(uint32_t(aacHeader.bits.dac_rate ? 48000 : 32000), uint8_t(channels));

}
#else // (AUDIO_DECODER_USE_FDKAAC)
void AudioDecoder::initAACDecoder()
{
    if (nullptr != aacDecoderHandle)
    {
        NeAACDecClose(aacDecoderHandle);
    }

    aacDecoderHandle = NeAACDecOpen();
    if(!aacDecoderHandle)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecOpen");
    }

    // ensure features
    unsigned long cap = NeAACDecGetCapabilities();
    if(!(cap & LC_DEC_CAP))
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": no LC decoding support!");
    }

    // set general config
    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(aacDecoderHandle);
    if(!config)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecGetCurrentConfiguration");
    }

    config->outputFormat = FAAD_FMT_16BIT;
    config->dontUpSampleImplicitSBR = 0;
    config->downMatrix = 1;

    if(NeAACDecSetConfiguration(aacDecoderHandle, config) != 1)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while NeAACDecSetConfiguration");
    }

    // init decoder
    unsigned long outputSr;
    unsigned char outputCh;
    long int init_result = NeAACDecInit2(aacDecoderHandle, asc, (unsigned long) ascLen, &outputSr, &outputCh);
    if(init_result != 0)
    {
        throw std::runtime_error("AACDecoderFAAD2: error while NeAACDecInit2: " + std::string(NeAACDecGetErrorMessage(-init_result)));
    }

    qDebug("Output SR = %lu, channels = %d", outputSr, outputCh);

    emit startAudio(uint32_t(outputSr), uint8_t(outputCh));
}
#endif // (AUDIO_DECODER_USE_FDKAAC)

void AudioDecoder::inputData(QByteArray *inData)
{
    if (!isRunning)
    {   // do nothing if not running
        delete inData;  // free input buffer
        return;
    }

    switch (mode)
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

void AudioDecoder::processMP2(QByteArray *inData)
{
#define MP2_FRAME_PCM_SAMPLES (2*1152)
#define MP2_DRC_ENABLE        1

    if (nullptr == mp2DecoderHandle)
    {
        initMPG123();

        // reset FIFO
        outFifoPtr->reset();
    }

#ifdef AUDIO_DECODER_MP2_OUT
    writeMP2Output(inData->data()+1, inData->size()-1);
#endif

    if (inData->size() > 1)
    {
        // input data
        const char * mp2Data = inData->data()+1;

        // [ETSI TS 103 466 V1.2.1 (2019-09)]
        // 5.2.9 Formatting of the audio bit stream
        // It shall further divide this bit stream into audio frames, each corresponding to 1152 PCM audio samples,
        // which is equivalent to a duration of 24 ms in the case of 48 kHz sampling frequency
        // and 48 ms in the case of 24 kHz sampling frequency.
        int16_t * outBuf = new int16_t[MP2_FRAME_PCM_SAMPLES];

        /* Feed input chunk and get first chunk of decoded audio. */
        size_t size;
        int ret = mpg123_decode(mp2DecoderHandle, (const unsigned char *)mp2Data, (inData->size() - 1), outBuf, MP2_FRAME_PCM_SAMPLES*sizeof(int16_t), &size);
        if(MPG123_NEW_FORMAT == ret)
        {
            long rate;
            int channels, enc;
            mpg123_getformat(mp2DecoderHandle, &rate, &channels, &enc);

            qDebug("New MP2 format: %ld Hz, %d channels, encoding %d", rate, channels, enc);

            emit startAudio(uint32_t(rate), uint8_t(channels));

            getFormatMP2();

            mp2DRC = 0;
        }

        // lets store data to FIFO
        outFifoPtr->mutex.lock();
        uint64_t count = outFifoPtr->count;
        while ((AUDIO_FIFO_SIZE - count) < size)
        {
            outFifoPtr->countChanged.wait(&outFifoPtr->mutex);
            count = outFifoPtr->count;
        }
        outFifoPtr->mutex.unlock();

        // we know that size is available in buffer
        uint64_t bytesToEnd = AUDIO_FIFO_SIZE - outFifoPtr->head;

#if MP2_DRC_ENABLE
        if (mp2DRC != 0)
        {   // multiply buffer by gain
            float gain = pow(10, mp2DRC * 0.0125);    // 0.0125 = 1/(4*20)
            //qDebug("DRC = %fdB => %f", mp2DRC*0.25, gain);
            for (int n = 0; n< int(size>>1); ++n)     // considering int16_t data => 2 bytes
            {   // multiply all samples by gain
                outBuf[n] = int16_t(qRound(outBuf[n] * gain));
            }
        }
#endif // MP2_DRC_ENABLE
        if (bytesToEnd < size)
        {
            memcpy(outFifoPtr->buffer+outFifoPtr->head, outBuf, bytesToEnd);
            memcpy(outFifoPtr->buffer, outBuf+bytesToEnd, size - bytesToEnd);
            outFifoPtr->head = (size - bytesToEnd);
        }
        else
        {
            memcpy(outFifoPtr->buffer+outFifoPtr->head, outBuf, size);
            outFifoPtr->head += size;
        }

        outFifoPtr->mutex.lock();
        outFifoPtr->count += size;
        outFifoPtr->countChanged.wakeAll();
        outFifoPtr->mutex.unlock();

        // there should be nothing more to decode, but try to be sure
        while(ret != MPG123_ERR && ret != MPG123_NEED_MORE)
        {   // Get all decoded audio that is available now before feeding more input
            ret = mpg123_decode(mp2DecoderHandle, NULL, 0, outBuf, MP2_FRAME_PCM_SAMPLES*sizeof(int16_t), &size);

            if (0 == size)
            {
                break;
            }

            bytesToEnd = AUDIO_FIFO_SIZE - outFifoPtr->head;
            if (bytesToEnd < size)
            {
                memcpy(outFifoPtr->buffer+outFifoPtr->head, outBuf, bytesToEnd);
                memcpy(outFifoPtr->buffer, outBuf+bytesToEnd, size - bytesToEnd);
                outFifoPtr->head = (size - bytesToEnd);
            }
            else
            {
                memcpy(outFifoPtr->buffer+outFifoPtr->head, outBuf, size);
                outFifoPtr->head += size;
            }

            outFifoPtr->mutex.lock();
            outFifoPtr->count += size;
            outFifoPtr->countChanged.wakeAll();
            outFifoPtr->mutex.unlock();
        }

        delete [] outBuf;
    }
    // store DRC for next frame
    mp2DRC = inData->at(0);
}


void AudioDecoder::getFormatMP2()
{
    mpg123_frameinfo info;
    int res = mpg123_info(mp2DecoderHandle, &info);
    if(MPG123_OK != res)
    {
        qDebug("%s: error while mpg123_info: %s", Q_FUNC_INFO, mpg123_plain_strerror(res));
    }

    audioParameters.coding = AudioCoding::MP2;
    audioParameters.parametricStereo = false;
    switch(info.mode) {
    case MPG123_M_STEREO:
        audioParameters.stereo = true;
        audioParameters.sbr = false;
        break;
    case MPG123_M_JOINT:
        audioParameters.stereo = true;
        audioParameters.sbr = true;
        break;
    case MPG123_M_DUAL:
        // this should not happen -> not supported by DAB
        audioParameters.stereo = true;
        audioParameters.sbr = false;
        break;
    case MPG123_M_MONO:
        audioParameters.stereo = false;
        audioParameters.sbr = false;
        break;
    }

    audioParameters.sampleRateKHz = info.rate/1000;
    emit audioParametersInfo(audioParameters);
}

void AudioDecoder::processAAC(QByteArray *inData)
{
    dabAudioFrameHeader_t header;
    header.raw = *inData[0];
    if (header.bits.conceal)
    {   // concealment not supported => discarding
        return;
    }
    if ((nullptr == aacDecoderHandle) || (header.raw != aacHeader.raw))
    {
        readAACHeader(header.raw);
        initAACDecoder();

        // reset FIFO
        outFifoPtr->reset();
    }

    // decode audio    
#if (AUDIO_DECODER_USE_FDKAAC)
    uint8_t * aacData[1] = {(uint8_t *) inData->data()+1};
    unsigned int len[1];
    len[0] = inData->size()-1;
    unsigned int bytesValid = len[0];

    // fill internal input buffer
    AAC_DECODER_ERROR result = aacDecoder_Fill(aacDecoderHandle, aacData, len, &bytesValid);

#ifdef AUDIO_DECODER_AAC_OUT
    writeAACOutput((char *) aacData[0], len[0]);
#endif

    if(result != AAC_DEC_OK)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": error while aacDecoder_Fill: " + std::to_string(result));
    }
    if (bytesValid)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": aacDecoder_Fill did not consume all bytes");
    }

    // decode audio
    result = aacDecoder_DecodeFrame(aacDecoderHandle, (INT_PCM*) outputFrame, outputFrameLen / sizeof(INT_PCM), 0);
    if(AAC_DEC_OK != result)
    {
        qDebug() << "Error decoding AAC frame: " << result;
    }

    if (!IS_OUTPUT_VALID(result))
    {   // no output
        return;
    }

#ifdef AUDIO_DECODER_RAW_OUT
    if (rawOut)
    {
        fwrite(outputFrame, 1, outputFrameLen, rawOut);
    }
#endif

    int64_t bytesToWrite = outputFrameLen;
#else // AUDIO_DECODER_USE_FDKAAC
    const char * aacData = inData->data()+1;
    unsigned long len = inData->size()-1;

    uint8_t* outputFrame = (uint8_t*) NeAACDecDecode(aacDecoderHandle, &aacDecFrameInfo, (unsigned char *) aacData, len);

#ifdef AUDIO_DECODER_AAC_OUT
    writeAACOutput(aacData, len);
#endif

    if(aacDecFrameInfo.error)
    {
        qDebug() << "AAC error" << NeAACDecGetErrorMessage(aacDecFrameInfo.error);
    }

    if(aacDecFrameInfo.bytesconsumed == 0 && aacDecFrameInfo.samples == 0)
    {   // no output
        return;
    }

    if(aacDecFrameInfo.bytesconsumed != len)
    {
        throw std::runtime_error(std::string(Q_FUNC_INFO) + ": NeAACDecDecode did not consume all bytes");
    }

#ifdef AUDIO_DECODER_RAW_OUT
    if (rawOut)
    {
        fwrite(outputFrame, sizeof(int16_t), aacDecFrameInfo.samples, rawOut);
    }
#endif

    int64_t bytesToWrite = aacDecFrameInfo.samples*sizeof(int16_t);
#endif // AUDIO_DECODER_USE_FDKAAC

    // wait for space in ouput buffer    
    outFifoPtr->mutex.lock();
    uint64_t count = outFifoPtr->count;
    while (int64_t(AUDIO_FIFO_SIZE - count) < bytesToWrite)
    {
        //qDebug("%s %lld: %lld < %lld", Q_FUNC_INFO, AUDIO_FIFO_SIZE, int64_t(AUDIO_FIFO_SIZE - count), bytesToWrite);
        outFifoPtr->countChanged.wait(&outFifoPtr->mutex);
        count = outFifoPtr->count;
    }
    outFifoPtr->mutex.unlock();

    int64_t bytesToEnd = AUDIO_FIFO_SIZE - outFifoPtr->head;
    if (bytesToEnd < bytesToWrite)
    {
        memcpy(outFifoPtr->buffer+outFifoPtr->head, outputFrame, bytesToEnd);
        memcpy(outFifoPtr->buffer, outputFrame+bytesToEnd, bytesToWrite-bytesToEnd);
        outFifoPtr->head = bytesToWrite-bytesToEnd;
    }
    else
    {
        memcpy(outFifoPtr->buffer+outFifoPtr->head, outputFrame, bytesToWrite);
        outFifoPtr->head += bytesToWrite;
    }

    outFifoPtr->mutex.lock();
    outFifoPtr->count += bytesToWrite;
#if (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    outFifoPtr->countChanged.wakeAll();
#endif
    outFifoPtr->mutex.unlock();
}

#ifdef AUDIO_DECODER_AAC_OUT
void AudioDecoder::writeAACOutput(const char *data, uint16_t dataLen)
{
    uint8_t adts_sfreqidx;
    uint8_t audioFs;

    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 1))
    {
        audioFs = 16*2;
        adts_sfreqidx = 0x8;  // 16 kHz
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 1))
    {
        audioFs = 24*2;
        adts_sfreqidx = 0x6;  // 24 kHz
    }
    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 0))
    {
        audioFs = 32;
        adts_sfreqidx = 0x5;  // 32 kHz
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 0))
    {
        audioFs = 48;
        adts_sfreqidx = 0x3;  // 48 kHz
    }
    uint16_t au_size = dataLen;

    uint8_t aac_header[20];

    // [11 bits] 0x2B7 syncword
    aac_header[0] = 0x2B7>>3;          // (8 bits)
    aac_header[1] = (0x2B7<<5) & 0xE0; // (3 bits)
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
    uint_fast8_t sbr_flag = aacHeader.bits.sbr_flag;
    if (sbr_flag)
    {
        // [5 bits] // SBR
        *aac_header_ptr = 0b00101 << 3;   // SBR
        // [4 bits] sampling freq index
        *aac_header_ptr++ |= (adts_sfreqidx >> 1) & 0x7;
        *aac_header_ptr  = (adts_sfreqidx & 0x1) << 7;
        // [4 bits] channel configuration
        *aac_header_ptr |= (aacHeader.bits.aac_channel_mode + 1) << 3;
        // [4 bits] extension sample rate index = 3 or 5
        *aac_header_ptr++ |= (5-2*aacHeader.bits.dac_rate) >> 1;
        *aac_header_ptr  = ((5-2*aacHeader.bits.dac_rate) & 0x1) << 7;
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
        *aac_header_ptr |= (aacHeader.bits.aac_channel_mode + 1) << 3;
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
    int au_size_255 = au_size/255;
    for (int i = 0; i < au_size_255; i++)
    {
        *aac_header_ptr++ |= (uint8_t)(0xFF >> (5+sbr_flag));
        *aac_header_ptr = (uint8_t)(0xFF << (3-sbr_flag));
    }
    // the rest is (au_size % 255);
    *aac_header_ptr++ |= (au_size % 255) >> (5+sbr_flag);
    *aac_header_ptr = (au_size % 255) << (3-sbr_flag);

    // total length = total size - 3 (length is in byte 1-2, thus -3 bytes)
    int len = 9 + aacHeader.bits.sbr_flag + au_size_255 + 1 + au_size - 3;
    aac_header[1] |= (len >> 8) & 0x1F;
    aac_header[2] = len & 0xFF;

    fwrite(aac_header, sizeof(uint8_t), 9 + aacHeader.bits.sbr_flag + au_size_255, aacOut);

    uint8_t byte = *aac_header_ptr;
    const uint8_t * auPtr = (const uint8_t*) data; //&buffer[mscDataPtr->au_start[r]];
    for (int i = 0; i < au_size; ++i)
    {
        byte |= (*auPtr) >> (5+sbr_flag);
        fwrite(&byte, sizeof(uint8_t), 1, aacOut);
        byte = (uint8_t) *auPtr++ << (3-sbr_flag);
    }
    fwrite(&byte, sizeof(uint8_t), 1, aacOut);
}
#endif

#ifdef AUDIO_DECODER_MP2_OUT
void AudioDecoder::writeMP2Output(const char *data, uint16_t dataLen)
{
    fwrite(data, sizeof(uint8_t), dataLen, mp2Out);
}
#endif
