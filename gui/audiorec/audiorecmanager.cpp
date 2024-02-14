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

#include "audiorecmanager.h"
#include "epgtime.h"

AudioRecManager::AudioRecManager(AudioRecScheduleModel *model, AudioRecorder *recorder, QObject *parent)
    : QObject{parent}, m_model(model), m_recorder(recorder)
{
    m_isAudioRecordingActive = false;
    m_haveTimeConnection = false;

    connect(EPGTime::getInstance(), &EPGTime::haveValidTime, this, &AudioRecManager::onValidTime);

    connect(this, &AudioRecManager::startRecording, m_recorder, &AudioRecorder::start, Qt::QueuedConnection);
    connect(this, &AudioRecManager::stopRecording, m_recorder, &AudioRecorder::stop, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStarted, this, &AudioRecManager::onAudioRecordingStarted, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingStopped, this, &AudioRecManager::onAudioRecordingStopped, Qt::QueuedConnection);
    connect(m_recorder, &AudioRecorder::recordingProgress, this, &AudioRecManager::audioRecordingProgress, Qt::QueuedConnection);

    connect(m_model, &QAbstractItemModel::modelReset, this,  &AudioRecManager::onModelReset);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this,  &AudioRecManager::onModelRowsRemoved);
}

void AudioRecManager::onValidTime()
{   // valid time is available
    qDebug() << EPGTime::getInstance()->currentTime();

    // cleanup model -> it will remove items that are in past (emits reset)
    m_model->cleanup(EPGTime::getInstance()->currentTime());
}

void AudioRecManager::onTimeChanged()
{
    if (!m_model->isEmpty())
    {
        qDebug() << EPGTime::getInstance()->currentTime();
        //const AudioRecScheduleItem item = m_model->itemAtIndex(m_model->index(0,0, QModelIndex()));

    }
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
        emit stopRecording();
    }
}

void AudioRecManager::updateScheduledRecording()
{
    if (m_model->isEmpty())
    {
        if (m_haveTimeConnection)
        {
            disconnect(EPGTime::getInstance(), &EPGTime::secSinceEpochChanged, this, &AudioRecManager::onTimeChanged);
            m_haveTimeConnection = false;
        }
    }
    else
    {
        if (!m_haveTimeConnection)
        {
            connect(EPGTime::getInstance(), &EPGTime::secSinceEpochChanged, this, &AudioRecManager::onTimeChanged);
            m_haveTimeConnection = true;
        }
    }
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
