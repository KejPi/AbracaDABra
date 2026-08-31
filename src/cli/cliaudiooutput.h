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

#ifndef CLIAUDIOOUTPUT_H
#define CLIAUDIOOUTPUT_H

#include <QObject>
#include <QThread>
#include <atomic>
#include <portaudio.h>

#include "audiofifo.h"

// Reads decoded PCM samples out of the shared audioFifo_t buffer (filled by AudioDecoder) and
// blocking-writes them straight to the system's default PortAudio output device. This is a
// deliberately minimal alternative to the GUI's AudioOutputPa: no Qt Multimedia dependency (used
// there only for output-device enumeration), no mute/fade ramps, just "make the selected service
// audible" for headless/TUI use.
class CliAudioWorker : public QObject
{
    Q_OBJECT
public:
    explicit CliAudioWorker(QObject *parent = nullptr);

    // Thread-safe (atomic-backed): safe to call directly from any thread. Not queued slots,
    // since run() never returns to this object's event loop while it's active.
    void setFifo(audioFifo_t *fifo);
    void clearFifo();
    void requestStop();
    void setVolume(float linear);

public slots:
    void run();

private:
    void ensureStream(uint32_t sampleRate, uint8_t numChannels);
    void closeStream();

    std::atomic<audioFifo_t *> m_fifo{nullptr};
    std::atomic<bool> m_stop{false};
    std::atomic<float> m_volume{1.0f};

    PaStream *m_stream = nullptr;
    uint32_t m_streamSampleRate = 0;
    uint8_t m_streamNumChannels = 0;
};

// Owns the worker thread, PortAudio library lifetime, and exposes slots matching
// AudioDecoder's audio signals (same shape as AudioStreamer).
class CliAudioOutput : public QObject
{
    Q_OBJECT
public:
    explicit CliAudioOutput(QObject *parent = nullptr);
    ~CliAudioOutput() override;

public slots:
    void onStartAudio(audioFifo_t *buffer);
    void onSwitchAudio(audioFifo_t *buffer);
    void onStopAudio();

    // linear gain in [0.0, 1.0]; thread-safe, may be called from any thread
    void setVolume(float linear);

private:
    QThread *m_thread;
    CliAudioWorker *m_worker;
    bool m_paInitialized = false;
};

#endif  // CLIAUDIOOUTPUT_H
