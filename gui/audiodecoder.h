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

#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <mpg123.h>

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QObject>

#include "audiofifo.h"
#include "audiorecorder.h"
#include "radiocontrol.h"

#define AUDIO_DECODER_BUFFER_SIZE 3840  // this is maximum buffer size for HE-AAC

enum class AudioCoding
{
    None = -1,
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
    explicit AudioDecoder(AudioRecorder *recorder, QObject *parent = nullptr);
    ~AudioDecoder();
    void start(const RadioControlServiceComponent &s);
    void stop();
    void decodeData(RadioControlAudioData *inData);
    void getAudioParameters();
    virtual void setNoiseConcealment(int level) = 0;

signals:
    void startAudio(audioFifo_t *buffer);
    void switchAudio(audioFifo_t *buffer);
    void stopAudio();
    void audioParametersInfo(const AudioParameters &params);

protected:
    enum class PlaybackState
    {
        Stopped = 0,
        WaitForInit,
        Running
    } m_playbackState;
    bool m_streamDropout = false;

    dabsdrAudioFrameHeader_t m_aacHeader;
    int m_ascLen;
    uint8_t m_asc[7];

    AudioRecorder *m_recorder;
    AudioParameters m_audioParameters;

    int16_t *m_outBufferPtr;
    size_t m_outputBufferSamples;

    dabsdrDecoderId_t m_inputDataDecoderId;
    int m_outFifoIdx;
    audioFifo_t *m_outFifoPtr;

    void setOutput(int sampleRate, int numChannels);

    virtual bool isAACHandleValid() const = 0;
    void readAACHeader();
    virtual void initAACDecoder() = 0;
    virtual void deinitAACDecoder() = 0;
    virtual void processAAC(RadioControlAudioData *inData) = 0;

    void initMPG123();
    void deinitMPG123();

private:
    float m_mp2DRC = 0;
    mpg123_handle *m_mp2DecoderHandle;

    void processMP2(RadioControlAudioData *inData);
    void getFormatMP2();
};

#endif  // AUDIODECODER_H
