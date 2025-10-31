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

#ifndef AUDIOOUTPUTQT_H
#define AUDIOOUTPUTQT_H

#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QWaitCondition>

#include "audiofifo.h"
#include "audiooutput.h"

class AudioIODevice;

class AudioOutputQt : public AudioOutput
{
    Q_OBJECT

public:
    AudioOutputQt(QObject *parent = nullptr);
    ~AudioOutputQt();

    void start(audioFifo_t *buffer) override;
    void restart(audioFifo_t *buffer) override;
    void stop() override;
    void mute(bool on) override;
    void setVolume(int value) override;
    void setAudioDevice(const QByteArray &deviceId) override;

private:
    // Qt audio
    AudioIODevice *m_ioDevice;
    QAudioSink *m_audioSink;
    float m_linearVolume;

    void handleStateChanged(QAudio::State newState);
    int64_t bytesAvailable();
    void doStop();
    void doRestart(audioFifo_t *buffer);
};

class AudioIODevice : public QIODevice
{
public:
    AudioIODevice(QObject *parent = nullptr);

    void start();
    void stop();
    void setBuffer(audioFifo_t *buffer);

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;

    void mute(bool on);
    bool isMuted() const { return AudioOutputPlaybackState::Muted == m_playbackState; }

private:
    audioFifo_t *m_inFifoPtr = nullptr;
    AudioOutputPlaybackState m_playbackState;
    uint8_t m_bytesPerFrame;
    uint32_t m_sampleRate_kHz;
    uint8_t m_numChannels;
    float m_muteFactor;
    bool m_doStop = false;

    std::atomic<bool> m_muteFlag = false;
    std::atomic<bool> m_stopFlag = false;
};

#endif  // AUDIOOUTPUTQT_H
