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

// ========== TxTableProxyModel ==========

TxTableProxyModel::TxTableProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, &TxTableProxyModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &TxTableProxyModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &TxTableProxyModel::rowCountChanged);
}

void TxTableProxyModel::setColumnsFilter(bool filterCols)
{
    if (filterCols != m_filterCols)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif

        m_filterCols = filterCols;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Columns);
#else
        invalidateColumnsFilter();
#endif
    }
}

void TxTableProxyModel::setInactiveTxFilter(bool filterInactiveTx)
{
    if (filterInactiveTx != m_filterInactiveTx)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif

        m_filterInactiveTx = filterInactiveTx;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateRowsFilter();
#endif
    }
}

void TxTableProxyModel::setLocalTxFilter(bool filterLocalTx)
{
    if (filterLocalTx != m_filterLocalTx)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif
        m_filterLocalTx = filterLocalTx;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateRowsFilter();
#endif
    }
}

void TxTableProxyModel::setRfLevelFilter(bool filterRfLevel)
{
    if (filterRfLevel != m_filterRfLevel)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif
        m_filterRfLevel = filterRfLevel;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange(QSortFilterProxyModel::Direction::Columns);
#else
        invalidateColumnsFilter();
#endif
        emit rfLevelFilterChanged();
    }
}

bool TxTableProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    int sourceCol = left.column();

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
        if (m_filterRfLevel && source_column == TxTableModel::ColRfLevel)
        {
            return false;
        }
        return source_column <= TxTableModel::LastColumnWithoutCoordinates;
    }

    // When m_filterCols is true (TII mode), accept all columns.
    // Column filtering and reordering is handled by TxTableColumnProxyModel on top.
    return true;
}

bool TxTableProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
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

// ========== TxTableColumnProxyModel ==========

TxTableColumnProxyModel::TxTableColumnProxyModel(QObject *parent) : QAbstractProxyModel(parent)
{}

void TxTableColumnProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel())
    {
        disconnect(sourceModel(), nullptr, this, nullptr);
    }

    QAbstractProxyModel::setSourceModel(model);

    if (model)
    {
        connect(model, &QAbstractItemModel::dataChanged, this, &TxTableColumnProxyModel::onSourceDataChanged);
        connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &TxTableColumnProxyModel::onSourceRowsAboutToBeInserted);
        connect(model, &QAbstractItemModel::rowsInserted, this, &TxTableColumnProxyModel::onSourceRowsInserted);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &TxTableColumnProxyModel::onSourceRowsAboutToBeRemoved);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &TxTableColumnProxyModel::onSourceRowsRemoved);
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &TxTableColumnProxyModel::onSourceModelAboutToBeReset);
        connect(model, &QAbstractItemModel::modelReset, this, &TxTableColumnProxyModel::onSourceModelReset);
        connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &TxTableColumnProxyModel::onSourceLayoutAboutToBeChanged);
        connect(model, &QAbstractItemModel::layoutChanged, this, &TxTableColumnProxyModel::onSourceLayoutChanged);
    }
}

void TxTableColumnProxyModel::setColumns(const Settings::TIISettings::TxTableSettings &settings)
{
    // Build column order only from enabled columns
    // Code and Level are always included
    QMap<int, int> orderMap;  // order -> source column
    orderMap.insert(settings.code.order, TxTableModel::ColCode);
    orderMap.insert(settings.level.order, TxTableModel::ColLevel);
    if (settings.location.enabled)
    {
        orderMap.insert(settings.location.order, TxTableModel::ColLocation);
    }
    if (settings.power.enabled)
    {
        orderMap.insert(settings.power.order, TxTableModel::ColPower);
    }
    if (settings.dist.enabled)
    {
        orderMap.insert(settings.dist.order, TxTableModel::ColDist);
    }
    if (settings.azimuth.enabled)
    {
        orderMap.insert(settings.azimuth.order, TxTableModel::ColAzimuth);
    }

    QList<int> colOrder = orderMap.values();
    if (colOrder != m_columnOrder)
    {
        beginResetModel();
        m_columnOrder = colOrder;
        endResetModel();
    }
}

QModelIndex TxTableColumnProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !sourceModel())
    {
        return QModelIndex();
    }

    int sourceCol = sourceColumnForProxyColumn(proxyIndex.column());
    if (sourceCol < 0)
    {
        return QModelIndex();
    }
    return sourceModel()->index(proxyIndex.row(), sourceCol, proxyIndex.parent());
}

QModelIndex TxTableColumnProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
    {
        return QModelIndex();
    }

    int proxyCol = proxyColumnForSourceColumn(sourceIndex.column());
    if (proxyCol < 0)
    {
        return QModelIndex();
    }
    return createIndex(sourceIndex.row(), proxyCol);
}

int TxTableColumnProxyModel::rowCount(const QModelIndex &parent) const
{
    if (!sourceModel())
    {
        return 0;
    }
    return sourceModel()->rowCount(parent);
}

int TxTableColumnProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_columnOrder.size();
}

QModelIndex TxTableColumnProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (row < 0 || column < 0 || column >= m_columnOrder.size())
    {
        return QModelIndex();
    }
    if (!sourceModel() || row >= sourceModel()->rowCount())
    {
        return QModelIndex();
    }
    return createIndex(row, column);
}

QModelIndex TxTableColumnProxyModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();  // flat table, no parent
}

QVariant TxTableColumnProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (!sourceModel())
    {
        return QVariant();
    }
    if (orientation == Qt::Horizontal)
    {
        int sourceCol = sourceColumnForProxyColumn(section);
        if (sourceCol < 0)
        {
            return QVariant();
        }
        return sourceModel()->headerData(sourceCol, orientation, role);
    }
    return sourceModel()->headerData(section, orientation, role);
}

void TxTableColumnProxyModel::sort(int column, Qt::SortOrder order)
{
    // Map proxy column to source column and delegate sorting to the source (TxTableProxyModel)
    int sourceCol = sourceColumnForProxyColumn(column);
    if (sourceCol >= 0 && sourceModel())
    {
        sourceModel()->sort(sourceCol, order);
    }
}

int TxTableColumnProxyModel::sourceColumnForProxyColumn(int proxyColumn) const
{
    if (proxyColumn < 0 || proxyColumn >= m_columnOrder.size())
    {
        return -1;
    }
    return m_columnOrder.at(proxyColumn);
}

int TxTableColumnProxyModel::proxyColumnForSourceColumn(int sourceColumn) const
{
    return m_columnOrder.indexOf(sourceColumn);
}

void TxTableColumnProxyModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles)
{
    if (m_columnOrder.isEmpty())
    {
        return;
    }

    int firstRow = topLeft.row();
    int lastRow = bottomRight.row();
    int lastCol = m_columnOrder.size() - 1;

    emit dataChanged(createIndex(firstRow, 0), createIndex(lastRow, lastCol), roles);
}

void TxTableColumnProxyModel::onSourceRowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    beginInsertRows(QModelIndex(), first, last);
}

void TxTableColumnProxyModel::onSourceRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    endInsertRows();
    emit rowCountChanged();
}

void TxTableColumnProxyModel::onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    beginRemoveRows(QModelIndex(), first, last);
}

void TxTableColumnProxyModel::onSourceRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);
    endRemoveRows();
    emit rowCountChanged();
}

void TxTableColumnProxyModel::onSourceModelAboutToBeReset()
{
    beginResetModel();
}

void TxTableColumnProxyModel::onSourceModelReset()
{
    endResetModel();
    emit rowCountChanged();
}

void TxTableColumnProxyModel::onSourceLayoutAboutToBeChanged()
{
    emit layoutAboutToBeChanged();

    const auto proxyPersistentIndexes = persistentIndexList();
    m_savedProxyIndexes.clear();
    m_savedMappedSourceIndexes.clear();
    for (const auto &proxyIdx : proxyPersistentIndexes)
    {
        m_savedProxyIndexes.append(proxyIdx);
        m_savedMappedSourceIndexes.append(mapToSource(proxyIdx));
    }
}

void TxTableColumnProxyModel::onSourceLayoutChanged()
{
    for (int i = 0; i < m_savedProxyIndexes.size(); ++i)
    {
        changePersistentIndex(m_savedProxyIndexes.at(i), mapFromSource(m_savedMappedSourceIndexes.at(i)));
    }
    m_savedProxyIndexes.clear();
    m_savedMappedSourceIndexes.clear();
    emit layoutChanged();
}
