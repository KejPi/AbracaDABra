#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>
#include <neaacdec.h>
#include <mpg123.h>
#include "radiocontrol.h"
#include "audiofifo.h"
#include <fdk-aac/aacdecoder_lib.h>

//#define AUDIO_DECODER_RAW_OUT
//#define AUDIO_DECODER_AAC_OUT
//#define AUDIO_DECODER_MP2_OUT

#define AUDIO_DECODER_USE_FDKAAC 0
#define AUDIO_DECODER_FDKAAC_CONCEALMENT 1


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

signals:
    void startAudio(uint32_t sampleRate, uint8_t numChannels);
    void stopAudio();
    void audioParametersInfo(const AudioParameters & params);
private:
    bool isRunning;
    DabAudioDataSCty mode;
    dabAudioFrameHeader_t aacHeader;
    AudioParameters audioParameters;

#if (AUDIO_DECODER_USE_FDKAAC)
    HANDLE_AACDECODER aacDecoderHandle;
    uint8_t *outputFrame = nullptr;
    size_t outputFrameLen;
#else
    NeAACDecHandle aacDecoderHandle;
    NeAACDecFrameInfo aacDecFrameInfo;
#endif
    int ascLen;
    uint8_t asc[7];

    float mp2DRC = 0;
    mpg123_handle * mp2DecoderHandle;

    audioFifo_t * outFifoPtr;

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

    void readAACHeader(const uint8_t header);

    void initAACDecoder();
    void processAAC(QByteArray *inData);

    void initMPG123();
    void processMP2(QByteArray *inData);
    void getFormatMP2();
};

#endif // AUDIODECODER_H
