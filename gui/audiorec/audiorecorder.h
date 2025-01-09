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

#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QFile>
#include <QObject>

#include "dabsdr.h"
#include "radiocontrol.h"

class AudioRecorder : public QObject
{
    Q_OBJECT
public:
    enum class RecordingState
    {
        Stopped = 0,
        RecordingMP2,
        RecordingAAC,
        RecordingWav
    };

    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder();
    QString recordingPath() const;
    void setup(const QString &recordingPath, bool doOutputRecording = false);
    void setAudioService(const RadioControlServiceComponent &s);
    void setDataFormat(int sampleRateKHz, bool isAAC);
    void start();
    void stop();
    void recordData(const RadioControlAudioData *inData, const int16_t *outputData, size_t numOutputSamples);

signals:
    void recordingStarted(const QString &filename);
    void recordingStopped();
    void recordingProgress(size_t bytes, size_t timeSec);

private:
    QString m_recordingPath;
    QFile *m_file;
    DabSId m_sid;
    QString m_serviceName;
    RecordingState m_recordingState;
    bool m_doOutputRecording;
    size_t m_bytesWritten;
    size_t m_timeSec;
    size_t m_timeWrittenMs;
    int m_sampleRateKHz;
    bool m_isAAC;

    void writeMP2(const std::vector<uint8_t> &data);
    void writeAAC(const std::vector<uint8_t> &data, const dabsdrAudioFrameHeader_t &aacHeader);
    void writeWav(const int16_t *data, size_t numSamples);
    void writeWavHeader();
};

#endif  // AUDIORECORDER_H
