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

#include <QColor>
#include "tiitablemodel.h"
#include "txdataloader.h"

TiiTableModel::TiiTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{

    TxDataLoader::loadTable(m_txList);
}

int TiiTableModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.count();
}

int TiiTableModel::columnCount(const QModelIndex &parent) const
{
    return NumCols;
}

QVariant TiiTableModel::data(const QModelIndex &index, int role) const
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
    case Qt::DisplayRole: {
        switch (index.column())
        {
        case ColMainId:
            return item.mainId();
        case ColSubId:
            return item.subId();
        case ColLevel:
            return QString::number(static_cast<double>(item.level()), 'f', 3);
        case ColDist:
            if (item.haveTxData())
            {
                return QString::number(static_cast<double>(item.distance()), 'f', 1);
            }
            return QVariant();
        case ColAzimuth:
            if (item.haveTxData())
            {
                return QString::number(static_cast<double>(item.azimuth()), 'f', 1);
            }
            return QVariant();
        }
    }
        break;
    case TiiTableModelRoles::CoordinatesRole:
        return QVariant().fromValue(item.transmitterData().coordinates());
    case TiiTableModelRoles::MainIdRole:
        return item.mainId();
    case TiiTableModelRoles::SubIdRole:
        return item.subId();
    case TiiTableModelRoles::TiiRole:
        return QVariant(QString("%1-%2").arg(item.mainId()).arg(item.subId()));
    case TiiTableModelRoles::LevelColorRole:
        if (item.level() > 0.6)
        {
            //return QVariant(QColor(0x5b, 0xc2, 0x14));
            return QVariant(QColor(Qt::green));
        }
        if (item.level() > 0.2) {
            return QVariant(QColor(0xff, 0xb5, 0x27));
        }
        return QVariant(QColor(0xff, 0x4b, 0x4b));
    default:
        break;
    }

    return QVariant();
}

QVariant TiiTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColMainId:
            return tr("Main");
        case ColSubId:
            return tr("Sub");
        case ColLevel:
            return tr("Level");
        case ColDist:
            return tr("Distance");
        case ColAzimuth:
            return tr("Azimuth");
        default:
            break;
        }
    }
    return QVariant();
}

QHash<int, QByteArray> TiiTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[TiiTableModelRoles::CoordinatesRole] = "coordinates";
    roles[TiiTableModelRoles::TiiRole] = "tiiString";
    roles[TiiTableModelRoles::MainIdRole] = "mainId";
    roles[TiiTableModelRoles::SubIdRole] = "subId";
    roles[TiiTableModelRoles::LevelColorRole] = "levelColor";

    return roles;
}

void TiiTableModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void TiiTableModel::populateModel(const QList<dabsdrTii_t> &data, const ServiceListId & ensId)
{
#if 1
    beginResetModel();
    m_modelData.clear();

    for (const auto & tii : data)
    {
        TiiTableModelItem item;
        item.setMainId(tii.main);
        item.setSubId(tii.sub);
        item.setLevel(tii.level);

        QList<TxDataItem *> txItemList = m_txList.values(ensId);
        TxDataItem * tx = nullptr;
        for (const auto & txItem : txItemList)
        {
            if ((txItem->subId() == tii.sub) && (txItem->mainId() == tii.main))
            {
                tx = txItem;
                break;
            }
        }
        if (tx != nullptr)
        {   // found transmitter record
            item.setTransmitterData(*tx);
            qDebug() << item.mainId() << item.subId() << item.transmitterData().location();

            if (m_coordinates.isValid())
            {   // we can calculate distance and azimuth
                item.setDistance(m_coordinates.distanceTo(item.transmitterData().coordinates()) * 0.001);
                item.setAzimuth(m_coordinates.azimuthTo(item.transmitterData().coordinates()));
            }
        }

        m_modelData.append(item);
    }

    endResetModel();
#else
    if (m_modelData.isEmpty())
    {
        beginResetModel();
        m_modelData.clear();
        m_modelData = data;
        endResetModel();
    }
    else
    {
        beginRemoveRows(QModelIndex(), qMax(m_modelData.count() - 3, 0), qMax(m_modelData.count() - 3, 0));
        m_modelData.removeAt(qMax(m_modelData.count() - 3, 0));
        endRemoveRows();
    }
#endif
}

void TiiTableModel::setCoordinates(const QGeoCoordinate &newCoordinates)
{
    if (newCoordinates != m_coordinates)
    {
        m_coordinates = newCoordinates;
    }
}


