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

#ifndef TXTABLEMODEL_H
#define TXTABLEMODEL_H

#include <QAbstractTableModel>
#include <QGeoPositionInfo>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QtQml>

#include "dabsdr.h"
#include "servicelistid.h"
#include "txtablemodelitem.h"

class TxDataItem;
class TxLocalList;

class TxTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
public:
    enum TxTableModelRoles
    {
        ExportRole = Qt::UserRole,
        ExportRoleUTC,         // this role is the same as export role but time is in UTC
        ExportRoleEnglish,     // this role is the same as export role but untranslated
        ExportRoleUTCEnglish,  // this role is the same as export role UTC but untranslated
        CoordinatesRole,
        TiiRole,
        MainIdRole,
        SubIdRole,
        LevelColorRole,
        ItemRole,
        IdRole,
        SelectedTxRole,
        IsActiveRole,
        IsLocalRole,
        IconSourceRole,
    };

    enum TxTableCols
    {
        ColTime,
        ColChannel,
        ColFreq,
        ColEnsId,
        ColEnsLabel,
        ColNumServices,
        ColSnr,
        ColRfLevel,
        ColMainId,
        ColSubId,
        ColLevel,
        ColLocation,
        ColPower,
        ColDist,
        ColAzimuth,           // keep order of these
        ColTxCoordinatesLat,  // this is used as first column for no coordinates case (do not add items below)
        ColTxCoordinatesLon,
        ColTxAltidude,
        ColTxAntennaHeight,
        ColRxCoordinatesLat,
        ColRxCoordinatesLon,
        ColRxAltitude,
        ColCode,  // this is only used to display in TII table, skipped for scanner and export
        NumCols,
        LastColumn = ColRxAltitude,
        LastColumnWithoutCoordinates = ColAzimuth,
        NumColsWithoutCoordinates = LastColumnWithoutCoordinates + 1,

        LastColumnV1Coords = 18,  // frozen: CSV format with only Lat/Lon (no altitude), no RF Level = 18 cols
                                  // LastColumnV1Coords + 1 = 19 cols (same format, with RF Level)
    };
    Q_ENUM(TxTableCols)

    explicit TxTableModel(QObject *parent = nullptr);
    ~TxTableModel();
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int activeCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    const TxTableModelItem &itemAt(int row) const;
    void clear();
    void reloadTxTable();
    void setSelectedRows(const QSet<int> &rows);
    void removeInactive(qint64 timeoutSec);

    void updateTiiData(const QList<dabsdrTii_t> &data, const ServiceListId &ensId, const QString &ensLabel, int numServices, float snr);
    void appendEnsData(const QDateTime &time, const QList<dabsdrTii_t> &data, const ServiceListId &ensId, const QString &ensLabel,
                       const QString &ensConfig, const QString &ensCSV, int numServices, float snr, float rfLevel);
    void setCoordinates(const QGeoCoordinate &newCoordinates);
    void setDisplayTimeInUTC(bool newDisplayTimeInUTC);
    void countryFlagUpdated(const ServiceListId &ensId);

    // local TX management
    void loadLocalTxList(const QString &filename);
    void setAsLocalTx(const QModelIndex &index, bool setAsLocal);
    void clearLocalTx();

    // loading from file
    void beginLoadingFromFile();
    void endLoadingFromFile();
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject &json, bool utcTime);
signals:
    void rowCountChanged();
    void selectedRowsChanged(const QSet<int> &rows);

private:
    bool m_displayTimeInUTC = false;
    bool m_loadingFromFile = false;
    QList<TxTableModelItem> m_modelData;
    QSet<int> m_selectedRows;
    QMultiHash<ServiceListId, TxDataItem *> m_txList;
    QGeoCoordinate m_coordinates;
    TxLocalList *m_localTxList = nullptr;
    int m_flagRefreshCounter = 0;  // cache-busting token appended to flag icon URL so QML detects the value change
};

#endif  // TXTABLEMODEL_H
