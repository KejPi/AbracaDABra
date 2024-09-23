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

#include <QLoggingCategory>
#include "audiorecmanager.h"
#include "epgtime.h"

Q_LOGGING_CATEGORY(audioRecMgr, "AudioRecSchedule", QtInfoMsg)

AudioRecManager::AudioRecManager(AudioRecScheduleModel *model, SLModel *slModel, AudioRecorder *recorder, Settings * settings, QObject *parent)
    : QObject{parent}, m_model(model), m_slModel(slModel), m_recorder(recorder), m_settings(settings)
{   
    m_isAudioRecordingActive = false;
    m_haveTimeConnection = false;
    m_haveAudio = false;
    m_scheduleTimeSecSinceEpoch = 0;
    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
    m_recTimeSec = 0;
    m_dlLogFile = nullptr;

    // pass service list mode to schedule model
    m_model->setSlModel(m_slModel);

    connect(this, &AudioRecManager::startRecording, m_recorder, &AudioRecorder::start, Qt::QueuedConnection);
    connect(this, &AudioRecManager::stopRecording, m_recorder, &AudioRecorder::stop, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStarted, this, &AudioRecManager::onAudioRecordingStarted, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStopped, this, &AudioRecManager::onAudioRecordingStopped, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingProgress, this, &AudioRecManager::onAudioRecordingProgress, Qt::QueuedConnection);

    connect(m_model, &QAbstractItemModel::modelReset, this,  &AudioRecManager::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this,  &AudioRecManager::onModelRowsRemoved);           
}

void AudioRecManager::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    //qDebug() << EPGTime::getInstance()->currentTime() << QDateTime::currentDateTime();
    if (m_scheduleTimeSecSinceEpoch != 0)
    {        
        switch (m_scheduledRecordingState) {
        case StateIdle:
        {
            // check that service is available
            const ServiceList * slPtr = m_slModel->getServiceList();
            const auto it = slPtr->findService(m_currentItem.serviceId());
            if (it == slPtr->serviceListEnd())
            {   // service not found => cancelling the schedule
                m_timer.stop();
                m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                m_scheduleTimeSecSinceEpoch = 0;
                if (!m_model->isEmpty())
                {
                    m_model->removeRows(0,1);
                }
            }
            else
            {
                m_timer.start(1000, Qt::PreciseTimer, this);

                int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentDateTime().toSecsSinceEpoch();
                qCInfo(audioRecMgr) << "Recording countdown" <<  numSec << "sec:" << m_currentItem.name();
                emit audioRecordingCountdown(numSec-SERVICESELECTION_SEC);
                m_scheduledRecordingState = ScheduledRecordingState::StateCountdown;
            }
            break;
        }
        case StateCountdown:
        {
            int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentDateTime().toSecsSinceEpoch();
            if (numSec <= SERVICESELECTION_SEC)
            {
                emit stopRecording();
                m_scheduledRecordingState = ScheduledRecordingState::StateServiceSelection;
            }
        }
        break;
        case StateServiceSelection:
        {
            qCInfo(audioRecMgr) << "Service selection:" << m_currentItem.name();
            const ServiceList * slPtr = m_slModel->getServiceList();
            const auto it = slPtr->findService(m_currentItem.serviceId());
            if (it == slPtr->serviceListEnd())
            {   // service not found => cancelling the schedule
                m_timer.stop();
                m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                m_scheduleTimeSecSinceEpoch = 0;
                if (!m_model->isEmpty())
                {
                    m_model->removeRows(0,1);
                }
            }
            else
            {
                emit requestServiceSelection(m_currentItem.serviceId());
                m_scheduledRecordingState = ScheduledRecordingState::StateReady;
                emit audioRecordingProgress(0, -1);
            }
        }
        break;
        case StateReady:
        {
            int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentDateTime().toSecsSinceEpoch();
            if ((numSec <= STARTADVANCE_SEC) && (m_serviceId == m_currentItem.serviceId()) && m_haveAudio)
            {
                qCInfo(audioRecMgr) << "Recording started:" << m_currentItem.name();
                emit startRecording();
                m_model->setData(m_model->index(0,0), true, Qt::EditRole);
                m_scheduleTimeSecSinceEpoch = m_currentItem.endTime().toSecsSinceEpoch();
                m_scheduledRecordingState = ScheduledRecordingState::StateRecording;
            }
            else if (QDateTime::currentDateTime().toSecsSinceEpoch() >= m_currentItem.endTime().toSecsSinceEpoch())
            {   // stop
                emit audioRecordingStopped();
                m_timer.stop();
                m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                m_scheduleTimeSecSinceEpoch = 0;
                if (!m_model->isEmpty())
                {
                    m_model->removeRows(0,1);
                }
            }
        }
        break;
        case StateRecording:
        {
            int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentDateTime().toSecsSinceEpoch();
            if (numSec <= 0) {
                qCInfo(audioRecMgr) << "Recording finished:" << m_currentItem.name();
                emit stopRecording();
                m_timer.stop();
                m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                m_scheduleTimeSecSinceEpoch = 0;
                if (!m_model->isEmpty())
                {
                    m_model->removeRows(0,1);
                }
            }

        }
        break;
        }
    }
    else
    {
        m_timer.stop();
    }
}

void AudioRecManager::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    m_serviceId = ServiceListId(s);
    m_haveAudio = false;
}

void AudioRecManager::stopCurrentSchedule()
{
    m_timer.stop();
    if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
    {
        emit stopRecording();
    }
    m_scheduleTimeSecSinceEpoch = 0;
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
    if (m_settings->audioRecDl && (nullptr == m_dlLogFile))
    {
        QFileInfo fi(filename);
        QString dlLogFilename  = fi.path() + "/" + fi.completeBaseName() + ".txt";
        m_dlLogFile = new QFile(dlLogFilename);
        if (m_dlLogFile->open(QIODevice::WriteOnly))
        {
            QTextStream out(m_dlLogFile);
            if (!m_dlText.isEmpty()) {
                if (m_settings->audioRecDlAbsTime)
                {
                    out << "00:00:00\t" << EPGTime::getInstance()->dabTime().toString("yyyy-MM-dd-hhmmss\t") << m_dlText << Qt::endl;
                }
                else
                {
                    out << "00:00:00\t" << m_dlText << Qt::endl;
                }
            }
        }
        else
        {
            qCCritical(audioRecMgr) << "Unable to open file:" << dlLogFilename;
            delete m_dlLogFile;
            m_dlLogFile = nullptr;
        }
    }
    else {
        m_dlLogFile = nullptr;
    }
    emit audioRecordingStarted();
}

void AudioRecManager::onAudioRecordingProgress(size_t bytes, size_t timeSec)
{
    m_recTimeSec = timeSec;
    emit audioRecordingProgress(bytes, timeSec);
}

void AudioRecManager::onAudioRecordingStopped()
{
    m_isAudioRecordingActive = false;
    if (m_dlLogFile)
    {
        m_dlLogFile->close();
        delete m_dlLogFile;
        m_dlLogFile = nullptr;
    }
    emit audioRecordingStopped();
}

void AudioRecManager::doAudioRecording(bool start)
{
    if (start) {
        m_recTimeSec = 0;
        emit startRecording();
    }
    else
    {
        if (isAudioScheduleActive())
        {
            requestCancelSchedule();
        }
        emit stopRecording();
    }
}

void AudioRecManager::requestCancelSchedule()
{
    if (m_scheduledRecordingState != ScheduledRecordingState::StateIdle)
    {
        qCInfo(audioRecMgr) << "Recording cancelled:" << m_currentItem.name();
        m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
        m_scheduleTimeSecSinceEpoch = 0;
        m_model->removeRows(0,1);
        emit audioRecordingStopped();
    }
}

void AudioRecManager::setHaveAudio(bool newHaveAudio)
{
    m_haveAudio = newHaveAudio;
}

void AudioRecManager::onDLComplete(const QString &dl)
{
    if (m_dlText != dl)
    {
        m_dlText = dl;
        if (m_isAudioRecordingActive && m_dlLogFile)
        {
            QTextStream out(m_dlLogFile);

            int hours = m_recTimeSec / 3600;
            int min = (m_recTimeSec - hours*3600)/60;
            int sec = (m_recTimeSec - hours*3600 - min*60);
            if (m_settings->audioRecDlAbsTime)
            {
                out << QString("%1:%2:%3\t%4\t%5\n").arg(hours, 2, 10, QChar('0')).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
                           .arg(EPGTime::getInstance()->dabTime().toString("yyyy-MM-dd-hhmmss"), dl);
            }
            else
            {
                out << QString("%1:%2:%3\t%4\n").arg(hours, 2, 10, QChar('0')).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
                .arg(dl);
            }
        }
    }
}

void AudioRecManager::updateScheduledRecording()
{
    if (m_model->isEmpty())
    {
        stopCurrentSchedule();
    }
    else
    {   // there is something in schedule => get first item
        const AudioRecScheduleItem item = m_model->itemAtIndex(m_model->index(0,0, QModelIndex()));
        if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
        {
            if ((item.serviceId() == m_currentItem.serviceId()) && (item.startTime() == m_currentItem.startTime()))
            {   // the same item -> update end time
                qCDebug(audioRecMgr) << "Updating end time";
                m_currentItem = item;
                m_scheduleTimeSecSinceEpoch = m_currentItem.endTime().toSecsSinceEpoch();
                return;
            }
        }

        stopCurrentSchedule();
        m_currentItem = item;
        m_scheduleTimeSecSinceEpoch = m_currentItem.startTime().toSecsSinceEpoch();
        qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
        qint64 secToSchedule = m_scheduleTimeSecSinceEpoch - now - COUNTDOWN_SEC;
        if (secToSchedule > 0)
        {   // run timer
            m_timer.start(secToSchedule*1000, Qt::VeryCoarseTimer, this);
        }
        else if (secToSchedule <= 0)
        {   // run callback immediately
            timerEvent(nullptr);
        }
    }
}

void AudioRecManager::onModelReset()
{
    updateScheduledRecording();
}

void AudioRecManager::onModelRowsRemoved(const QModelIndex &, int first, int last)
{
    updateScheduledRecording();
}

