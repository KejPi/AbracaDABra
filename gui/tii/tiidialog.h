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

#ifndef TIIDIALOG_H
#define TIIDIALOG_H

#include <QDialog>
#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QQuickView>

#include "config.h"
#include "radiocontrol.h"
#include "settings.h"
#include "txmapdialog.h"
#if HAVE_QCUSTOMPLOT
#include <qcustomplot.h>
#endif

class TxDataItem;

class TIIDialog : public TxMapDialog
{
    Q_OBJECT
public:
    explicit TIIDialog(Settings *settings, QWidget *parent = nullptr);
    ~TIIDialog();
    void onTiiData(const RadioControlTIIData &data) override;
    void onSignalState(uint8_t, float snr) { m_snr = snr; };
    void setupDarkMode(bool darkModeEna) override;
    void onChannelSelection();
    void onEnsembleInformation(const RadioControlEnsemble &ens) override;
    void onSettingsChanged() override;

    Q_INVOKABLE void startStopLog() override;

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void onSelectedRowChanged() override;

private:
    enum GraphId
    {
        Spect,
        TII
    };
    enum GraphRange
    {
        MinX = 0,
        MaxX = 191,
        MinY = 0,
        MaxY = 1
    };

    // UI
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QCustomPlot *m_tiiSpectrumPlot;
    bool m_isZoomed;
    QSplitter *m_splitter;
#endif
    QQuickView *m_qmlView;
    QFile *m_logFile = nullptr;
    float m_snr = 0.0;

    void reset() override;
    void logTiiData() const;

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    void showPointToolTip(QMouseEvent *event);
    void onXRangeChanged(const QCPRange &newRange);
    void onYRangeChanged(const QCPRange &newRange);
    void addToPlot(const RadioControlTIIData &data);
    void updateTiiPlot();
    void onPlotSelectionChanged();
    void onPlotMousePress(QMouseEvent *event);
    void onPlotMouseWheel(QWheelEvent *event);
    void onContextMenuRequest(QPoint pos);
#endif
};

#endif  // TIIDIALOG_H
