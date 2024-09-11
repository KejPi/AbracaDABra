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

#ifndef SNRPLOTDIALOG_H
#define SNRPLOTDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QTimer>
#include "settings.h"

namespace Ui {
class SNRPlotDialog;
}

class SNRPlotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SNRPlotDialog(Settings *settings, QWidget *parent = nullptr);
    ~SNRPlotDialog();
    void setSignalState(uint8_t sync, float snr);
    void setupDarkMode(bool darkModeEna);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum { xPlotRange = 2*60 };
    static const char * syncLevelLabels[];
    static const QStringList snrProgressStylesheet;
    Ui::SNRPlotDialog *ui;
    Settings * m_settings = nullptr;
    QTime m_startTime;
    QTimer * m_timer = nullptr;

    void addToPlot(float snr);
};

#endif // SNRPLOTDIALOG_H
