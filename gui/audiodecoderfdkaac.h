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

#ifndef AUDIODECODERFDKAAC_H
#define AUDIODECODERFDKAAC_H

#include <fdk-aac/aacdecoder_lib.h>
#include <mpg123.h>

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QObject>

#include "audiodecoder.h"
#include "audiorecorder.h"
#include "radiocontrol.h"

#define AUDIO_DECODER_FDKAAC_CONCEALMENT 1
#define AUDIO_DECODER_FDKAAC_NOISE_CONCEALMENT 0  // keep 0 here

class AudioDecoderFDKAAC : public AudioDecoder
{
    Q_OBJECT
public:
    explicit AudioDecoderFDKAAC(AudioRecorder *recorder, QObject *parent = nullptr);
    ~AudioDecoderFDKAAC();
    void setNoiseConcealment(int level) override;

private:
    HANDLE_AACDECODER m_aacDecoderHandle;

    void setOutput(int sampleRate, int numChannels);

    bool isAACHandleValid() const override { return m_aacDecoderHandle != nullptr; }
    void initAACDecoder() override;
    void deinitAACDecoder() override;
    void processAAC(RadioControlAudioData *inData) override;
};

#endif  // AUDIODECODERFDKAAC_H
