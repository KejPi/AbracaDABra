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

#ifndef TXMAPDIALOG_H
#define TXMAPDIALOG_H

#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QQuickView>

#include "radiocontrol.h"
#include "settings.h"
#include "txtablemodel.h"
#include "txtableproxymodel.h"
#include "uicontrolprovider.h"

class TxDataItem;

class TxMapBackend : public UIControlProvider
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TxMapBackend cannot be instantiated")
    Q_PROPERTY(QGeoCoordinate currentPosition READ currentPosition WRITE setCurrentPosition NOTIFY currentPositionChanged FINAL)
    Q_PROPERTY(bool positionValid READ positionValid WRITE setPositionValid NOTIFY positionValidChanged FINAL)
    Q_PROPERTY(QStringList ensembleInfo READ ensembleInfo NOTIFY ensembleInfoChanged FINAL)
    Q_PROPERTY(QStringList txInfo READ txInfo NOTIFY txInfoChanged FINAL)
    Q_PROPERTY(bool isTii READ isTii CONSTANT FINAL)
    Q_PROPERTY(bool isRecordingLog READ isRecordingLog WRITE setIsRecordingLog NOTIFY isRecordingLogChanged FINAL)
    Q_PROPERTY(bool centerToCurrentPosition READ centerToCurrentPosition WRITE setCenterToCurrentPosition NOTIFY centerToCurrentPositionChanged FINAL)
    Q_PROPERTY(float zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged FINAL)
    Q_PROPERTY(QGeoCoordinate mapCenter READ mapCenter WRITE setMapCenter NOTIFY mapCenterChanged FINAL)
    Q_PROPERTY(QAbstractItemModel *tableModel READ tableModel CONSTANT FINAL)
    Q_PROPERTY(QItemSelectionModel *tableSelectionModel READ tableSelectionModel CONSTANT FINAL)
    Q_PROPERTY(bool showSpetrumPlot READ showSpetrumPlot WRITE setShowSpetrumPlot NOTIFY showSpetrumPlotChanged FINAL)

public:
    explicit TxMapBackend(Settings *settings, bool isTii, QObject *parent = nullptr);
    ~TxMapBackend();
    virtual void onTiiData(const RadioControlTIIData &data) = 0;
    virtual void onEnsembleInformation(const RadioControlEnsemble &ens) = 0;
    virtual void onSettingsChanged();

    QGeoCoordinate currentPosition() const;
    void setCurrentPosition(const QGeoCoordinate &newCurrentPosition);

    bool positionValid() const;
    void setPositionValid(bool newPositionValid);

    virtual QStringList ensembleInfo() const;
    virtual QStringList txInfo() const;

    Q_INVOKABLE void selectTx(int index);

    bool showTiiTable() const;
    bool isTii() const;

    Q_INVOKABLE virtual void startStopLog() {}
    bool isRecordingLog() const;
    void setIsRecordingLog(bool newIsRecordingLog);

    void updateTxTable() { m_model->reloadTxTable(); }

    float zoomLevel() const;
    void setZoomLevel(float newZoomLevel);

    QGeoCoordinate mapCenter() const;
    void setMapCenter(const QGeoCoordinate &mapCenter);

    bool centerToCurrentPosition() const;
    void setCenterToCurrentPosition(bool centerToCurrentPosition);

    QAbstractItemModel *tableModel() const { return m_tableModel; }
    QItemSelectionModel *tableSelectionModel() const { return m_tableSelectionModel; }

    bool showSpetrumPlot() const { return m_showSpetrumPlot; }
    void setShowSpetrumPlot(bool showSpetrumPlot);

signals:
    void setTii(bool ena);
    void selectedRowChanged();
    void currentPositionChanged();
    void positionValidChanged();
    void ensembleInfoChanged();
    void txInfoChanged();
    void isRecordingLogChanged();
    void zoomLevelChanged();
    void mapCenterChanged();
    void centerToCurrentPositionChanged();
    void txTableColChanged();
    void showSpetrumPlotChanged();

protected:
    void setIsActive(bool isActive);
    int selectedRow() const;
    void setSelectedRow(int newSelectedRow);

    virtual void onSelectedRowChanged() { /* do nothing by default */ };

    Settings *m_settings;
    TxTableModel *m_model;
    TxTableProxyModel *m_sortedFilteredModel;
    QAbstractItemModel *m_tableModel;  // exposed model (m_sortedFilteredModel or column proxy on top)
    QItemSelectionModel *m_tableSelectionModel;

    // UI
    RadioControlEnsemble m_currentEnsemble;
    QStringList m_txInfo;

    void reset();
    void positionUpdated(const QGeoPositionInfo &position);
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void doLocationUpdate(bool en);
    QModelIndex mapToSourceModel(const QModelIndex &proxyIndex) const;
    void setupSelectionModel();

private:
    QGeoPositionInfoSource *m_geopositionSource = nullptr;
    QGeoCoordinate m_currentPosition;
    bool m_positionValid = false;
    const bool m_isTii;
    bool m_isRecordingLog = false;
    int m_selectedRow = -1;  // source model row
    bool m_showInactive;
    float m_zoomLevel = 9.0;
    QGeoCoordinate m_mapCenter;
    bool m_centerToCurrentPosition = true;
    bool m_showSpetrumPlot = true;
    bool m_isActive = false;

    static int s_locUpdateCounter;
};

#endif  // TXMAPDIALOG_H
