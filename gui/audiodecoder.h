/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#define AUDIO_DECODER_BUFFER_SIZE     3840  // this is maximum buffer size for HE-AAC
#if HAVE_FDKAAC
#define AUDIO_DECODER_FDKAAC_CONCEALMENT 1
#define AUDIO_DECODER_NOISE_CONCEALMENT  0 // keep 0 here
#else // HAVE_FDKAAC
#define AUDIO_DECODER_FADE_TIME_MS      20  // maximum is 20 ms
#define AUDIO_DECODER_NOISE_CONCEALMENT  1
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
    void start(const RadioControlServiceComponent &s);
    void stop();
    void decodeData(RadioControlAudioData *inData);
    void getAudioParameters();
    void setNoiseConcealment(int level);

signals:
    void startAudio(audioFifo_t *buffer);
    void switchAudio(audioFifo_t *buffer);
    void stopAudio();
    void audioParametersInfo(const AudioParameters & params);
    void inputData(const std::vector<uint8_t> &data);
    void outputData(const int16_t *data, size_t numSamples);

private:
    enum class PlaybackState { Stopped = 0, WaitForInit, Running } m_playbackState;

    dabsdrAudioFrameHeader_t m_aacHeader;
    AudioParameters m_audioParameters;

    int16_t * m_outBufferPtr;
#if HAVE_FDKAAC
    HANDLE_AACDECODER m_aacDecoderHandle;
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
    size_t m_outputBufferSamples;
    int m_numChannels;
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
    float m_noiseLevel;
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
    void writeAACOutput(const std::vector<uint8_t> & data);
#endif
#ifdef AUDIO_DECODER_MP2_OUT
    FILE * m_mp2Out;
    void writeMP2Output(const std::vector<uint8_t> & data);
#endif
};

#endif // AUDIODECODER_H
