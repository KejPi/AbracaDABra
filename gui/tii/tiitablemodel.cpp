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
#include "QtCore/qassert.h"
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
            if (item.hasTxData())
            {
                return QString::number(static_cast<double>(item.distance()), 'f', 1);
            }
            return QVariant();
        case ColAzimuth:
            if (item.hasTxData())
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
    case TiiTableModelRoles::ItemRole:
        return QVariant().fromValue(item);
    case TiiTableModelRoles::IdRole:
        return QVariant(item.id());
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

void TiiTableModel::updateData(const QList<dabsdrTii_t> &data, const ServiceListId & ensId)
{
#if 0
    beginResetModel();
    m_modelData.clear();
    for (const auto & tii : data)
    {
        m_modelData.append(TiiTableModelItem(tii.main, tii.sub, tii.level, m_coordinates, m_txList.values(ensId)));
    }
    endResetModel();
#else
    // add new items and remove old
    int row = 0;
    QList<TiiTableModelItem> appendList;
    for (int dataIdx = 0; dataIdx < data.count(); ++dataIdx)
    {
        // create new item
        TiiTableModelItem item(data.at(dataIdx).main, data.at(dataIdx).sub, data.at(dataIdx).level, m_coordinates, m_txList.values(ensId));

        if (row < m_modelData.size())
        {
            int startIdx = row;
            int numToRemove = 0;
            while ((row < m_modelData.size()) && (m_modelData.at(row).id() < item.id()))
            {
                row += 1;
                numToRemove += 1;
            }
            if (numToRemove > 0)
            {
                beginRemoveRows(QModelIndex(), startIdx, startIdx + numToRemove - 1);
                m_modelData.remove(startIdx, numToRemove);
                endRemoveRows();
                row = startIdx;
            }

            if (row < m_modelData.size())
            {   // we are still not at the end
                // all id < new id were removed
                if (m_modelData.at(row).id() == item.id())
                {   // equal ID ==> update item
                    m_modelData[row] = item;
                    // inform views
                    emit dataChanged(index(row, ColLevel), index(row, ColAzimuth));
                }
                else
                {   // insert item
                    beginInsertRows(QModelIndex(), row, row);
                    m_modelData.insert(row, item);
                    endInsertRows();
                }
                row += 1;
            }
            else
            {   // we are at the end of m_model data -> insert remaining items
                appendList.append(item);
            }
        }
        else
        {   // we are at the end of m_model data -> insert remaining items
            appendList.append(item);
        }
    }

    // final clean up
    if (appendList.isEmpty())
    {   // nothing to append
        if (row < m_modelData.size())
        {   // some rows to remove
            beginRemoveRows(QModelIndex(), row, m_modelData.size() - 1);
            m_modelData.remove(row, m_modelData.size() - row);
            endRemoveRows();
        }
    }
    else
    {   // rows to append
        beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size() + appendList.count() - 1);
        m_modelData.append(appendList);
        endInsertRows();
    }

#warning "Remove this check"
    Q_ASSERT(m_modelData.size() == data.size());
    for (int n = 0; n < m_modelData.size(); ++n) {
        Q_ASSERT(m_modelData.at(n).mainId() == data.at(n).main);
        Q_ASSERT(m_modelData.at(n).subId() == data.at(n).sub);
        Q_ASSERT(m_modelData.at(n).level() == data.at(n).level);
    }

#endif
}

void TiiTableModel::setCoordinates(const QGeoCoordinate &newCoordinates)
{
    if (newCoordinates != m_coordinates)
    {
        m_coordinates = newCoordinates;

        int row = 0;
        for (auto & item : m_modelData)
        {
            if (item.hasTxData())
            {
                item.updateGeo(m_coordinates);
                emit dataChanged(index(row, ColDist), index(row, ColAzimuth));
            }
            row += 1;
        }
    }
}



TiiTableSortModel::TiiTableSortModel(QObject *parent) : QSortFilterProxyModel(parent) {}

bool TiiTableSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    TiiTableModelItem itemL = sourceModel()->data(left, TiiTableModel::TiiTableModelRoles::ItemRole).value<TiiTableModelItem>();
    TiiTableModelItem itemR = sourceModel()->data(right, TiiTableModel::TiiTableModelRoles::ItemRole).value<TiiTableModelItem>();

    switch (left.column())
    {
    case TiiTableModel::ColMainId:
        if (itemL.mainId() == itemR.mainId())
        {
            return itemL.subId() < itemR.subId();
        }
        return itemL.mainId() < itemR.mainId();
    case TiiTableModel::ColSubId:
        return itemL.subId() < itemR.subId();
    case TiiTableModel::ColLevel:
        return itemL.level() < itemR.level();
    case TiiTableModel::ColDist:
        return itemL.distance() < itemR.distance();
    case TiiTableModel::ColAzimuth:
        return itemL.azimuth() < itemR.azimuth();
    }

    return true;
}


