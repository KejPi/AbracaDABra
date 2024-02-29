/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
    explicit AudioRecManager(AudioRecScheduleModel * model, SLModel *slModel, AudioRecorder * recorder, QObject *parent = nullptr);
    void onAudioServiceSelection(const RadioControlServiceComponent &s);

    bool isAudioRecordingActive() const;
    QString audioRecordingFile() const;

    void audioRecording(bool start);

    bool isAudioScheduleActive() const;
    void requestCancelSchedule();

    void setHaveAudio(bool newHaveAudio);

signals:
    void audioRecordingCountdown(int numSec);
    void audioRecordingStarted();
    void audioRecordingStopped();
    void audioRecordingProgress(size_t bytes, qint64 timeSec);
    void requestServiceSelection(const ServiceListId & serviceId);

    // used to communicate with worker
    void startRecording();
    void stopRecording();

protected:
    void timerEvent(QTimerEvent *);

private:
    enum { COUNTDOWN_SEC = 30,
           SERVICESELECTION_SEC = 10,
           STARTADVANCE_SEC = 0};
    enum ScheduledRecordingState {StateIdle, StateCountdown, StateServiceSelection, StateReady, StateRecording} m_scheduledRecordingState;

    QBasicTimer m_timer;

    SLModel * m_slModel;
    AudioRecScheduleModel * m_model;    
    AudioRecorder * m_recorder;
    bool m_isAudioRecordingActive;
    QString m_audioRecordingFile;
    bool m_haveTimeConnection;    

    qint64 m_scheduleTimeSecSinceEpoch;
    AudioRecScheduleItem m_currentItem;

    // this information is required to start scheduled recoding
    ServiceListId m_serviceId;
    bool m_haveAudio;

    void updateScheduledRecording();
    void onModelReset();
    void onModelRowsRemoved(const QModelIndex &, int first, int last);
    void onAudioRecordingStarted(const QString &filename);
    void onAudioRecordingStopped();
    void stopCurrentSchedule();
};

#endif // AUDIORECMANAGER_H
