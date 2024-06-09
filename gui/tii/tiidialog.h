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

#ifndef TIIDIALOG_H
#define TIIDIALOG_H

#include <QDialog>
#include "QQuickView"
#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include "settings.h"
#include "radiocontrol.h"
#include "tiitablemodel.h"
#include "config.h"
#if HAVE_QCUSTOMPLOT
#include <qcustomplot.h>
#endif

class TxDataItem;

class TIIDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QGeoCoordinate currentPosition READ currentPosition WRITE setCurrentPosition NOTIFY currentPositionChanged FINAL)
    Q_PROPERTY(bool positionValid READ positionValid WRITE setPositionValid NOTIFY positionValidChanged FINAL)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged FINAL)
    Q_PROPERTY(QStringList ensembleInfo READ ensembleInfo NOTIFY ensembleInfoChanged FINAL)
    Q_PROPERTY(QStringList txInfo READ txInfo NOTIFY txInfoChanged FINAL)
    Q_PROPERTY(int selectedRow READ selectedRow WRITE setSelectedRow NOTIFY selectedRowChanged FINAL)

public:
    explicit TIIDialog(const Settings * settings, QWidget *parent = nullptr);
    ~TIIDialog();
    void onTiiData(const RadioControlTIIData & data);
    void setupDarkMode(bool darkModeEna);
    void startLocationUpdate();
    void stopLocationUpdate();
    void onChannelSelection();
    void onEnsembleInformation(const RadioControlEnsemble &ens);
    void onSettingsChanged();

    QGeoCoordinate currentPosition() const;
    void setCurrentPosition(const QGeoCoordinate &newCurrentPosition);

    bool positionValid() const;
    void setPositionValid(bool newPositionValid);

    bool isVisible() const;
    void setIsVisible(bool newIsVisible);

    QStringList ensembleInfo() const;
    QStringList txInfo() const;

    int selectedRow() const;
    void setSelectedRow(int modelRow);

signals:
    void setTii(bool ena, float thr);
    void currentPositionChanged();
    void positionValidChanged();
    void isVisibleChanged();
    void ensembleInfoChanged();
    void txInfoChanged();
    void selectedRowChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    enum GraphId {Spect, TII};
    enum GraphRange { MinX = 0, MaxX = 191, MinY = 0, MaxY = 1 };
    const Settings * m_settings;

    // UI
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QCustomPlot *m_tiiSpectrumPlot;
    bool m_isZoomed;
#endif
    QQuickView *m_qmlView ;

    TiiTableModel * m_model;
    TiiTableSortModel * m_sortModel;
    QGeoPositionInfoSource * m_geopositionSource = nullptr;
    QGeoCoordinate m_currentPosition;
    bool m_positionValid = false;
    bool m_isVisible = false;
    RadioControlEnsemble m_currentEnsemble;
    QStringList m_txInfo;
    QItemSelectionModel * m_tiiTableSelectionModel;
    int m_selectedRow;  // this is row in source model !!!

    void reset();

    void positionUpdated(const QGeoPositionInfo & position);
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onSelectedRowChanged();
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    void showPointToolTip(QMouseEvent *event);
    void onXRangeChanged(const QCPRange &newRange);
    void onYRangeChanged(const QCPRange &newRange);
    void addToPlot(const RadioControlTIIData &data);
    void updateTiiPlot();
    void onPlotSelectionChanged();
    void onPlotMousePress(QMouseEvent * event);
    void onPlotMouseWheel(QWheelEvent * event);
    void onContextMenuRequest(QPoint pos);
#endif
};

#endif // TIIDIALOG_H
