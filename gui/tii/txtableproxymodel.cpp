/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

bool TxTableProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    TxTableModelItem itemL = sourceModel()->data(left, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
    TxTableModelItem itemR = sourceModel()->data(right, TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();

    switch (left.column())
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
    {
        return true;
    }

    switch (source_column)
    {
        case TxTableModel::ColMainId:
        case TxTableModel::ColSubId:
        case TxTableModel::ColLevel:
        case TxTableModel::ColDist:
        case TxTableModel::ColAzimuth:
            return true;
    }
    return false;
}

bool TxTableProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = sourceModel()->index(source_row, 0);
    return (m_filterInactiveTx == false) || sourceModel()->data(idx, TxTableModel::TxTableModelRoles::IsActiveRole).toBool();
}
