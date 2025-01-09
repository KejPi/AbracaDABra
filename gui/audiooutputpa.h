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

#ifndef AUDIOOUTPUTPA_H
#define AUDIOOUTPUTPA_H

#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QWaitCondition>

#include "audiofifo.h"
#include "audiooutput.h"
#include "portaudio.h"

#define AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND 1

// port audio allows to set number of samples in callback
// this number must be aligned between AUDIOOUTPUT_FADE_TIME_MS and AUDIO_FIFO_CHUNK_MS
#if (AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)
#error "(AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)"
#endif

class AudioOutputPa : public AudioOutput
{
    Q_OBJECT

public:
    AudioOutputPa(QObject *parent = nullptr);
    ~AudioOutputPa();

    void start(audioFifo_t *buffer) override;
    void restart(audioFifo_t *buffer) override;
    void stop() override;
    void mute(bool on) override;
    void setVolume(int value) override;
    void setAudioDevice(const QByteArray &deviceId) override;

private:
    enum Request
    {
        None = 0,           // no bit
        Mute = (1 << 0),    // bit #0
        Stop = (1 << 1),    // bit #1
        Restart = (1 << 2)  // bit #2
    };
    std::atomic<unsigned int> m_cbRequest = Request::None;

    PaStream *m_outStream = nullptr;
    audioFifo_t *m_inFifoPtr = nullptr;
    audioFifo_t *m_restartFifoPtr = nullptr;
    uint8_t m_numChannels;
    uint32_t m_sampleRate_kHz;
    unsigned int m_bufferFrames;
    uint8_t m_bytesPerFrame;
    float m_muteFactor;
    std::atomic<float> m_linearVolume;
    AudioOutputPlaybackState m_playbackState;
    bool m_reloadDevice = false;

    int portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames);
    void portAudioStreamFinishedPrivateCb() { emit streamFinished(); }

    static int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames, const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags, void *ctx);
    static void portAudioStreamFinishedCb(void *ctx);

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    FILE *m_rawOut;
#endif

    void onStreamFinished();
    PaDeviceIndex getCurrentDeviceIdx();

signals:
    void streamFinished();  // this signal is emited from portAudioStreamFinishedCb
};

#endif  // AUDIOOUTPUTPA_H
