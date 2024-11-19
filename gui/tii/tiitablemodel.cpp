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

#include "tiitablemodel.h"
#include "txtablemodel.h"

TiiTableModel::TiiTableModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::rowsInserted, this, &TiiTableModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, &TiiTableModel::rowCountChanged);
    connect(this, &QSortFilterProxyModel::modelReset, this, &TiiTableModel::rowCountChanged);

}

bool TiiTableModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
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

bool TiiTableModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);

    switch (source_column) {
    case TxTableModel::ColMainId:
    case TxTableModel::ColSubId:
    case TxTableModel::ColLevel:
    case TxTableModel::ColDist:
    case TxTableModel::ColAzimuth:
        return true;
    }
    return false;
}
