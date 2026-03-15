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

#include "audiorecmanager.h"

#include <QLoggingCategory>

#include "androidfilehelper.h"
#include "epgtime.h"

Q_LOGGING_CATEGORY(audioRecMgr, "AudioRecSchedule", QtInfoMsg)

AudioRecManager::AudioRecManager(AudioRecScheduleModel *model, SLModel *slModel, AudioRecorder *recorder, Settings *settings, QObject *parent)
    : UIControlProvider{parent}, m_scheduleModel(model), m_slModel(slModel), m_recorder(recorder), m_settings(settings)
{
    m_haveTimeConnection = false;
    m_haveAudio = false;
    m_scheduleTimeSecSinceEpoch = 0;
    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
    m_recTimeSec = 0;
    m_dlLogFile = nullptr;

    // pass service list mode to schedule model
    m_scheduleModel->setSlModel(m_slModel);
    m_serviceSelectionModel = new QItemSelectionModel(m_slModel, this);
    connect(this, &AudioRecManager::startRecording, m_recorder, &AudioRecorder::start, Qt::QueuedConnection);
    connect(this, &AudioRecManager::stopRecording, m_recorder, &AudioRecorder::stop, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStarted, this, &AudioRecManager::onAudioRecordingStarted, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStopped, this, &AudioRecManager::onAudioRecordingStopped, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingProgress, this, &AudioRecManager::onAudioRecordingProgress, Qt::QueuedConnection);

    connect(m_scheduleModel, &QAbstractItemModel::modelReset, this, &AudioRecManager::onModelReset);
    connect(m_scheduleModel, &QAbstractItemModel::rowsRemoved, this, &AudioRecManager::onModelRowsRemoved);
    connect(m_scheduleModel, &AudioRecScheduleModel::currentIndexChanged, this, &AudioRecManager::onCurrentIndexChanged);

    connect(this, &AudioRecManager::startDateChanged, this, &AudioRecManager::updateStopTime);
    connect(this, &AudioRecManager::startTimeChanged, this, &AudioRecManager::updateStopTime);
    connect(this, &AudioRecManager::durationChanged, this, &AudioRecManager::updateStopTime);
}

void AudioRecManager::requestItemDialog(bool isEdit)
{
    if (isEdit == false)
    {
        m_scheduleModel->setCurrentIndex(-1);
    }
    else if (m_scheduleModel->currentIndex() < 0)
    {
        return;
    }
    emit showRecordingItemDialog();
}

void AudioRecManager::addOrEditItem()
{
    if (m_scheduleModel->currentIndex() >= 0)
    {
        editItem();
    }
    else
    {
        addItem();
    }
}

void AudioRecManager::addItem()
{
    AudioRecScheduleItem item;
    QDateTime start;
    start.setDate(startDate());
    start.setTime(startTime());
    item.setStartTime(start);
    item.setDurationSec(QTime(0, 0).secsTo(duration()));
    auto selection = m_serviceSelectionModel->selection();
    if (!selection.isEmpty())
    {
        QModelIndex index = selection.indexes().first();
        item.setServiceId(m_slModel->id(index));
        if (label().isEmpty())
        {  // using service name as fallback
            item.setName(m_slModel->data(index, Qt::DisplayRole).toString());
        }
        else
        {
            item.setName(label());
        }

        m_scheduleModel->insertItem(item);

        // clear data
        onCurrentIndexChanged();
    }
}

void AudioRecManager::editItem()
{
    AudioRecScheduleItem item;
    QDateTime start;
    start.setDate(startDate());
    start.setTime(startTime());
    item.setStartTime(start);
    item.setDurationSec(QTime(0, 0).secsTo(duration()));
    auto selection = m_serviceSelectionModel->selection();
    if (!selection.isEmpty())
    {
        QModelIndex index = selection.indexes().first();
        item.setServiceId(m_slModel->id(index));
        if (label().isEmpty())
        {  // using service name as fallback
            item.setName(m_slModel->data(index, Qt::DisplayRole).toString());
        }
        else
        {
            item.setName(label());
        }
        m_scheduleModel->replaceItemAtIndex(m_scheduleModel->index(m_scheduleModel->currentIndex(), 0), item);
    }
}

void AudioRecManager::removeItem()
{
    m_scheduleModel->removeRows(m_scheduleModel->currentIndex(), 1, QModelIndex());
}

void AudioRecManager::deleteAll()
{
    m_scheduleModel->clear();
}

void AudioRecManager::addItem(const AudioRecScheduleItem &item)
{
    m_scheduleModel->setCurrentIndex(-1);

    // set UI
    label(item.name());
    startDate(item.startTime().date());
    startTime(item.startTime().time());
    duration(QTime(0, 0).addSecs(item.durationSec()));
    stopTime(item.startTime().addSecs(item.durationSec()).toString("dd.MM.yyyy, hh:mm"));

    for (int r = 0; r < m_slModel->rowCount(); ++r)
    {
        QModelIndex index = m_slModel->index(r, 0);
        if (m_slModel->id(index) == item.serviceId())
        {  // found
            m_serviceSelectionModel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
            break;
        }
    }
    emit showRecordingItemDialog();
}

void AudioRecManager::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    // qDebug() << EPGTime::getInstance()->currentTime() << QDateTime::currentDateTime();
    if (m_scheduleTimeSecSinceEpoch != 0)
    {
        switch (m_scheduledRecordingState)
        {
            case StateIdle:
            {
                // check that service is available
                const ServiceList *slPtr = m_slModel->getServiceList();
                const auto it = slPtr->findService(m_currentItem.serviceId());
                if (it == slPtr->serviceListEnd())
                {  // service not found => cancelling the schedule
                    m_timer.stop();
                    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                    m_scheduleTimeSecSinceEpoch = 0;
                    if (!m_scheduleModel->isEmpty())
                    {
                        m_scheduleModel->removeRows(0, 1);
                    }
                }
                else
                {
                    m_timer.start(1000, Qt::PreciseTimer, this);

                    int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentSecsSinceEpoch();
                    qCInfo(audioRecMgr) << "Recording countdown" << numSec << "sec:" << m_currentItem.name();
                    emit audioRecordingCountdown(numSec - SERVICESELECTION_SEC);
                    m_scheduledRecordingState = ScheduledRecordingState::StateCountdown;
                }
                break;
            }
            case StateCountdown:
            {
                int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentSecsSinceEpoch();
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
                const ServiceList *slPtr = m_slModel->getServiceList();
                const auto it = slPtr->findService(m_currentItem.serviceId());
                if (it == slPtr->serviceListEnd())
                {  // service not found => cancelling the schedule
                    m_timer.stop();
                    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                    m_scheduleTimeSecSinceEpoch = 0;
                    if (!m_scheduleModel->isEmpty())
                    {
                        m_scheduleModel->removeRows(0, 1);
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
                int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentSecsSinceEpoch();
                if ((numSec <= STARTADVANCE_SEC) && (m_serviceId == m_currentItem.serviceId()) && m_haveAudio)
                {
                    qCInfo(audioRecMgr) << "Recording started:" << m_currentItem.name();
                    emit startRecording();
                    m_scheduleModel->setData(m_scheduleModel->index(0, 0), true, AudioRecScheduleModel::AudioRecScheduleRoles::StateRole);
                    m_scheduleTimeSecSinceEpoch = m_currentItem.endTime().toSecsSinceEpoch();
                    m_scheduledRecordingState = ScheduledRecordingState::StateRecording;
                }
                else if (QDateTime::currentSecsSinceEpoch() >= m_currentItem.endTime().toSecsSinceEpoch())
                {  // stop
                    emit audioRecordingStopped();
                    m_timer.stop();
                    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                    m_scheduleTimeSecSinceEpoch = 0;
                    if (!m_scheduleModel->isEmpty())
                    {
                        m_scheduleModel->removeRows(0, 1);
                    }
                }
            }
            break;
            case StateRecording:
            {
                int numSec = m_scheduleTimeSecSinceEpoch - QDateTime::currentSecsSinceEpoch();
                if (numSec <= 0)
                {
                    qCInfo(audioRecMgr) << "Recording finished:" << m_currentItem.name();
                    emit stopRecording();
                    m_timer.stop();
                    m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
                    m_scheduleTimeSecSinceEpoch = 0;
                    if (!m_scheduleModel->isEmpty())
                    {
                        m_scheduleModel->removeRows(0, 1);
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

void AudioRecManager::onCurrentIndexChanged()
{
    if (m_scheduleModel->currentIndex() >= 0)
    {
        const auto &item = m_scheduleModel->itemAtIndex(m_scheduleModel->index(m_scheduleModel->currentIndex(), 0));
        label(item.name());
        startTime(item.startTime().time());
        startDate(item.startTime().date());
        duration(item.duration());
        stopTime(item.startTime().addSecs(item.durationSec()).toString("dd.MM.yyyy, hh:mm"));

        m_serviceSelectionModel->clearSelection();

        for (int r = 0; r < m_slModel->rowCount(); ++r)
        {
            QModelIndex index = m_slModel->index(r, 0);
            if (m_slModel->id(index) == item.serviceId())
            {  // found
                m_serviceSelectionModel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                break;
            }
        }
    }
    else
    {
        label("");

        QDateTime startDateTime = QDateTime::currentDateTime().addSecs(60);

        // Ceil to minutes by setting seconds and milliseconds to 0, then adding 1 minute if needed
        QTime time = startDateTime.time();
        if (time.second() > 0 || time.msec() > 0)
        {
            startDateTime.setTime(QTime(time.hour(), time.minute(), 0, 0));
            startDateTime = startDateTime.addSecs(60);
        }
        startTime(startDateTime.time());
        startDate(startDateTime.date());
        duration(QTime(0, 10));
        stopTime(startDateTime.addSecs(10 * 60).toString("dd.MM.yyyy, hh:mm"));
        m_serviceSelectionModel->clearSelection();
    }
}

void AudioRecManager::updateStopTime()
{
    QDateTime start;
    start.setDate(startDate());
    start.setTime(startTime());
    QDateTime end = start.addSecs(duration().hour() * 3600 + duration().minute() * 60 + duration().second());
    stopTime(end.toString("dd.MM.yyyy, hh:mm"));
}

bool AudioRecManager::isAudioScheduleActive() const
{
    return m_scheduledRecordingState >= ScheduledRecordingState::StateReady;
}

QString AudioRecManager::audioRecordingFile() const
{
    return m_audioRecordingFile;
}

void AudioRecManager::onAudioRecordingStarted(const QString &recpath, const QString &filename)
{
    m_audioRecordingFile = filename;
    isAudioRecordingActive(true);
    if (m_settings->audioRec.dl && (nullptr == m_dlLogFile))
    {
        QFileInfo fi(filename);
        QString dlLogFilename = fi.completeBaseName() + ".txt";
        m_dlLogFile = AndroidFileHelper::openFileForWriting(recpath, dlLogFilename, "text/plain");
        if (m_dlLogFile)
        {
            QTextStream out(m_dlLogFile);
            if (!m_dlText.isEmpty())
            {
                if (m_settings->audioRec.dlAbsTime)
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
    else
    {
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
    isAudioRecordingActive(false);
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
    if (start)
    {
        m_recTimeSec = 0;
        emit startRecording();
        isAudioRecordingActive(true);
    }
    else
    {
        if (isAudioScheduleActive())
        {
            requestCancelSchedule();
        }
        emit stopRecording();
        isAudioRecordingActive(false);
    }
}

void AudioRecManager::requestCancelSchedule()
{
    if (m_scheduledRecordingState != ScheduledRecordingState::StateIdle)
    {
        qCInfo(audioRecMgr) << "Recording cancelled:" << m_currentItem.name();
        m_scheduledRecordingState = ScheduledRecordingState::StateIdle;
        m_scheduleTimeSecSinceEpoch = 0;
        m_scheduleModel->removeRows(0, 1);
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
            int min = (m_recTimeSec - hours * 3600) / 60;
            int sec = (m_recTimeSec - hours * 3600 - min * 60);
            if (m_settings->audioRec.dlAbsTime)
            {
                out << QString("%1:%2:%3\t%4\t%5\n")
                           .arg(hours, 2, 10, QChar('0'))
                           .arg(min, 2, 10, QChar('0'))
                           .arg(sec, 2, 10, QChar('0'))
                           .arg(EPGTime::getInstance()->dabTime().toString("yyyy-MM-dd-hhmmss"), dl);
            }
            else
            {
                out << QString("%1:%2:%3\t%4\n").arg(hours, 2, 10, QChar('0')).arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0')).arg(dl);
            }
        }
    }
}

void AudioRecManager::updateScheduledRecording()
{
    if (m_scheduleModel->isEmpty())
    {
        stopCurrentSchedule();
    }
    else
    {  // there is something in schedule => get first item
        const AudioRecScheduleItem item = m_scheduleModel->itemAtIndex(m_scheduleModel->index(0, 0, QModelIndex()));
        if (m_scheduledRecordingState == ScheduledRecordingState::StateRecording)
        {
            if ((item.serviceId() == m_currentItem.serviceId()) && (item.startTime() == m_currentItem.startTime()))
            {  // the same item -> update end time
                qCDebug(audioRecMgr) << "Updating end time";
                m_currentItem = item;
                m_scheduleTimeSecSinceEpoch = m_currentItem.endTime().toSecsSinceEpoch();
                return;
            }
        }

        stopCurrentSchedule();
        m_currentItem = item;
        m_scheduleTimeSecSinceEpoch = m_currentItem.startTime().toSecsSinceEpoch();
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 secToSchedule = m_scheduleTimeSecSinceEpoch - now - COUNTDOWN_SEC;
        if (secToSchedule > 0)
        {  // run timer
            m_timer.start(secToSchedule * 1000, Qt::VeryCoarseTimer, this);
        }
        else if (secToSchedule <= 0)
        {  // run callback immediately
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
