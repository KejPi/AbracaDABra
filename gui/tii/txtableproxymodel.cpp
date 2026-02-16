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

#include "txtableproxymodel.h"

#include "txtablemodel.h"

TxTableProxyModel::TxTableProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, &TxTableProxyModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &TxTableProxyModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &TxTableProxyModel::rowCountChanged);
}

void TxTableProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel())
    {
        disconnect(sourceModel(), &QAbstractItemModel::dataChanged, this, &TxTableProxyModel::onSourceDataChanged);
    }
    QSortFilterProxyModel::setSourceModel(model);
    if (model)
    {
        connect(model, &QAbstractItemModel::dataChanged, this, &TxTableProxyModel::onSourceDataChanged);
    }
}

void TxTableProxyModel::setColumnsFilter(bool filterCols)
{
    if (filterCols != m_filterCols)
    {
        m_filterCols = filterCols;
        invalidateColumnsFilter();
    }
}

void TxTableProxyModel::setInactiveTxFilter(bool filterInactiveTx)
{
    if (filterInactiveTx != m_filterInactiveTx)
    {
        m_filterInactiveTx = filterInactiveTx;
        invalidateRowsFilter();
    }
}

void TxTableProxyModel::setLocalTxFilter(bool filterLocalTx)
{
    if (filterLocalTx != m_filterLocalTx)
    {
        m_filterLocalTx = filterLocalTx;
        invalidateRowsFilter();
    }
}

void TxTableProxyModel::setColumns(const Settings::TIISettings::TxTableSettings &settings)
{
    if (m_filterCols == false)
    {
        return;
    }
    // settings is only applied when columns are filtered by the model => TII table
    m_columnsSettings = settings;

    // Code and Level are always included
    QMap<int, int> orderMap;  // order -> source column
    orderMap.insert(m_columnsSettings.code.order, TxTableModel::ColCode);
    orderMap.insert(m_columnsSettings.level.order, TxTableModel::ColLevel);
    if (m_columnsSettings.location.enabled)
    {
        orderMap.insert(m_columnsSettings.location.order, TxTableModel::ColLocation);
    }
    if (m_columnsSettings.power.enabled)
    {
        orderMap.insert(m_columnsSettings.power.order, TxTableModel::ColPower);
    }
    if (m_columnsSettings.dist.enabled)
    {
        orderMap.insert(m_columnsSettings.dist.order, TxTableModel::ColDist);
    }
    if (m_columnsSettings.azimuth.enabled)
    {
        orderMap.insert(m_columnsSettings.azimuth.order, TxTableModel::ColAzimuth);
    }

    QList<int> colOrder = orderMap.values();
    if (colOrder != m_columnOrder)
    {
        beginResetModel();
        m_columnOrder = colOrder;
        endResetModel();
    }
    invalidateColumnsFilter();
}

bool TxTableProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    int sourceCol = left.column();
    if (m_filterCols)
    {
        int sortCol = sortColumn();
        sourceCol = sourceColumnForProxyColumn(sortCol);
    }

    TxTableModelItem itemL = sourceModel()->data(left, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
    TxTableModelItem itemR = sourceModel()->data(right, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();

    switch (sourceCol)
    {
        case TxTableModel::ColMainId:
            if (itemL.mainId() == itemR.mainId())
            {
                if (!m_filterCols && (itemL.subId() == itemR.subId()))
                {
                    return itemL.rxTime() < itemR.rxTime();
                }
                return itemL.subId() < itemR.subId();
            }
            return itemL.mainId() < itemR.mainId();
        case TxTableModel::ColSubId:
            if (!m_filterCols && (itemL.subId() == itemR.subId()))
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.subId() < itemR.subId();
        case TxTableModel::ColCode:
            if (itemL.mainId() == itemR.mainId())
            {
                if (!m_filterCols && (itemL.subId() == itemR.subId()))
                {
                    return itemL.rxTime() < itemR.rxTime();
                }
                return itemL.subId() < itemR.subId();
            }
            return itemL.mainId() < itemR.mainId();
        case TxTableModel::ColLevel:
            if (!m_filterCols && (itemL.level() == itemR.level()))
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.level() < itemR.level();
        case TxTableModel::ColDist:
            if (!m_filterCols && (itemL.distance() == itemR.distance()))
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.distance() < itemR.distance();
        case TxTableModel::ColAzimuth:
            if (!m_filterCols && (itemL.azimuth() == itemR.azimuth()))
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.azimuth() < itemR.azimuth();
        // scanner cols
        case TxTableModel::ColFreq:
        case TxTableModel::ColChannel:
            if (itemL.ensId().freq() == itemR.ensId().freq())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.ensId().freq() < itemR.ensId().freq();
        case TxTableModel::ColLocation:
            return itemL.transmitterData().location() < itemR.transmitterData().location();
        case TxTableModel::ColTime:
            return itemL.rxTime() < itemR.rxTime();
        case TxTableModel::ColEnsId:
            if (itemL.ensId().ueid() == itemR.ensId().ueid())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.ensId().ueid() < itemR.ensId().ueid();
        case TxTableModel::ColEnsLabel:
            if (itemL.ensLabel() == itemR.ensLabel())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.ensLabel() < itemR.ensLabel();
        case TxTableModel::ColNumServices:
            if (itemL.numServices() == itemR.numServices())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.numServices() < itemR.numServices();
        case TxTableModel::ColSnr:
            if (itemL.snr() == itemR.snr())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.snr() < itemR.snr();
        case TxTableModel::ColPower:
            if (itemL.power() == itemR.power())
            {
                return itemL.rxTime() < itemR.rxTime();
            }
            return itemL.power() < itemR.power();
    }

    return true;
}

bool TxTableProxyModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);

    if (!m_filterCols)
    {  // we do not display coordinates, these are only used for export to CSV
        // we do not display code in scanner mode
        return source_column < TxTableModel::NumColsWithoutCoordinates && source_column != TxTableModel::ColCode;
    }

    // When m_filterCols is true, accept ALL source columns.
    // Column filtering + reordering is handled by columnCount() + mapToSource()/mapFromSource().
    return true;
}

bool TxTableProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = sourceModel()->index(source_row, 0);
    if (m_filterLocalTx)
    {  // filter known/local
        return sourceModel()->data(idx, TxTableModel::TxTableModelRoles::IsLocalRole).toBool() == false;
    }
    if (m_filterInactiveTx)
    {  // filter not active
        return sourceModel()->data(idx, TxTableModel::TxTableModelRoles::IsActiveRole).toBool() == true;
    }
    else
    {
        auto item = sourceModel()->data(idx, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
        return sourceModel()->data(idx, TxTableModel::TxTableModelRoles::IsActiveRole).toBool() == true || item.hasTxData();
    }
    return true;
}

int TxTableProxyModel::sourceColumnForProxyColumn(int proxyColumn) const
{
    if (!m_filterCols || proxyColumn < 0 || proxyColumn >= m_columnOrder.size())
    {
        return proxyColumn;
    }
    return m_columnOrder.at(proxyColumn);
}

int TxTableProxyModel::proxyColumnForSourceColumn(int sourceColumn) const
{
    if (!m_filterCols)
    {
        return sourceColumn;
    }
    return m_columnOrder.indexOf(sourceColumn);
}

QModelIndex TxTableProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
    {
        return QModelIndex();
    }

    if (!m_filterCols)
    {
        return QSortFilterProxyModel::mapToSource(proxyIndex);
    }

    // Create a new proxy index with the remapped column, then let the base class handle row mapping
    int sourceCol = sourceColumnForProxyColumn(proxyIndex.column());
    QModelIndex remapped = createIndex(proxyIndex.row(), sourceCol, proxyIndex.internalPointer());

    // We need to use the base class but with the remapped column
    // The base class handles row filtering/sorting, so we map row first then fix column
    QModelIndex baseMapped = QSortFilterProxyModel::mapToSource(index(proxyIndex.row(), 0, proxyIndex.parent()));

    if (!baseMapped.isValid())
    {
        return QModelIndex();
    }

    return sourceModel()->index(baseMapped.row(), sourceCol, baseMapped.parent());
}

QModelIndex TxTableProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
    {
        return QModelIndex();
    }

    if (!m_filterCols)
    {
        return QSortFilterProxyModel::mapFromSource(sourceIndex);
    }

    // Map row through base class (using column 0 for reliable mapping)
    QModelIndex baseMapped = QSortFilterProxyModel::mapFromSource(sourceModel()->index(sourceIndex.row(), 0, sourceIndex.parent()));

    if (!baseMapped.isValid())
    {
        return QModelIndex();
    }

    int proxyCol = proxyColumnForSourceColumn(sourceIndex.column());
    if (proxyCol < 0)
    {
        return QModelIndex();
    }

    return index(baseMapped.row(), proxyCol, baseMapped.parent());
}

int TxTableProxyModel::columnCount(const QModelIndex &parent) const
{
    if (m_filterCols)
    {
        return m_columnOrder.size();
    }
    return QSortFilterProxyModel::columnCount(parent);
}

QVariant TxTableProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && m_filterCols)
    {
        int sourceCol = sourceColumnForProxyColumn(section);
        return sourceModel()->headerData(sourceCol, orientation, role);
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

void TxTableProxyModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles)
{
    if (!m_filterCols)
    {
        // Non-reordered mode: the base class handles it
        return;
    }

    // Map source rows to proxy rows
    QModelIndex proxyTopLeft = mapFromSource(sourceModel()->index(topLeft.row(), 0));
    QModelIndex proxyBottomRight = mapFromSource(sourceModel()->index(bottomRight.row(), 0));

    if (!proxyTopLeft.isValid() || !proxyBottomRight.isValid())
    {
        return;
    }

    // Emit dataChanged for the full proxy column range
    int firstProxyRow = qMin(proxyTopLeft.row(), proxyBottomRight.row());
    int lastProxyRow = qMax(proxyTopLeft.row(), proxyBottomRight.row());
    int lastProxyCol = columnCount() - 1;

    emit dataChanged(index(firstProxyRow, 0), index(lastProxyRow, lastProxyCol), roles);
}
