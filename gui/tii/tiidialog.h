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
#include <qcustomplot.h>
#include "QQuickView"
#include <QGeoPositionInfoSource>
#include "radiocontrol.h"
#include "servicelistid.h"
#include "tiitablemodel.h"

namespace Ui {
class TIIDialog;
}

class TxDataItem;

class TIIDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QGeoCoordinate currentPosition READ currentPosition WRITE setCurrentPosition NOTIFY currentPositionChanged FINAL)
    Q_PROPERTY(bool positionValid READ positionValid WRITE setPositionValid NOTIFY positionValidChanged FINAL)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged FINAL)
    Q_PROPERTY(QStringList ensembleInfo READ ensembleInfo NOTIFY ensembleInfoChanged FINAL)

public:
    explicit TIIDialog(QWidget *parent = nullptr);
    ~TIIDialog();
    void reset();
    void onTiiData(const RadioControlTIIData & data);
    void setupDarkMode(bool darkModeEna);
    void startLocationUpdate();
    void onEnsembleInformation(const RadioControlEnsemble &ens);

    QGeoCoordinate currentPosition() const;
    void setCurrentPosition(const QGeoCoordinate &newCurrentPosition);

    bool positionValid() const;
    void setPositionValid(bool newPositionValid);

    bool isVisible() const;
    void setIsVisible(bool newIsVisible);

    QStringList ensembleInfo() const;

signals:
    void setTii(bool ena, float thr);

    void currentPositionChanged();
    void positionValidChanged();

    void isVisibleChanged();

    void ensembleInfoChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    enum GraphId {Spect, TII, Thr};
    enum GraphRange { MinX = -1024, MaxX = 1023, MinY = 0, MaxY = 1 };
    Ui::TIIDialog *ui;
    QQuickView *m_qmlView ;
    TiiTableModel * m_model;
    bool m_isZoomed;
    QGeoCoordinate m_currentPosition;
    bool m_positionValid = false;
    bool m_isVisible = false;
    //QList<TxDataItem *> m_txList;
    //QMultiHash<ServiceListId, TxDataItem*> m_txList;
    RadioControlEnsemble m_currentEnsemble;

    void addToPlot(const RadioControlTIIData &data);
    void onPlotSelectionChanged();
    void onPlotMousePress(QMouseEvent * event);
    void onPlotMouseWheel(QWheelEvent * event);
    void onContextMenuRequest(QPoint pos);
    void onXRangeChanged(const QCPRange &newRange);
    void onYRangeChanged(const QCPRange &newRange);
    void showPointToolTip(QMouseEvent *event);
    void positionUpdated(const QGeoPositionInfo & position);
    //void loadTiiTable();
};

#endif // TIIDIALOG_H
