#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>
#include <neaacdec.h>
#include <mpg123.h>
#include "radiocontrol.h"
#include "audiofifo.h"

//#define AUDIO_DECODER_RAW_OUT
//#define AUDIO_DECODER_AAC_OUT
//#define AUDIO_DECODER_MP2_OUT

typedef union
{
    uint8_t raw;
    struct
    {
        uint8_t mpeg_surr_cfg : 3;  // [A decoder that does not support MPEG Surround shall ignore this parameter.]
        uint8_t ps_flag : 1;
        uint8_t aac_channel_mode : 1;
        uint8_t sbr_flag : 1;
        uint8_t dac_rate : 1;
    } bits;
} audiodecAacHeader_t;

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
    void start(const DabAudioMode & m);
    void stop();
    void inputData(QByteArray * inData);

signals:
    void startAudio(uint32_t sampleRate, uint8_t numChannels);
    void stopAudio();
    void audioParametersInfo(const struct AudioParameters & params);
private:
    bool isRunning;
    DabAudioMode mode;
    audiodecAacHeader_t aacHeader;
    AudioParameters audioParameters;

    NeAACDecHandle aacDecoderHandle;
    NeAACDecFrameInfo aacDecFrameInfo;

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

    void initFAAD();    
    void processAAC(QByteArray *inData);

    void initMPG123();
    void processMP2(QByteArray *inData);
    void getFormatMP2();
};

#endif // AUDIODECODER_H
