#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>
#include <mpg123.h>
#include "config.h"
#if defined(HAVE_FDKAAC)
#include <fdk-aac/aacdecoder_lib.h>
#else
#include <neaacdec.h>
#endif // defined(HAVE_FDKAAC)
#include "radiocontrol.h"
#include "audiofifo.h"

//#define AUDIO_DECODER_RAW_OUT
//#define AUDIO_DECODER_AAC_OUT
//#define AUDIO_DECODER_MP2_OUT

// this is set from cmake
//#define HAVE_FDKAAC

#if defined(HAVE_FDKAAC)
#define AUDIO_DECODER_FDKAAC_CONCEALMENT 1

// leave it disabled, not supported for FDK-AAC yet
#define AUDIO_DECODER_MUTE_CONCEALMENT 0
#else
#define AUDIO_DECODER_MUTE_CONCEALMENT 1

#define AUDIO_DECODER_BUFFER_SIZE   3840  // this is maximum buffer size for HE-AAC
#define AUDIO_DECODER_FADE_TIME_MS    15
#endif

enum class AudioCoding
{
    MP2 = 0,
    AACLC,
    HEAAC,
    HEAACv2,
};

struct AudioParameters
{
    AudioCoding coding;
    int sampleRateKHz;
    bool stereo;
    bool parametricStereo;
    bool sbr;
};

Q_DECLARE_METATYPE(AudioParameters);

class AudioDecoder : public QObject
{
    Q_OBJECT
public:
    explicit AudioDecoder(audioFifo_t *buffer, QObject *parent = nullptr);
    ~AudioDecoder();

public slots:
    void start(const RadioControlServiceComponent &s);
    void stop();
    void inputData(QByteArray * inData);
    void getAudioParameters();

signals:
    void startAudio(uint32_t sampleRate, uint8_t numChannels);
    void stopAudio();
    void audioParametersInfo(const AudioParameters & params);
private:
    bool isRunning;
    DabAudioDataSCty mode;
    dabsdrAudioFrameHeader_t aacHeader;
    AudioParameters audioParameters;

#if defined(HAVE_FDKAAC)
    HANDLE_AACDECODER aacDecoderHandle;
    uint8_t *outputFrame = nullptr;
    size_t outputFrameLen;
#else
    NeAACDecHandle aacDecoderHandle;
    NeAACDecFrameInfo aacDecFrameInfo;
    void handleAudioOutputFAAD(const NeAACDecFrameInfo & frameInfo, const uint8_t * inFramePtr);
#endif
    int ascLen;
    uint8_t asc[7];

    float mp2DRC = 0;
    mpg123_handle * mp2DecoderHandle;

    audioFifo_t * outFifoPtr;

#if AUDIO_DECODER_MUTE_CONCEALMENT
    int16_t * outBufferPtr;
    size_t outBufferSamples;   
    int numChannels;
    int muteRampDsFactor;
    std::vector<float> muteRamp;
    enum class OutputState
    {
        Init = 0,
        Muted,
        Unmuted
    } state;
#endif

#ifdef AUDIO_DECODER_RAW_OUT
    FILE * rawOut;
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    FILE * aacOut;
    void writeAACOutput(const char *data, uint16_t dataLen);
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    FILE * mp2Out;
    void writeMP2Output(const char *data, uint16_t dataLen);
#endif

    void readAACHeader();

    void initAACDecoder();
    void processAAC(QByteArray *inData);

    void initMPG123();
    void processMP2(QByteArray *inData);
    void getFormatMP2();
};

#endif // AUDIODECODER_H
