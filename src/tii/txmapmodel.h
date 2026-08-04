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

#ifndef TXMAPMODEL_H
#define TXMAPMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QGeoCoordinate>
#include <QHash>
#include <QList>

#include "txtablemodel.h"

class QSortFilterProxyModel;
class TxTableProxyModel;

// Deduplication key: same physical transmitter = same location + same ensemble (channel) + same TII code.
struct MarkerKey
{
    double lat;
    double lon;
    uint64_t ensId;  // ServiceListId::value() — identifies the channel/ensemble
    int tiiId;       // TxTableModelItem::id() = (subId<<8)|mainId
    bool operator==(const MarkerKey &o) const
    {
        return lat == o.lat && lon == o.lon && ensId == o.ensId && tiiId == o.tiiId;
    }
};
inline size_t qHash(const MarkerKey &key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, key.lat, key.lon, key.ensId, key.tiiId);
}

// TxMapModel: a deduplicated list model for map markers.
// Collapses all TxTableModel rows that share the same physical transmitter
// (same location, ensemble/channel and TII code) into a single entry so that
// MapItemView only instantiates one delegate per unique transmitter.
class TxMapModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        CoordinatesRole = Qt::UserRole,
        TiiStringRole,
        LevelColorRole,
        SelectedTxRole,
    };

    // proxy is the row-filtering proxy (local/inactive Tx filters) whose visibility
    // decisions are used to decide whether a source row should contribute a marker.
    explicit TxMapModel(TxTableModel *sourceModel, TxTableProxyModel *proxy, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Returns the highest proxy-model row index (in the given proxy's current
    // sort/filter order) among all source rows belonging to markerIndex.
    // Returns -1 if none of the marker's source rows are visible in the proxy.
    int lastProxyRowForMarker(int markerIndex, QSortFilterProxyModel *proxy) const;

private:
    struct MarkerItem
    {
        QGeoCoordinate coordinate;
        QString tiiCode;
        QColor levelColor;          // color when not selected (last contributing row)
        QColor selectedLevelColor;  // color when selected (from the last selected row)
        bool isSelected = false;
        QList<int> sourceRows;  // TxTableModel row indices that map to this marker
    };

    TxTableModel *m_source;
    TxTableProxyModel *m_proxy;
    QList<MarkerItem> m_markers;
    // Key: (lat, lon, ensId, tiiId) — uniquely identifies one physical transmitter on one channel.
    QHash<MarkerKey, int> m_markerLookup;
    // Key: (lat, lon) — all marker indices sharing the same physical location.
    QHash<QPair<double, double>, QList<int>> m_locationToMarkers;
    QHash<int, int> m_sourceRowToMarker;  // sourceRow -> index into m_markers
    QSet<int> m_selectedMarkerIndices;    // currently selected marker indices

    void rebuild();
    // Process one source row: add a new marker or update an existing one.
    // Pass emitSignals=false when called inside beginResetModel/endResetModel.
    void processSourceRow(int sourceRow, bool emitSignals);
    // Efficiently update marker selection state from the source model's selection set.
    void applySelection(const QSet<int> &selectedSourceRows);

    static MarkerKey markerKey(const TxTableModelItem &item);
    static QColor levelColorFor(const TxTableModelItem &item);
};

#endif  // TXMAPMODEL_H
