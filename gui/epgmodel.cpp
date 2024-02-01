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
#include "epgmodel.h"
#include "epgtime.h"

Q_DECLARE_LOGGING_CATEGORY(metadataManager)

EPGModel::EPGModel(QObject *parent)
    : QAbstractListModel{parent}
{}

EPGModel::~EPGModel()
{
    qDeleteAll(m_itemList);
}

QVariant EPGModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    if (index.row() >= rowCount())
    {
        return QVariant();
    }
    // valid index
    const EPGModelItem * item = m_itemList.at(index.row());
    switch (static_cast<EPGModelRoles>(role)) {
    case EPGModelRoles::ShortIdRole:
        return QVariant(item->shortId());
    case EPGModelRoles::NameRole:
#if 0
        if (item->durationSec() < 10 * 60) {
            if (!item->shortName().isEmpty()) {
                return QVariant(item->shortName());
            }
        }
        if (item->durationSec() > 15 * 60) {
            if (!item->longName().isEmpty()) {
                return QVariant(item->longName());
            }
        }
#endif
        if (!item->longName().isEmpty()) {
            return QVariant(item->longName());
        }
        return QVariant(item->mediumName());

        // return QVariant(QString("%1 %2 %3-%4 [%5]").arg(item->shortId()).arg(item->startTime().toString("dd.MM hh:mm"))
        //                                                                          .arg(item->startTimeSec())
        //                                                                          .arg(item->endTimeSec())
        //                                                                          .arg(item->durationSec())
        //                                                                      );
        break;
    case LongNameRole:
        return QVariant(item->longName());
    case MediumNameRole:
        return QVariant(item->mediumName());
    case ShortNameRole:
        return QVariant(item->shortName());
    case StartTimeRole:
        return QVariant(item->startTime());
    case StartTimeStringRole:
        return QVariant(EPGTime::getInstance()->timeLocale().toString(item->startTime(), QString("dddd, dd.MM.yyyy, hh:mm")));
    case StartTimeSecRole:
        return QVariant(item->startTimeSec());
    case EndTimeSecRole:
        return QVariant(item->endTimeSec());
    case EndTimeStringRole:
        return QVariant(EPGTime::getInstance()->timeLocale().toString(item->endTime(), QString("hh:mm")));
    case DurationSecRole:
        return QVariant(item->durationSec());
    case LongDescriptionRole:
        return QVariant(item->longDescription());
    case ShortDescriptionRole:
        return QVariant(item->shortDescription());
    case StartTimeSecSinceEpochRole:
        return QVariant(item->startTimeSecSinceEpoch());
    case EPGModelRoles::EndTimeSecSinceEpochRole:
        return QVariant(item->endTimeSecSinceEpoch());
    case EPGModelRoles::StartToEndTimeStringRole:
    {
        QDateTime start = item->startTime();
        QDateTime end = item->endTime();
        QString str = EPGTime::getInstance()->timeLocale().toString(start, QString("dddd, dd.MM.yyyy, hh:mm"));
        if (start.date() != end.date()) {
            str += " - " + EPGTime::getInstance()->timeLocale().toString(end, QString("dddd, dd.MM.yyyy, hh:mm"));
        }
        else {
            str += " - " + EPGTime::getInstance()->timeLocale().toString(end, QString("hh:mm"));
        }
        return QVariant(str);
    }
    }

    return QVariant();
}

QHash<int, QByteArray> EPGModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[EPGModelRoles::ShortIdRole] = "shortId";
    roles[EPGModelRoles::NameRole] = "name";
    roles[EPGModelRoles::LongNameRole] = "longName";
    roles[EPGModelRoles::MediumNameRole] = "mediumName";
    roles[EPGModelRoles::ShortNameRole] = "shortName";
    roles[EPGModelRoles::StartTimeRole] = "startTime";
    roles[EPGModelRoles::StartTimeStringRole] = "startTimeString";
    roles[EPGModelRoles::StartTimeSecRole] = "startTimeSec";    
    roles[EPGModelRoles::StartTimeSecSinceEpochRole] = "startTimeSecSinceEpoch";
    roles[EPGModelRoles::EndTimeSecRole] = "endTimeSec";
    roles[EPGModelRoles::EndTimeStringRole] = "endTimeString";
    roles[EPGModelRoles::EndTimeSecSinceEpochRole] = "endTimeSecSinceEpoch";
    roles[EPGModelRoles::StartToEndTimeStringRole] = "startToEndTimeString";
    roles[EPGModelRoles::DurationSecRole] = "durationSec";
    roles[EPGModelRoles::LongDescriptionRole] = "longDescription";
    roles[EPGModelRoles::ShortDescriptionRole] = "shortDescription";    

    return roles;
}

void EPGModel::populateWithList(const QList<EPGModelItem *> &list)
{

}

bool EPGModel::addItem(EPGModelItem *item)
{
    if (item->isValid())
    {
        if (m_startTimeList.contains(item->startTime()))
        {   // already in list
            if (item->shortId() != m_startTimeList[item->startTime()]) {
                qCDebug(metadataManager) << "Unexpected EPG item ID" << item->shortId() << "start time:" << item->startTime();
            }
            delete item;
            return false;
        }

        // we are here if item was not in list
        beginInsertRows(QModelIndex(), m_itemList.size(), m_itemList.size());
        m_startTimeList[item->startTime()] = item->shortId();
        m_itemList.append(item);
        endInsertRows();

        return true;
    }
    else
    {
        qCDebug(metadataManager) << "Invalid item:" << item->shortId();
    }
    return false;
}
