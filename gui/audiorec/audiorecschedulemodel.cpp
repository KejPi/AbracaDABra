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
#include "audiorecschedulemodel.h"

AudioRecScheduleModel::AudioRecScheduleModel(QObject *parent)
    : QAbstractTableModel{parent}
{

#warning "Remove this test data"
    AudioRecScheduleItem item;
    for (int n = 0; n<3; ++n)
    {
        item.setName(QString("Test label #%1").arg(n+1));
        item.setStartTime(QDateTime(QDateTime::currentDateTime().date(), QTime(13-2*n, 7, 0)));
        item.setDurationSec(3600 - 10*60*n);
        m_modelData.append(item);
    }
    sortFindConflicts();
}

AudioRecScheduleModel::AudioRecScheduleModel(const QList<AudioRecScheduleItem> &schedule, QObject *parent)
    : QAbstractTableModel{parent}, m_modelData(schedule)
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
        case ColConflict:
            return item.hasConflict() ? "!" : "";
        case ColLabel:
            return item.name();
        case ColStartTime:
            return item.startTime();
        case ColEndTime:
            return item.endTime();
        case ColDuration:
            return item.duration();
        default:
            break;
        }
    } else if (role == Qt::BackgroundRole) {
        if (item.hasConflict()) {
            return QVariant(QBrush(Qt::red));
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
        case ColConflict:
            return "";
        case ColLabel:
            return tr("Name");
        case ColStartTime:
            return tr("Start time");
        case ColEndTime:
            return tr("End time");
        case ColDuration:
            return tr("Duration");
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

// bool AudioRecScheduleModel::setData(const QModelIndex &index, const QVariant &value, int role)
// {
//     if (index.isValid() && role == Qt::EditRole)
//     {
//         const int row = index.row();
//         auto item = m_modelData.value(row);

//         switch (index.column())
//         {
//         case ColLabel:
//             item.setName(value.toString());
//             break;
//         case ColStartTime:
//             item.setStartTime(value.toDateTime());
//             break;
//         case ColEndTime:
//             //item.endTime();
//             break;
//         case ColDuration:
//             //QTime().addSecs(item.durationSec()).toString("hh:mm:ss");
//             break;
//         default:
//             return false;
//         }

//         emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

//         return true;
//     }

//     return false;
// }

bool AudioRecScheduleModel::removeRows(int position, int rows, const QModelIndex &index)
{
    Q_UNUSED(index);
    beginRemoveRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row)
    {
        m_modelData.removeAt(position);
    }

    endRemoveRows();
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
