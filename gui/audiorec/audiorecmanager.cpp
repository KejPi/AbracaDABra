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

#include "audiorecmanager.h"
#include "epgtime.h"

AudioRecManager::AudioRecManager(AudioRecScheduleModel *model, AudioRecorder *recorder, QObject *parent)
    : QObject{parent}, m_model(model), m_recorder(recorder)
{
    m_isAudioRecordingActive = false;
    m_haveTimeConnection = false;
    m_scheduleTimeSecSinceEpoch = 0;
    m_durationSec = 0;
    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AudioRecManager::onTimer);


    connect(EPGTime::getInstance(), &EPGTime::haveValidTime, this, &AudioRecManager::onValidTime);

    connect(this, &AudioRecManager::startRecording, m_recorder, &AudioRecorder::start, Qt::QueuedConnection);
    connect(this, &AudioRecManager::stopRecording, m_recorder, &AudioRecorder::stop, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStarted, this, &AudioRecManager::onAudioRecordingStarted, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStopped, this, &AudioRecManager::onAudioRecordingStopped, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingProgress, this, &AudioRecManager::audioRecordingProgress, Qt::QueuedConnection);

    connect(m_model, &QAbstractItemModel::modelReset, this,  &AudioRecManager::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this,  &AudioRecManager::onModelRowsRemoved);
}

AudioRecManager::~AudioRecManager()
{
    m_timer->stop();
    delete m_timer;
}

void AudioRecManager::onValidTime()
{   // valid time is available
    qDebug() << EPGTime::getInstance()->currentTime();

    // cleanup model -> it will remove items that are in past (emits reset)
    m_model->cleanup(EPGTime::getInstance()->currentTime());
}

void AudioRecManager::onTimeChanged()
{
    if (m_scheduleTimeSecSinceEpoch != 0)
    {
        qint64 numSec = m_scheduleTimeSecSinceEpoch - EPGTime::getInstance()->secSinceEpoch() - 1;
        qDebug() << EPGTime::getInstance()->currentTime() << numSec;
        if (numSec <= COUNTDOWN_SEC)
        {
            m_scheduledRecordingState = ScheduledRecordingState::StateStarted;
            m_numSec = numSec;
            int msec = 1000 - EPGTime::getInstance()->currentTime().time().msec();
            if (msec < 50)
            {   // call slot now
                onTimer();
            }
            else
            {
                m_timer->start(msec);
            }
            setTimeConnection(false);
            qDebug() << "Timer" << EPGTime::getInstance()->currentTime() << numSec << msec;
        }
    }
    else
    {
        setTimeConnection(false);
    }
}

void AudioRecManager::onTimer()
{
    qDebug() << EPGTime::getInstance()->currentTime() << m_numSec;
    switch (m_scheduledRecordingState)
    {
    case StateIdle:
        // this should not happen
        m_timer->stop();
        break;
    case StateStarted:
    {
        m_scheduledRecordingState = ScheduledRecordingState::StateCountdown;

        int timerSec = m_numSec-SERVICESELECTION_SEC;
        if (timerSec > 0)
        {
            qDebug() << "Countdown" << EPGTime::getInstance()->currentTime() << m_numSec;
            m_timer->start(1000 * timerSec);
            emit audioRecordingCountdown(timerSec-1);
            m_numSec -= timerSec;
            break;
        }
    }
    case StateCountdown:
    {
        emit stopRecording();

        // request service selection and no break
        qDebug() << "Service selection" << EPGTime::getInstance()->currentTime() << m_numSec;
        emit requestServiceSelection(m_serviceId);

        m_scheduledRecordingState = ScheduledRecordingState::StateReady;

        int timerSec = SERVICESELECTION_SEC - STARTADVANCE_SEC;
        if (timerSec > 0)
        {
            m_timer->start(1000 * timerSec);
            m_numSec -= timerSec;
            break;
        }
    }
    case StateReady:
        qDebug() << "Start" << EPGTime::getInstance()->currentTime() << m_numSec;
        emit startRecording();
        m_model->setData(m_model->index(0,0), true, Qt::EditRole);
        m_scheduledRecordingState = ScheduledRecordingState::StateRecording;
        m_timer->start(1000*(m_durationSec + STARTADVANCE_SEC));
        m_numSec = 0;
        break;
    case StateRecording:
        qDebug() << "End" << EPGTime::getInstance()->currentTime();
        emit stopRecording();

        m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
        m_scheduleTimeSecSinceEpoch = 0;
        m_durationSec = 0;
        if (!m_model->isEmpty())
        {
            m_model->removeRows(0,1);
        }
        m_timer->stop();
        break;
    }
}

void AudioRecManager::stopCurrentSchedule()
{
    m_timer->stop();
    if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
    {
        emit stopRecording();
        if (!m_model->isEmpty())
        {
            m_model->removeRows(0,1);
        }
    }

    m_scheduleTimeSecSinceEpoch = 0;
    m_durationSec = 0;
    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
}

bool AudioRecManager::isAudioScheduleActive() const
{
    return m_scheduledRecordingState >= ScheduledRecordingState::StateReady;
}

bool AudioRecManager::isAudioRecordingActive() const
{
    return m_isAudioRecordingActive;
}

QString AudioRecManager::audioRecordingFile() const
{
    return m_audioRecordingFile;
}

void AudioRecManager::onAudioRecordingStarted(const QString &filename)
{
    m_audioRecordingFile = filename;
    m_isAudioRecordingActive = true;
    emit audioRecordingStarted();
}

void AudioRecManager::onAudioRecordingStopped()
{
    qDebug() << Q_FUNC_INFO;
    m_isAudioRecordingActive = false;
    emit audioRecordingStopped();
}

void AudioRecManager::audioRecording(bool start)
{
    if (start) {
        emit startRecording();
    }
    else
    {
        if (isAudioScheduleActive())
        {
            requestCancelSchedule(true);
        }
        emit stopRecording();
    }
}

void AudioRecManager::requestCancelSchedule(bool cancelRequest)
{
    if (cancelRequest && (m_scheduledRecordingState != ScheduledRecordingState::StateIdle))
    {
        qDebug() << "Scheduled recording cancelled" << EPGTime::getInstance()->currentTime();
        m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
        m_scheduleTimeSecSinceEpoch = 0;
        m_durationSec = 0;
        m_model->removeRows(0,1);
        m_timer->stop();
    }
}

void AudioRecManager::setTimeConnection(bool ena)
{
    if (ena)
    {
        if (!m_haveTimeConnection)
        {
            connect(EPGTime::getInstance(), &EPGTime::secSinceEpochChanged, this, &AudioRecManager::onTimeChanged);
            m_haveTimeConnection = true;
        }
    }
    else
    {
        if (m_haveTimeConnection)
        {
            disconnect(EPGTime::getInstance(), &EPGTime::secSinceEpochChanged, this, &AudioRecManager::onTimeChanged);
            m_haveTimeConnection = false;
        }
    }
}

void AudioRecManager::updateScheduledRecording()
{
    qDebug() << Q_FUNC_INFO;
    if (m_model->isEmpty())
    {
        setTimeConnection(false);
        stopCurrentSchedule();
    }
    else
    {   // there is something in schedule => get first item
        const AudioRecScheduleItem item = m_model->itemAtIndex(m_model->index(0,0, QModelIndex()));
        if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
        {
            if ((item.serviceId() == m_serviceId) && (item.startTime().toSecsSinceEpoch() == m_scheduleTimeSecSinceEpoch))
            {   // the same item -> update end time
                qDebug() << Q_FUNC_INFO << "Updating end time";
                int newTimeout = item.durationSec()*1000 - (m_durationSec*1000 - m_timer->remainingTime());
                m_timer->stop();
                if (newTimeout > 0)
                {
                    qDebug() << "new timeout" << newTimeout/1000;
                    m_timer->start(newTimeout);
                }
                else
                {
                    m_timer->start(1);
                }
                m_durationSec = item.durationSec();
                return;
            }
        }

        stopCurrentSchedule();
        m_scheduleTimeSecSinceEpoch = item.startTime().toSecsSinceEpoch();
        m_durationSec = item.durationSec();
        m_serviceId = item.serviceId();
        setTimeConnection(true);
    }


#if 0
        if (isAudioScheduleActive())
        {   // recording from schedude
            if ((item.serviceId() == m_serviceId) && (item.startTime().toSecsSinceEpoch() == m_scheduleTimeSecSinceEpoch))
            {   // the same item -> update end time
                qDebug() << Q_FUNC_INFO << "Updating end time";
                if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
                {   // updating timer
                    //int remainingTime = m_timer->remainingTime();
                    int newTimeout = item.durationSec()*1000 - (m_durationSec*1000 - m_timer->remainingTime());
                    m_timer->stop();
                    if (newTimeout > 0)
                    {
                        qDebug() << "new timeout" << newTimeout;
                        m_timer->start(newTimeout);
                    }
                    else
                    {
                        m_timer->start(1);
                    }
                }
                m_durationSec = item.durationSec();
                return;
            }
            else
            {   // stop current recording
                qDebug() << Q_FUNC_INFO << "Stopping recording";
                m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                m_scheduleTimeSecSinceEpoch = 0;
                m_durationSec = 0;
                m_timer->stop();
                emit stopRecording();
            }
        }        
        m_scheduleTimeSecSinceEpoch = item.startTime().toSecsSinceEpoch();
        m_durationSec = item.durationSec();
        m_serviceId = item.serviceId();
        setTimeConnection(true);

        qDebug() << item.startTime();
#endif
}

void AudioRecManager::onModelReset()
{
    qDebug() << Q_FUNC_INFO;
    updateScheduledRecording();
}

void AudioRecManager::onModelRowsRemoved(const QModelIndex &, int first, int last)
{
    qDebug() << Q_FUNC_INFO;
    updateScheduledRecording();
}

