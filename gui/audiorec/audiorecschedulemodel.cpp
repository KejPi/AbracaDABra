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

#include <QTime>
#include <QBrush>
#include <QPixmap>
#include "audiorecschedulemodel.h"

AudioRecScheduleModel::AudioRecScheduleModel(QObject *parent)
    : QAbstractTableModel{parent}, m_slModel(nullptr)
{
}

int AudioRecScheduleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_modelData.size();
}

int AudioRecScheduleModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : NumColumns;
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
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColState:
            // if (item.isRecorded())
            // {

            // }
            // else if (item.hasConflict())
            // {
            //     return  "!";
            // }
            break;
        case ColLabel:
            return item.name();
        case ColStartTime:
            return item.startTime();
        case ColEndTime:
            return item.endTime();
        case ColDuration:
            return item.duration();
        case ColService: {
            if (m_slModel == nullptr) {
                return QString("%1").arg(item.serviceId().sid(), 6, 16, QChar('0')).toUpper();
            }
            const ServiceList * slPtr = m_slModel->getServiceList();
            const auto it = slPtr->findService(item.serviceId());
            if (it != slPtr->serviceListEnd()) {
                return it.value()->label();
            }
            else {
                return QString("%1").arg(item.serviceId().sid(), 6, 16, QChar('0')).toUpper();
            }
        }
            break;
        default:
            break;
        }
    }
    // else if (role == Qt::ForegroundRole)
    // {
    //     if (item.hasConflict() && ((index.column() == ColStartTime) || (index.column() == ColEndTime))) {

    //         return QVariant(QBrush(QColor(0xff,0x3a,0x3a)));
    //     }
    // }
    else if (role == Qt::DecorationRole)
    {
        if ((index.column() == ColState) && item.isRecorded()) {
            return QPixmap(":/resources/record.png");
        }
        if ((index.column() == ColState) && item.hasConflict()) {
            return QPixmap(":/resources/conflict.png");
        }
    }
    return QVariant();
}

QVariant AudioRecScheduleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColState:
            return "";
        case ColLabel:
            return tr("Name");
        case ColStartTime:
            return tr("Start time");
        case ColEndTime:
            return tr("End time");
        case ColDuration:
            return tr("Duration");
        case ColService:
            return tr("Service");
        default:
            break;
        }
    }
    return QVariant();
}

Qt::ItemFlags AudioRecScheduleModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return Qt::ItemIsEnabled;
    }

    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

bool AudioRecScheduleModel::removeRows(int position, int rows, const QModelIndex &index)
{
    Q_UNUSED(index);
    //beginRemoveRows(QModelIndex(), position, position + rows - 1);
    beginResetModel();
    for (int row = 0; row < rows; ++row)
    {
        m_modelData.removeAt(position);
    }
    sortFindConflicts();
    endResetModel();
    //endRemoveRows();
    return true;
}

const QList<AudioRecScheduleItem> &AudioRecScheduleModel::getSchedule() const
{
    return m_modelData;
}

void AudioRecScheduleModel::insertItem(const AudioRecScheduleItem &item)
{
    beginResetModel();
    m_modelData.append(item);
    sortFindConflicts();
    endResetModel();
}

void AudioRecScheduleModel::replaceItemAtIndex(const QModelIndex & index, const AudioRecScheduleItem & item)
{
    beginResetModel();
    m_modelData[index.row()] = item;
    sortFindConflicts();
    endResetModel();
}

const AudioRecScheduleItem &AudioRecScheduleModel::itemAtIndex(const QModelIndex &index) const
{
    return m_modelData.at(index.row());
}

void AudioRecScheduleModel::setSlModel(SLModel *newSlModel)
{
    m_slModel = newSlModel;
}

void AudioRecScheduleModel::load(QSettings &settings)
{
    int numItems = settings.beginReadArray("AudioRecordingSchedule");
    for (int n = 0; n<numItems; ++n)
    {
        AudioRecScheduleItem item;
        settings.setArrayIndex(n);
        item.setName(settings.value("Name").toString());
        item.setStartTime(settings.value("StartTime").value<QDateTime>());
        item.setDurationSec(settings.value("DurationSec").toInt());
        item.setServiceId(settings.value("ServiceId").value<uint64_t>());
        m_modelData.append(item);
    }
    settings.endArray();
    sortFindConflicts();
}

void AudioRecScheduleModel::save(QSettings &settings)
{
    settings.beginWriteArray("AudioRecordingSchedule", m_modelData.size());
    int n = 0;
    for (const auto & item : m_modelData)
    {
        settings.setArrayIndex(n++);
        settings.setValue("Name", item.name());
        settings.setValue("StartTime", item.startTime());
        settings.setValue("DurationSec", item.durationSec());
        settings.setValue("ServiceId", QVariant::fromValue(item.serviceId().value()));
    }
    settings.endArray();
}

void AudioRecScheduleModel::cleanup(const QDateTime &currentTime)
{
    beginResetModel();
    auto it = m_modelData.constBegin();
    while (it != m_modelData.constEnd())
    {
        qDebug() << it->endTime();
        if (it->endTime() <= currentTime)
        {
            it = m_modelData.erase(it);
        }
        else
        {
            ++it;
        }
    }
    endResetModel();
}

void AudioRecScheduleModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void AudioRecScheduleModel::sortFindConflicts()
{
    std::sort(m_modelData.begin(), m_modelData.end());
    QDateTime endTime;
    endTime.setSecsSinceEpoch(0);
    for (auto & item : m_modelData) {
        if (item.startTime() >= endTime)
        {
            item.setHasConflict(false);
            endTime = item.endTime();
        }
        else
        {
            item.setHasConflict(true);
            if (item.endTime() > endTime) {
                endTime = item.endTime();
            }
        }
    }
}

bool AudioRecScheduleModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        const int row = index.row();
        switch (index.column()) {
        case ColState:
            m_modelData[row].setIsRecorded(value.toBool());
            break;
        default:
            return false;
        }
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

        return true;
    }

    return false;
}
