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

#ifndef TXMAPDIALOG_H
#define TXMAPDIALOG_H

#include <QDialog>
#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QQuickView>

#include "radiocontrol.h"
#include "settings.h"
#include "txtablemodel.h"
#include "txtableproxymodel.h"

class TxDataItem;

class TxMapDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QGeoCoordinate currentPosition READ currentPosition WRITE setCurrentPosition NOTIFY currentPositionChanged FINAL)
    Q_PROPERTY(bool positionValid READ positionValid WRITE setPositionValid NOTIFY positionValidChanged FINAL)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged FINAL)
    Q_PROPERTY(QStringList ensembleInfo READ ensembleInfo NOTIFY ensembleInfoChanged FINAL)
    Q_PROPERTY(QStringList txInfo READ txInfo NOTIFY txInfoChanged FINAL)
    Q_PROPERTY(bool isTii READ isTii CONSTANT FINAL)
    Q_PROPERTY(bool isRecordingLog READ isRecordingLog WRITE setIsRecordingLog NOTIFY isRecordingLogChanged FINAL)

public:
    explicit TxMapDialog(Settings *settings, bool isTii, QWidget *parent = nullptr);
    ~TxMapDialog();
    virtual void onTiiData(const RadioControlTIIData &data) = 0;
    virtual void setupDarkMode(bool darkModeEna) = 0;
    void startLocationUpdate();
    void stopLocationUpdate();
    virtual void onEnsembleInformation(const RadioControlEnsemble &ens) = 0;
    virtual void onSettingsChanged();

    QGeoCoordinate currentPosition() const;
    void setCurrentPosition(const QGeoCoordinate &newCurrentPosition);

    bool positionValid() const;
    void setPositionValid(bool newPositionValid);

    bool isVisible() const;
    void setIsVisible(bool newIsVisible);

    virtual QStringList ensembleInfo() const;
    virtual QStringList txInfo() const;

    Q_INVOKABLE void selectTx(int index);

    bool showTiiTable() const;
    bool isTii() const;

    Q_INVOKABLE virtual void startStopLog() {}
    bool isRecordingLog() const;
    void setIsRecordingLog(bool newIsRecordingLog);

    void updateTxTable() { m_model->reloadTxTable(); }

signals:
    void setTii(bool ena);
    void selectedRowChanged();
    void currentPositionChanged();
    void positionValidChanged();
    void isVisibleChanged();
    void ensembleInfoChanged();
    void txInfoChanged();
    void isRecordingLogChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    int selectedRow() const;
    void setSelectedRow(int newSelectedRow);

    virtual void onSelectedRowChanged() { /* do nothing by default */ };

    Settings *m_settings;
    TxTableModel *m_model;
    TxTableProxyModel *m_sortedFilteredModel;
    QItemSelectionModel *m_tableSelectionModel;

    // UI
    RadioControlEnsemble m_currentEnsemble;
    QStringList m_txInfo;

    virtual void reset();

    void positionUpdated(const QGeoPositionInfo &position);
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
    QGeoPositionInfoSource *m_geopositionSource = nullptr;
    QGeoCoordinate m_currentPosition;
    bool m_positionValid = false;
    bool m_isVisible = false;
    const bool m_isTii;
    bool m_isRecordingLog = false;
    int m_selectedRow = -1;  // source model row
    bool m_showInactive;
};

#endif  // TXMAPDIALOG_H
