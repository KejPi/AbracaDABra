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

#ifndef AUDIORECMANAGER_H
#define AUDIORECMANAGER_H

#include <QObject>
#include "audiorecschedulemodel.h"
#include "audiorecorder.h"

class AudioRecManager : public QObject
{
    Q_OBJECT
public:
    explicit AudioRecManager(AudioRecScheduleModel * model, AudioRecorder * recorder, QObject *parent = nullptr);
    void onValidTime();
    void onTimeChanged();

    bool isAudioRecordingActive() const;
    QString audioRecordingFile() const;

    void onAudioRecordingStarted(const QString &filename);
    void onAudioRecordingStopped();

    void audioRecording(bool start);
signals:
    void audioRecordingStarted();
    void audioRecordingStopped();
    void audioRecordingProgress(size_t bytes, size_t timeSec);

    // used to communicate with worker
    void startRecording();
    void stopRecording();

private:
    AudioRecScheduleModel * m_model;
    AudioRecorder * m_recorder;
    //AudioRecScheduleItem m_scheduledItem;
    bool m_isAudioRecordingActive;
    QString m_audioRecordingFile;
    bool m_haveTimeConnection;

    void updateScheduledRecording();
    void onModelReset();
    void onModelRowsRemoved(const QModelIndex &, int first, int last);
};

#endif // AUDIORECMANAGER_H
