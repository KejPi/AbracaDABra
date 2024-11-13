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
#include "txtablemodel.h"
#include "txdataloader.h"

TxTableModel::TxTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{

    TxDataLoader::loadTable(m_txList);

    connect(this, &QAbstractListModel::rowsInserted, this, &TxTableModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &TxTableModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &TxTableModel::rowCountChanged);
}

TxTableModel::~TxTableModel()
{
    qDeleteAll(m_txList);
}

int TxTableModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.count();
}

int TxTableModel::columnCount(const QModelIndex &parent) const
{
    return NumCols;
}

QVariant TxTableModel::data(const QModelIndex &index, int role) const
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
        case ColTime:
            return item.rxTime().toString("yy-MM-dd hh:mm:ss");
        case ColChannel:
            return DabTables::channelList.value(item.ensId().freq(), 0);
        case ColFreq:
            return item.ensId().freq();
        case ColEnsId:
            return QString("%1").arg(item.ensId().ueid(), 6, 16, QChar(' ')).toUpper();
        case ColEnsLabel:
            return item.ensLabel();
        case ColNumServices:
            return item.numServices();
        case ColMainId:
            return (item.mainId() != 255) ? QString::number(item.mainId()) : "";
        case ColSubId:
            return (item.subId() != 255) ? QString::number(item.subId()) : "";
        case ColLevel:
            return QString("%1 dB").arg(static_cast<double>(item.level()), 5, 'f', 1); // QString::number(static_cast<double>(item.level()), 'f', 3);
        case ColName:
            return item.transmitterData().location();
        case ColDist:
            if (item.hasTxData() && item.distance() >= 0.0)
            {
                return QString("%1 km").arg(static_cast<double>(item.distance()), 0, 'f', 1);
            }
            return QVariant("");
        case ColAzimuth:
            if (item.hasTxData() && item.azimuth() >= 0.0)
            {
                return QString("%1°").arg(static_cast<double>(item.azimuth()), 0, 'f', 1);
            }
            return QVariant("");
        }
    }
        break;
    // case Qt::TextAlignmentRole:
    //     switch (index.column())
    //     {
    //     case ColName:
    //     case ColEnsLabel:
    //         return Qt::AlignLeft;
    //     default:
    //         return Qt::AlignCenter;
    //     }
    //     break;
    case TxTableModelRoles::CoordinatesRole:
        return QVariant().fromValue(item.transmitterData().coordinates());
    case TxTableModelRoles::MainIdRole:
        return item.mainId();
    case TxTableModelRoles::SubIdRole:
        return item.subId();
    case TxTableModelRoles::TiiRole:
        return QVariant(QString("%1-%2").arg(item.mainId()).arg(item.subId()));
    case TxTableModelRoles::LevelColorRole:
        if (item.level() > -6)
        {
            //return QVariant(QColor(0x5b, 0xc2, 0x14));
            return QVariant(QColor(Qt::green));
        }
        if (item.level() > -12) {
            return QVariant(QColor(0xff, 0xb5, 0x27));
        }
        return QVariant(QColor(0xff, 0x4b, 0x4b));
    case TxTableModelRoles::ItemRole:
        return QVariant().fromValue(item);
    case TxTableModelRoles::IdRole:
        return QVariant(item.id());        
    default:
        break;
    }

    return QVariant();
}

QVariant TxTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColTime:
            return tr("RX Time");
        case ColChannel:
            return tr("Channel");
        case ColFreq:
            return tr("Frequency");
        case ColEnsId:
            return tr("UEID");
        case ColEnsLabel:
            return tr("Label");
        case ColNumServices:
            return tr("Services");
        case ColMainId:
            return tr("Main");
        case ColSubId:
            return tr("Sub");
        case ColLevel:
            return tr("Level");
        case ColName:
            return tr("Name");
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

QHash<int, QByteArray> TxTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[Qt::DisplayRole] = "display";
    roles[TxTableModelRoles::CoordinatesRole] = "coordinates";
    roles[TxTableModelRoles::TiiRole] = "tiiString";
    roles[TxTableModelRoles::MainIdRole] = "mainId";
    roles[TxTableModelRoles::SubIdRole] = "subId";
    roles[TxTableModelRoles::LevelColorRole] = "levelColor";

    return roles;
}

const TxTableModelItem &TxTableModel::itemAt(int row) const
{
    Q_ASSERT(row >= 0 && row < m_modelData.size());

    return m_modelData.at(row);
}

void TxTableModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void TxTableModel::updateData(const QList<dabsdrTii_t> &data, const ServiceListId & ensId)
{
#warning "This is not needed fro scanner tool"
#if 0
    beginResetModel();
    m_modelData.clear();
    for (const auto & tii : data)
    {
        m_modelData.append(TxTableModelItem(tii.main, tii.sub, tii.level, m_coordinates, m_txList.values(ensId)));
    }
    endResetModel();
#else
    // add new items and remove old
    int row = 0;
    QList<TxTableModelItem> appendList;
    for (int dataIdx = 0; dataIdx < data.count(); ++dataIdx)
    {
        // create new item
        TxTableModelItem item(data.at(dataIdx).main, data.at(dataIdx).sub, data.at(dataIdx).level, m_coordinates, m_txList.values(ensId));

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

    // #warning "Remove this check"
    // Q_ASSERT(m_modelData.size() == data.size());
    for (int n = 0; n < m_modelData.size(); ++n) {
        Q_ASSERT(m_modelData.at(n).mainId() == data.at(n).main);
        Q_ASSERT(m_modelData.at(n).subId() == data.at(n).sub);
        Q_ASSERT(m_modelData.at(n).level() == data.at(n).level);
    }

#endif
}

void TxTableModel::appendData(const QList<dabsdrTii_t> &data, const ServiceListId &ensId, const QString &ensLabel, int numServices)
{
    QDateTime time = QDateTime::currentDateTime();
    if (!data.empty()) {
        for (auto it = data.cbegin(); it != data.cend(); ++it)
        {
            // create new item
            TxTableModelItem item(it->main, it->sub, it->level, m_coordinates, m_txList.values(ensId));
            item.setEnsData(ensId, ensLabel, numServices);
            item.setRxTime(time);
            beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
            m_modelData.append(item);
            endInsertRows();
        }
    }
    else {
        // create new item
        TxTableModelItem item(-1, -1, 0, m_coordinates, m_txList.values(ensId));
        item.setEnsData(ensId, ensLabel, numServices);
        item.setRxTime(time);
        beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
        m_modelData.append(item);
        endInsertRows();
    }
}

void TxTableModel::setCoordinates(const QGeoCoordinate &newCoordinates)
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

#if 0

TiiTableSortModel::TiiTableSortModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, &TiiTableSortModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &TiiTableSortModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &TiiTableSortModel::rowCountChanged);

}

bool TiiTableSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    TxTableModelItem itemL = sourceModel()->data(left, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
    TxTableModelItem itemR = sourceModel()->data(right, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();

    switch (left.column())
    {
    case TxTableModel::ColMainId:
        if (itemL.mainId() == itemR.mainId())
        {
            return itemL.subId() < itemR.subId();
        }
        return itemL.mainId() < itemR.mainId();
    case TxTableModel::ColSubId:
        return itemL.subId() < itemR.subId();
    case TxTableModel::ColLevel:
        return itemL.level() < itemR.level();
    case TxTableModel::ColDist:
        return itemL.distance() < itemR.distance();
    case TxTableModel::ColAzimuth:
        return itemL.azimuth() < itemR.azimuth();
    }

    return true;
}
#endif

