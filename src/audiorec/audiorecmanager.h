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

#ifndef AUDIORECMANAGER_H
#define AUDIORECMANAGER_H

#include <QObject>

#include "audiorecorder.h"
#include "audiorecschedulemodel.h"
#include "settings.h"
#include "uicontrolprovider.h"

class AudioRecManager : public UIControlProvider
{
    Q_OBJECT
    Q_PROPERTY(AudioRecScheduleModel *scheduleModel READ scheduleModel CONSTANT FINAL)
    Q_PROPERTY(QItemSelectionModel *serviceSelectionModel READ serviceSelectionModel CONSTANT FINAL)
    UI_PROPERTY(QString, label)
    UI_PROPERTY(QDate, startDate)
    UI_PROPERTY(QTime, startTime)
    UI_PROPERTY(QTime, duration)
    UI_PROPERTY(QString, stopTime)
    UI_PROPERTY_DEFAULT(bool, isAudioRecordingActive, false)

public:
    explicit AudioRecManager(AudioRecScheduleModel *model, SLModel *slModel, AudioRecorder *recorder, Settings *settings, QObject *parent = nullptr);

    Q_INVOKABLE void requestItemDialog(bool isEdit);
    Q_INVOKABLE void addOrEditItem();
    Q_INVOKABLE void removeItem();
    Q_INVOKABLE void deleteAll();

    void addItem(const AudioRecScheduleItem &item);  // this slot is used to create new recording from EPG
    void onAudioServiceSelection(const RadioControlServiceComponent &s);

    // bool isAudioRecordingActive() const;
    QString audioRecordingFile() const;

    void doAudioRecording(bool start);

    bool isAudioScheduleActive() const;
    void requestCancelSchedule();

    void setHaveAudio(bool newHaveAudio);
    void onDLComplete(const QString &dl);
    void onDLReset() { m_dlText.clear(); }

    AudioRecScheduleModel *scheduleModel() const { return m_scheduleModel; }
    QItemSelectionModel *serviceSelectionModel() const { return m_serviceSelectionModel; }

signals:
    void showRecordingItemDialog();
    void audioRecordingCountdown(int numSec);
    void audioRecordingStarted();
    void audioRecordingStopped();
    void audioRecordingProgress(size_t bytes, qint64 timeSec);
    void requestServiceSelection(const ServiceListId &serviceId);

    // used to communicate with worker
    void startRecording();
    void stopRecording();

protected:
    void timerEvent(QTimerEvent *);

private:
    enum
    {
        COUNTDOWN_SEC = 30,
        SERVICESELECTION_SEC = 10,
        STARTADVANCE_SEC = 0
    };
    enum ScheduledRecordingState
    {
        StateIdle,
        StateCountdown,
        StateServiceSelection,
        StateReady,
        StateRecording
    } m_scheduledRecordingState;

    QBasicTimer m_timer;

    Settings *m_settings;
    SLModel *m_slModel;
    AudioRecScheduleModel *m_scheduleModel;
    AudioRecorder *m_recorder;
    QString m_audioRecordingFile;
    bool m_haveTimeConnection;

    qint64 m_scheduleTimeSecSinceEpoch;
    AudioRecScheduleItem m_currentItem;

    qint64 m_recTimeSec;
    QString m_dlText;
    QFile *m_dlLogFile;

    // this information is required to start scheduled recoding
    ServiceListId m_serviceId;
    bool m_haveAudio;

    void addItem();
    void editItem();
    void updateScheduledRecording();
    void onModelReset();
    void onModelRowsRemoved(const QModelIndex &, int first, int last);
    void onAudioRecordingStarted(const QString &recpath, const QString &filename);
    void onAudioRecordingProgress(size_t bytes, size_t timeSec);
    void onAudioRecordingStopped();
    void stopCurrentSchedule();
    void onCurrentIndexChanged();
    void updateStopTime();
    QItemSelectionModel *m_serviceSelectionModel = nullptr;
};

#endif  // AUDIORECMANAGER_H
