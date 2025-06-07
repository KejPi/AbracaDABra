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

#ifndef AUDIODECODERFAAD_H
#define AUDIODECODERFAAD_H

#include <neaacdec.h>

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QObject>

#include "audiodecoder.h"
#include "audiorecorder.h"
#include "radiocontrol.h"

#define AUDIO_DECODER_FADE_TIME_MS 20  // maximum is 20 ms
#define AUDIO_DECODER_FAAD_NOISE_CONCEALMENT 1

class AudioDecoderFAAD : public AudioDecoder
{
    Q_OBJECT
public:
    explicit AudioDecoderFAAD(AudioRecorder *recorder, QObject *parent = nullptr);
    ~AudioDecoderFAAD();
    void setNoiseConcealment(int level) override;

private:
    NeAACDecHandle m_aacDecoderHandle;
    NeAACDecFrameInfo m_aacDecFrameInfo;

    int m_numChannels;
    std::vector<float> m_muteRamp;
    enum class OutputState
    {
        Init = 0,
        Muted,
        Unmuted
    } m_state;
#if AUDIO_DECODER_FAAD_NOISE_CONCEALMENT
    QFile *m_noiseFile = nullptr;
    int16_t *m_noiseBufferPtr;
    float m_noiseLevel;
#endif

    bool isAACHandleValid() const override { return m_aacDecoderHandle != nullptr; }
    void initAACDecoder() override;
    void deinitAACDecoder() override;
    void processAAC(RadioControlAudioData *inData) override;
    void handleAudioOutput(const NeAACDecFrameInfo &frameInfo, const uint8_t *inFramePtr);
};

#endif  // AUDIODECODERFAAD_H
