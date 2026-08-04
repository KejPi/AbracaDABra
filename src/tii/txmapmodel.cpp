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

#include "txmapmodel.h"

#include <QSortFilterProxyModel>

#include "txtableproxymodel.h"

TxMapModel::TxMapModel(TxTableModel *sourceModel, TxTableProxyModel *proxy, QObject *parent)
    : QAbstractListModel(parent), m_source(sourceModel), m_proxy(proxy)
{
    // Full rebuild whenever the source model is reset (e.g. CSV load).
    connect(m_source, &QAbstractItemModel::modelReset, this, &TxMapModel::rebuild);

    // Incremental update for appended rows (normal scan operation).
    connect(m_source, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int first, int last)
            {
                for (int row = first; row <= last; ++row)
                {
                    processSourceRow(row, true);
                }
            });

    // Rows removed: simplest safe strategy is a full rebuild.
    // In scanner mode rows are never individually removed (only via modelReset),
    // but handle it defensively.
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, &TxMapModel::rebuild);

    // Selection changes: use the dedicated signal for O(1) hash lookups.
    // TxTableModel::setSelectedRows emits selectedRowsChanged with the complete new set.
    connect(m_source, &TxTableModel::selectedRowsChanged, this, &TxMapModel::applySelection);

    // Row-filter criteria (local Tx / inactive Tx) changed on the proxy — a full rebuild is
    // required since markers may need to be added or removed independently of source row inserts.
    connect(m_proxy, &TxTableProxyModel::rowsFilterChanged, this, &TxMapModel::rebuild);

    connect(m_source, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles)
            {
                if (roles.contains(TxTableModel::TxTableModelRoles::IsLocalRole))
                {
                    rebuild();
                }
            });

    rebuild();
}

int TxMapModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_markers.count();
}

QVariant TxMapModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_markers.count())
    {
        return QVariant();
    }
    const MarkerItem &marker = m_markers.at(index.row());
    switch (role)
    {
        case CoordinatesRole:
            return QVariant::fromValue(marker.coordinate);
        case TiiStringRole:
            return marker.tiiCode;
        case LevelColorRole:
            // When a marker is selected, show the level-color of the last selected row.
            if (marker.isSelected && marker.selectedLevelColor.isValid())
            {
                return QVariant::fromValue(marker.selectedLevelColor);
            }
            return QVariant::fromValue(marker.levelColor);
        case SelectedTxRole:
            return marker.isSelected;
        default:
            break;
    }
    return QVariant();
}

QHash<int, QByteArray> TxMapModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CoordinatesRole] = "coordinates";
    roles[TiiStringRole] = "tiiString";
    roles[LevelColorRole] = "levelColor";
    roles[SelectedTxRole] = "selectedTx";
    return roles;
}

int TxMapModel::lastProxyRowForMarker(int markerIndex, QSortFilterProxyModel *proxy) const
{
    if (markerIndex < 0 || markerIndex >= m_markers.count() || proxy == nullptr)
    {
        return -1;
    }
    // Search across ALL markers at the same physical location so that clicking any
    // overlapping marker (different channel / TII code, same coordinates) selects
    // the most-recently logged row for that transmitter site.
    const QGeoCoordinate &coord = m_markers.at(markerIndex).coordinate;
    const auto locKey = qMakePair(coord.latitude(), coord.longitude());
    const QList<int> coLocated = m_locationToMarkers.value(locKey);

    int lastRow = -1;
    for (int idx : coLocated)
    {
        for (int srcRow : m_markers.at(idx).sourceRows)
        {
            const QModelIndex proxyIdx = proxy->mapFromSource(m_source->index(srcRow, 0));
            if (proxyIdx.isValid())
            {
                lastRow = qMax(lastRow, proxyIdx.row());
            }
        }
    }
    return lastRow;
}

void TxMapModel::rebuild()
{
    beginResetModel();
    m_markers.clear();
    m_markerLookup.clear();
    m_locationToMarkers.clear();
    m_sourceRowToMarker.clear();
    m_selectedMarkerIndices.clear();
    for (int row = 0; row < m_source->rowCount(); ++row)
    {
        processSourceRow(row, false);
    }
    endResetModel();
}

void TxMapModel::processSourceRow(int sourceRow, bool emitSignals)
{
    const TxTableModelItem &item = m_source->itemAt(sourceRow);

    // Skip rows with no transmitter DB match or no TII identification.
    if (!item.hasTxData() || item.mainId() == -1)
    {
        return;
    }
    // Skip rows currently hidden by the table's row filter (e.g. hide local Tx / hide inactive Tx),
    // so map markers stay in sync with what is shown in the table.
    if (!m_proxy->isSourceRowVisible(sourceRow))
    {
        return;
    }
    const QGeoCoordinate coord = item.transmitterData().coordinates();
    if (!coord.isValid())
    {
        return;
    }

    const QString tiiCode = QString("%1-%2").arg(item.mainId()).arg(item.subId());
    const QColor color = levelColorFor(item);
    const bool selected = m_source->data(m_source->index(sourceRow, 0), TxTableModel::TxTableModelRoles::SelectedTxRole).toBool();

    const auto key = markerKey(item);
    const auto it = m_markerLookup.constFind(key);
    if (it == m_markerLookup.constEnd())
    {
        // New unique transmitter (location + channel + TII code) — insert a new marker.
        MarkerItem marker;
        marker.coordinate = coord;
        marker.tiiCode = tiiCode;
        marker.levelColor = color;
        marker.isSelected = selected;
        if (selected)
        {
            marker.selectedLevelColor = color;
        }
        marker.sourceRows.append(sourceRow);

        const int newIdx = m_markers.count();
        if (emitSignals)
        {
            beginInsertRows(QModelIndex(), newIdx, newIdx);
        }
        m_markers.append(marker);
        m_markerLookup[key] = newIdx;
        m_locationToMarkers[{coord.latitude(), coord.longitude()}].append(newIdx);
        m_sourceRowToMarker[sourceRow] = newIdx;
        if (selected)
        {
            m_selectedMarkerIndices.insert(newIdx);
        }
        if (emitSignals)
        {
            endInsertRows();
        }
    }
    else
    {
        // Existing transmitter — add this source row to the marker.
        const int markerIdx = it.value();
        m_sourceRowToMarker[sourceRow] = markerIdx;
        MarkerItem &marker = m_markers[markerIdx];
        marker.sourceRows.append(sourceRow);

        // Update color and selection (later row takes precedence).
        bool changed = false;
        if (marker.levelColor != color)
        {
            marker.levelColor = color;
            changed = true;
        }
        if (!marker.isSelected && selected)
        {
            marker.isSelected = true;
            marker.selectedLevelColor = color;
            m_selectedMarkerIndices.insert(markerIdx);
            changed = true;
        }
        if (changed && emitSignals)
        {
            const QModelIndex mi = index(markerIdx);
            emit dataChanged(mi, mi, {LevelColorRole, SelectedTxRole});
        }
    }
}

void TxMapModel::applySelection(const QSet<int> &selectedSourceRows)
{
    // For each marker, find the last (highest index) selected source row.
    // That row's level-color will be shown while the marker is selected.
    QHash<int, int> markerToLastSelectedRow;  // markerIdx -> last selected srcRow
    for (int srcRow : selectedSourceRows)
    {
        const auto it = m_sourceRowToMarker.constFind(srcRow);
        if (it != m_sourceRowToMarker.constEnd())
        {
            const int markerIdx = it.value();
            auto &cur = markerToLastSelectedRow[markerIdx];
            if (cur < srcRow)
            {
                cur = srcRow;
            }
        }
        // Rows without TX data are not in m_sourceRowToMarker — they produce no marker.
    }

    const QSet<int> newSelectedMarkers(markerToLastSelectedRow.keyBegin(), markerToLastSelectedRow.keyEnd());

    // Deselect markers that are no longer in the new selection.
    for (int markerIdx : std::as_const(m_selectedMarkerIndices))
    {
        if (!newSelectedMarkers.contains(markerIdx))
        {
            m_markers[markerIdx].isSelected = false;
            m_markers[markerIdx].selectedLevelColor = QColor();
            const QModelIndex mi = index(markerIdx);
            emit dataChanged(mi, mi, {SelectedTxRole, LevelColorRole});
        }
    }

    // Select or update markers in the new selection.
    for (auto it = markerToLastSelectedRow.cbegin(); it != markerToLastSelectedRow.cend(); ++it)
    {
        const int markerIdx = it.key();
        const QColor newSelectedColor = levelColorFor(m_source->itemAt(it.value()));
        MarkerItem &marker = m_markers[markerIdx];
        const bool wasSelected = m_selectedMarkerIndices.contains(markerIdx);
        if (!wasSelected || marker.selectedLevelColor != newSelectedColor)
        {
            marker.isSelected = true;
            marker.selectedLevelColor = newSelectedColor;
            const QModelIndex mi = index(markerIdx);
            emit dataChanged(mi, mi, {SelectedTxRole, LevelColorRole});
        }
    }

    m_selectedMarkerIndices = newSelectedMarkers;
}

MarkerKey TxMapModel::markerKey(const TxTableModelItem &item)
{
    const QGeoCoordinate c = item.transmitterData().coordinates();
    return {c.latitude(), c.longitude(), item.ensId().value(), item.id()};
}

QColor TxMapModel::levelColorFor(const TxTableModelItem &item)
{
    if (!item.isActive())
    {
        return QColor(Qt::gray);
    }
    if (item.level() > -6)
    {
        return QColor(Qt::green);
    }
    if (item.level() > -12)
    {
        return QColor(0xff, 0xb5, 0x27);
    }
    return QColor(0xff, 0x4b, 0x4b);
}
