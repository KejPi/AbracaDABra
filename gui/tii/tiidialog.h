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
#include "radiocontrol.h"
#include "tiitablemodel.h"

namespace Ui {
class TIIDialog;
}

class TIIDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TIIDialog(QWidget *parent = nullptr);
    ~TIIDialog();
    void reset();
    void onTiiData(const RadioControlTIIData & data);
    void setupDarkMode(bool darkModeEna);

signals:
    void setTii(bool ena, float thr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    enum GraphId {Spect, TII, Thr};
    enum GraphRange { MinX = -1024, MaxX = 1023, MinY = 0, MaxY = 1 };
    Ui::TIIDialog *ui;
    TiiTableModel * m_model;
    bool m_isZoomed;
    void addToPlot(const RadioControlTIIData &data);
    void onPlotSelectionChanged();
    void onPlotMousePress(QMouseEvent * event);
    void onPlotMouseWheel(QWheelEvent * event);
    void onContextMenuRequest(QPoint pos);
    void onXRangeChanged(const QCPRange &newRange);
    void onYRangeChanged(const QCPRange &newRange);
    void showPointToolTip(QMouseEvent *event);
};

#endif // TIIDIALOG_H
