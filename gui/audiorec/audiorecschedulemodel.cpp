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

#include "audiorecschedulemodel.h"

#include <QBrush>
#include <QLoggingCategory>
#include <QPixmap>
#include <QTime>

Q_DECLARE_LOGGING_CATEGORY(audioRecMgr)

AudioRecScheduleModel::AudioRecScheduleModel(QObject *parent) : QAbstractListModel{parent}, m_slModel(nullptr)
{
    connect(this, &QAbstractListModel::rowsInserted, this, &AudioRecScheduleModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &AudioRecScheduleModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &AudioRecScheduleModel::rowCountChanged);
    connect(this, &AudioRecScheduleModel::rowCountChanged, this, [this]() { setCurrentIndex(-1); });
}

int AudioRecScheduleModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.size();
}

QVariant AudioRecScheduleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.row() >= m_modelData.size() || index.row() < 0)
    {
        return QVariant();
    }

    const auto &item = m_modelData.at(index.row());
    switch (role)
    {
        case StateRole:
            if (item.isRecorded())
            {
                return QString("icon-recording.svg");
            }
            if (item.hasConflict())
            {
                return QString("alert.svg");
            }
            return QString();
        case LabelRole:
            return item.name();
        case StartTimeRole:
            return item.startTime().toString("dd.MM.yyyy hh:mm");
        case EndTimeRole:
            return item.endTime().toString("dd.MM.yyyy hh:mm");
        case DurationRole:
            return item.duration().toString("hh:mm");
        case ServiceRole:
        {
            if (m_slModel == nullptr)
            {
                return QString("%1").arg(item.serviceId().sid(), 6, 16, QChar('0')).toUpper();
            }
            const ServiceList *slPtr = m_slModel->getServiceList();
            const auto it = slPtr->findService(item.serviceId());
            if (it != slPtr->serviceListEnd())
            {
                return it.value()->label();
            }
            else
            {
                return QString("%1").arg(item.serviceId().sid(), 6, 16, QChar('0')).toUpper();
            }
        }
        default:
            return QVariant();
    }
}

bool AudioRecScheduleModel::removeRows(int position, int rows, const QModelIndex &index)
{
    Q_UNUSED(index);
    beginResetModel();
    for (int row = 0; row < rows; ++row)
    {
        m_modelData.removeAt(position);
    }
    sortFindConflicts();
    endResetModel();
    return true;
}

void AudioRecScheduleModel::insertItem(const AudioRecScheduleItem &item)
{
    beginResetModel();
    m_modelData.append(item);
    sortFindConflicts();
    endResetModel();
}

void AudioRecScheduleModel::replaceItemAtIndex(const QModelIndex &index, const AudioRecScheduleItem &item)
{
    if (index.isValid())
    {
        beginResetModel();
        m_modelData[index.row()] = item;
        sortFindConflicts();
        endResetModel();
    }
    else
    {
        qDebug() << "Invalid index in replaceItemAtIndex";
    }
}

const AudioRecScheduleItem &AudioRecScheduleModel::itemAtIndex(const QModelIndex &index) const
{
    return m_modelData.at(index.row());
}

void AudioRecScheduleModel::setSlModel(SLModel *newSlModel)
{
    m_slModel = newSlModel;
}

void AudioRecScheduleModel::load(const QString &filename)
{
    QFile loadFile(filename);
    if (!loadFile.exists())
    {  // file does not exist -> do nothing
        return;
    }

    // file exists here
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        qCWarning(audioRecMgr) << "Unable to read audio recording schedule settings file";
        return;
    }

    QByteArray data = loadFile.readAll();
    loadFile.close();

    if (data.isEmpty())
    {  // no data in the file
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray())
    {
        qCWarning(audioRecMgr) << "Unable to read audio recording schedule settings file";
        return;
    }

    beginResetModel();
    m_modelData.clear();
    QVariantList list = doc.toVariant().toList();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {  // loading the list
        auto map = it->toMap();

        AudioRecScheduleItem item;
        item.setName(map.value("Name").toString());
        item.setStartTime(map.value("StartTime").value<QDateTime>());
        item.setDurationSec(map.value("DurationSec").toInt());
        item.setServiceId(map.value("ServiceId").value<uint64_t>());
        m_modelData.append(item);
    }
    cleanup(QDateTime::currentDateTime());
    sortFindConflicts();
    endResetModel();
}

void AudioRecScheduleModel::loadFromSettings(QSettings *settings)
{
    beginResetModel();
    m_modelData.clear();
    int numItems = settings->beginReadArray("AudioRecordingSchedule");
    for (int n = 0; n < numItems; ++n)
    {
        AudioRecScheduleItem item;
        settings->setArrayIndex(n);
        item.setName(settings->value("Name").toString());
        item.setStartTime(settings->value("StartTime").value<QDateTime>());
        item.setDurationSec(settings->value("DurationSec").toInt());
        item.setServiceId(settings->value("ServiceId").value<uint64_t>());
        m_modelData.append(item);
    }
    settings->endArray();
    cleanup(QDateTime::currentDateTime());
    sortFindConflicts();
    endResetModel();
}

void AudioRecScheduleModel::save(const QString &filename)
{
    QVariantList list;
    for (const auto &item : std::as_const(m_modelData))
    {
        QVariantMap map;
        map["Name"] = item.name();
        map["StartTime"] = item.startTime();
        map["DurationSec"] = item.durationSec();
        map["ServiceId"] = QVariant::fromValue(item.serviceId().value());
        list.append(map);
    }

    QDir().mkpath(QFileInfo(filename).path());
    QFile saveFile(filename);
    if (!saveFile.open(QIODevice::WriteOnly))
    {
        qCWarning(audioRecMgr) << "Failed to save audio recording schedule to file.";
        return;
    }
    saveFile.write(QJsonDocument::fromVariant(list).toJson());
    saveFile.close();
}

void AudioRecScheduleModel::cleanup(const QDateTime &currentTime)
{
    auto it = m_modelData.constBegin();
    while (it != m_modelData.constEnd())
    {
        if (it->endTime() <= currentTime)
        {
            it = m_modelData.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void AudioRecScheduleModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();

    setCurrentIndex(-1);
}

QHash<int, QByteArray> AudioRecScheduleModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[StateRole] = "stateIcon";
    roles[LabelRole] = "label";
    roles[StartTimeRole] = "startTime";
    roles[EndTimeRole] = "endTime";
    roles[DurationRole] = "duration";
    roles[ServiceRole] = "service";
    return roles;
}

void AudioRecScheduleModel::sortFindConflicts()
{
    std::sort(m_modelData.begin(), m_modelData.end());
    QDateTime endTime;
    endTime.setSecsSinceEpoch(0);
    for (auto &item : m_modelData)
    {
        if (item.startTime() >= endTime)
        {
            item.setHasConflict(false);
            endTime = item.endTime();
        }
        else
        {
            item.setHasConflict(true);
            if (item.endTime() > endTime)
            {
                endTime = item.endTime();
            }
        }
    }
}

bool AudioRecScheduleModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == StateRole)
    {
        const int row = index.row();
        m_modelData[row].setIsRecorded(value.toBool());
        emit dataChanged(index, index, {StateRole});
        return true;
    }
    return false;
}

void AudioRecScheduleModel::setCurrentIndex(int currentIndex)
{
    if (m_currentIndex == currentIndex || currentIndex >= rowCount())
    {
        return;
    }
    m_currentIndex = currentIndex;
    emit currentIndexChanged();
}
