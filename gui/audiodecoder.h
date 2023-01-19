#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <mpg123.h>

#include "config.h"

#if HAVE_FDKAAC
#include <fdk-aac/aacdecoder_lib.h>
#else
#include <neaacdec.h>

#endif // HAVE_FDKAAC

#include "radiocontrol.h"
#include "audiofifo.h"

#if HAVE_FDKAAC
#define AUDIO_DECODER_FDKAAC_CONCEALMENT 1
#define AUDIO_DECODER_NOISE_CONCEALMENT  0 // keep 0 here
#else // HAVE_FDKAAC
#define AUDIO_DECODER_BUFFER_SIZE     3840  // this is maximum buffer size for HE-AAC
#define AUDIO_DECODER_FADE_TIME_MS      15
#define AUDIO_DECODER_NOISE_CONCEALMENT  0  // 0 to disable or noise level (20,25,30,35,40)
#endif // HAVE_FDKAAC

// debug switches
//#define AUDIO_DECODER_RAW_OUT
//#define AUDIO_DECODER_AAC_OUT
//#define AUDIO_DECODER_MP2_OUT

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
    explicit AudioDecoder(QObject *parent = nullptr);
    ~AudioDecoder();

public slots:
    void start(const RadioControlServiceComponent &s);
    void stop();
    void decodeData(RadioControlAudioData *inData);
    void getAudioParameters();

signals:
    void startAudio(audioFifo_t *buffer);
    void switchAudio(audioFifo_t *buffer);
    void stopAudio();
    void audioParametersInfo(const AudioParameters & params);

private:
    enum class PlaybackState { Stopped = 0, WaitForInit, Running } m_playbackState;

    dabsdrAudioFrameHeader_t m_aacHeader;
    AudioParameters m_audioParameters;

#if HAVE_FDKAAC
    HANDLE_AACDECODER m_aacDecoderHandle;
    uint8_t * m_outputFrame = nullptr;
    size_t m_outputFrameLen;
#else
    NeAACDecHandle m_aacDecoderHandle;
    NeAACDecFrameInfo m_aacDecFrameInfo;
    void handleAudioOutputFAAD(const NeAACDecFrameInfo & frameInfo, const uint8_t * inFramePtr);
#endif
    int m_ascLen;
    uint8_t m_asc[7];

    float m_mp2DRC = 0;
    mpg123_handle * m_mp2DecoderHandle;

    dabsdrDecoderId_t m_inputDataDecoderId;
    int m_outFifoIdx;
    audioFifo_t * m_outFifoPtr;

#if !HAVE_FDKAAC
    int16_t * m_outBufferPtr;
    size_t m_outputBufferSamples;
    int m_numChannels;
    int m_muteRampDsFactor;
    std::vector<float> m_muteRamp;
    enum class OutputState
    {
        Init = 0,
        Muted,
        Unmuted
    } m_state;
#if AUDIO_DECODER_NOISE_CONCEALMENT
    QFile * m_noiseFile = nullptr;
    int16_t * m_noiseBufferPtr;
#endif
#endif
    void setOutput(int sampleRate, int numChannels);

    void readAACHeader();
    void initAACDecoder();
    void deinitAACDecoder();
    void processAAC(RadioControlAudioData *inData);

    void initMPG123();
    void deinitMPG123();
    void processMP2(RadioControlAudioData *inData);
    void getFormatMP2();

    // this is for dubugging
#ifdef AUDIO_DECODER_RAW_OUT
    FILE * m_rawOut;
#endif
#ifdef AUDIO_DECODER_AAC_OUT
    FILE * m_aacOut;
    void writeAACOutput(const char *data, uint16_t dataLen);
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    FILE * m_mp2Out;
    void writeMP2Output(const char *data, uint16_t dataLen);
#endif
};

#endif // AUDIODECODER_H
