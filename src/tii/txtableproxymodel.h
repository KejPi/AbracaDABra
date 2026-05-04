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

#ifndef TXTABLEPROXYMODEL_H
#define TXTABLEPROXYMODEL_H

#include <QAbstractProxyModel>
#include <QAbstractTableModel>
#include <QGeoPositionInfo>
#include <QObject>
#include <QSortFilterProxyModel>

#include "settings.h"

// Row filtering and sorting proxy model (used directly for scanner, and as source for TxTableColumnProxyModel for TII)
class TxTableProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
    Q_PROPERTY(bool rfLevelFilter READ rfLevelFilter WRITE setRfLevelFilter NOTIFY rfLevelFilterChanged FINAL)

public:
    TxTableProxyModel(QObject *parent = nullptr);
    void setColumnsFilter(bool filterCols);
    void setInactiveTxFilter(bool filterInactiveTx);
    void setLocalTxFilter(bool filterLocalTx);
    void setRfLevelFilter(bool filterRfLevel);
    bool rfLevelFilter() const { return m_filterRfLevel; }

signals:
    void rowCountChanged();

    void rfLevelFilterChanged();

protected:
    bool m_filterCols = true;
    bool m_filterInactiveTx = true;
    bool m_filterLocalTx = false;
    bool m_filterRfLevel = false;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

// Column filtering+reordering proxy model (sits on top of TxTableProxyModel for TII table)
class TxTableColumnProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
public:
    explicit TxTableColumnProxyModel(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *sourceModel) override;
    void setColumns(const Settings::TIISettings::TxTableSettings &settings);

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    int sourceColumnForProxyColumn(int proxyColumn) const;
    int proxyColumnForSourceColumn(int sourceColumn) const;

signals:
    void rowCountChanged();

private:
    QList<int> m_columnOrder;

    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles);
    void onSourceRowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void onSourceRowsInserted(const QModelIndex &parent, int first, int last);
    void onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onSourceRowsRemoved(const QModelIndex &parent, int first, int last);
    void onSourceModelAboutToBeReset();
    void onSourceModelReset();
    void onSourceLayoutAboutToBeChanged();
    void onSourceLayoutChanged();

    QList<QPersistentModelIndex> m_savedProxyIndexes;
    QList<QPersistentModelIndex> m_savedMappedSourceIndexes;
};

#endif  // TXTABLEPROXYMODEL_H
