/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QAudioFormat>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QWaitCondition>
#include <atomic>
#include <vector>

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

    // Restart tracking to prevent loops
    int m_consecutiveRestarts = 0;
    QElapsedTimer m_lastRestartTimer;
    static constexpr int MAX_CONSECUTIVE_RESTARTS = 3;
    static constexpr qint64 RESTART_RESET_INTERVAL_MS = 5000;  // Reset counter after 5s

    void handleStateChanged(QAudio::State newState);
    int64_t bytesAvailable();
    void doStop();
    void doRestart(audioFifo_t *buffer);
    void resetRestartCounter();

#ifdef Q_OS_ANDROID
    void acquireAndroidWakeLock();
    void releaseAndroidWakeLock();
#endif
};

class AudioIODevice : public QIODevice
{
public:
    AudioIODevice(QObject *parent = nullptr);

    void start();
    void stop();
    void setBuffer(audioFifo_t *buffer);

    // Sets the format that QAudioSink actually uses. If it differs from the FIFO format
    // (48kHz/Int16/stereo or similar) the samples are converted in readData().
    void setOutputFormat(const QAudioFormat &format);
    static bool canConvertTo(const QAudioFormat &format);

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;

    void mute(bool on);
    bool isMuted() const { return AudioOutputPlaybackState::Muted == m_playbackState; }
    bool isStopRequested() const { return m_stopFlag; }

private:
    qint64 readNative(char *data, qint64 maxlen);     // reads FIFO format (Int16, FIFO rate and channels)
    qint64 readConverted(char *data, qint64 maxlen);  // reads FIFO and converts to m_outFormat

    audioFifo_t *m_inFifoPtr = nullptr;
    AudioOutputPlaybackState m_playbackState;
    uint8_t m_bytesPerFrame;
    uint32_t m_sampleRate_kHz;
    uint8_t m_numChannels;
    float m_muteFactor;
    bool m_doStop = false;

    std::atomic<bool> m_muteFlag = false;
    std::atomic<bool> m_stopFlag = false;

    // output format conversion (used when sink format != FIFO format)
    bool m_convert = false;
    QAudioFormat m_outFormat;
    int m_outBytesPerFrame = 0;
    int m_outChannels = 0;
    double m_ratio = 1.0;             // input frames per output frame
    double m_resPos = 0.0;            // fractional read position in m_inBuf (in frames)
    std::vector<int16_t> m_inBuf;     // interleaved input frames not yet fully consumed

#ifdef AUDIO_DEBUG_STATS
    QTimer *m_statsTimer = nullptr;
    int m_byteCounter = 0;
    int64_t m_bufferFullnessAvrg = 0;
    int m_avrgCntr = 0;
#endif
};

#endif  // AUDIOOUTPUTQT_H
