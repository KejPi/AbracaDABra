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

#include "snrplotdialog.h"
#include "ui_snrplotdialog.h"

const char * SNRPlotDialog::syncLevelLabels[] = {QT_TR_NOOP("No signal"), QT_TR_NOOP("Signal found"), QT_TR_NOOP("Sync")};
const QStringList SNRPlotDialog::snrProgressStylesheet = {
    QString::fromUtf8("QProgressBar { border: 1px solid grey;} QProgressBar::chunk {background-color: #ff4b4b; }"),  // red
    QString::fromUtf8("QProgressBar { border: 1px solid grey;} QProgressBar::chunk {background-color: #ffb527; }"),  // yellow
    QString::fromUtf8("QProgressBar { border: 1px solid grey;} QProgressBar::chunk {background-color: #5bc214; }")   // green
};


SNRPlotDialog::SNRPlotDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SNRPlotDialog)
    , m_settings(settings)
{
    ui->setupUi(this);

    QSize sz = QSize(780, 300);
    if (!m_settings->snr.geometry.isEmpty())
    {
        restoreGeometry(m_settings->snr.geometry);
        sz = size();
    }
    QTimer::singleShot(10, this, [this, sz](){ resize(sz); } );

    QFont boldBigFont;
    boldBigFont.setBold(true);
    boldBigFont.setPointSize(20);
    ui->syncLabel->setFont(boldBigFont);
    int syncLabelWidth = ui->syncLabel->fontMetrics().boundingRect(tr(syncLevelLabels[1])).width()+10;
    ui->syncLabel->setFixedWidth(syncLabelWidth);
    ui->snrValue->setFont(boldBigFont);
    int snrValueWidth = ui->snrValue->fontMetrics().boundingRect(" 36.0 dB").width();
    ui->snrValue->setFixedWidth(snrValueWidth);
    ui->snrValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);    
    ui->snrValue->setToolTip(QString(tr("DAB signal SNR")));
    ui->snrValue->setText("");
    ui->fixedSpacer->setGeometry(QRect(0,0, syncLabelWidth-snrValueWidth, 5));
    ui->progressBar->setFixedHeight(ui->snrValue->fontMetrics().boundingRect(" 36.0 dB").height()/2);

    ui->snrPlot->addGraph();
    ui->snrPlot->graph(0)->setLineStyle(QCPGraph::lsStepCenter);
    // ui->snrPlot->addGraph();
    // ui->snrPlot->graph(1)->setPen(QPen(QColor(255, 110, 40), 2));

    ui->snrPlot->xAxis->grid()->setSubGridVisible(true);
    ui->snrPlot->yAxis->grid()->setSubGridVisible(true);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    ui->snrPlot->xAxis->setTicker(timeTicker);
    ui->snrPlot->axisRect()->setupFullAxesBox();
    ui->snrPlot->xAxis->setRange(0, xPlotRange);
    ui->snrPlot->yAxis->setRange(0, 36);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->snrPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->snrPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->snrPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->snrPlot->yAxis2, SLOT(setRange(QCPRange)));

    m_timer = new QTimer;
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, [this]() { setSignalState(0, 0.0); } );
    setSignalState(0, 0.0);
}

SNRPlotDialog::~SNRPlotDialog()
{
    m_timer->stop();
    delete m_timer;

    delete ui;
}

void SNRPlotDialog::setSignalState(uint8_t sync, float snr)
{
    ui->syncLabel->setText(tr(syncLevelLabels[sync]));
    ui->snrValue->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
    ui->progressBar->setValue(snr);
    if (snr < 7.0)
    {
        ui->progressBar->setStyleSheet(snrProgressStylesheet[0]);
    }
    else if (snr < 10.0)
    {
        ui->progressBar->setStyleSheet(snrProgressStylesheet[1]);
    }
    else
    {
        ui->progressBar->setStyleSheet(snrProgressStylesheet[2]);
    }

    addToPlot(snr);

    m_timer->start();
}

void SNRPlotDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
        ui->snrPlot->xAxis->setBasePen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis->setBasePen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis2->setBasePen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis2->setBasePen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis->setTickPen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis->setTickPen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis2->setTickPen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis2->setTickPen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis->setSubTickPen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis->setSubTickPen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis2->setSubTickPen(QPen(Qt::white, 0));
        ui->snrPlot->yAxis2->setSubTickPen(QPen(Qt::white, 0));
        ui->snrPlot->xAxis->setTickLabelColor(Qt::white);
        ui->snrPlot->yAxis->setTickLabelColor(Qt::white);
        ui->snrPlot->xAxis2->setTickLabelColor(Qt::white);
        ui->snrPlot->yAxis2->setTickLabelColor(Qt::white);
        ui->snrPlot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        ui->snrPlot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine));
        ui->snrPlot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        ui->snrPlot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        ui->snrPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->snrPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->snrPlot->setBackground(QBrush(Qt::black));

        ui->snrPlot->graph(0)->setPen(QPen(Qt::cyan, 2));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 255, 255, 100)));
        ui->snrPlot->replot();
    }
    else
    {
        ui->snrPlot->xAxis->setBasePen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis->setBasePen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis2->setBasePen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis2->setBasePen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis->setTickPen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis->setTickPen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis2->setTickPen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis2->setTickPen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis->setSubTickPen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis->setSubTickPen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis2->setSubTickPen(QPen(Qt::black, 0));
        ui->snrPlot->yAxis2->setSubTickPen(QPen(Qt::black, 0));
        ui->snrPlot->xAxis->setTickLabelColor(Qt::black);
        ui->snrPlot->yAxis->setTickLabelColor(Qt::black);
        ui->snrPlot->xAxis2->setTickLabelColor(Qt::black);
        ui->snrPlot->yAxis2->setTickLabelColor(Qt::black);
        ui->snrPlot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        ui->snrPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
        ui->snrPlot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        ui->snrPlot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        ui->snrPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->snrPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->snrPlot->setBackground(QBrush(Qt::white));

        ui->snrPlot->graph(0)->setPen(QPen(Qt::blue, 2));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 0, 255, 100)));
        ui->snrPlot->replot();
    }
}

void SNRPlotDialog::closeEvent(QCloseEvent *event)
{
    m_settings->snr.geometry = saveGeometry();

    QDialog::closeEvent(event);
}

void SNRPlotDialog::addToPlot(float snr)
{
    double key = 0.0;
    if (m_startTime.isNull())
    {
        m_startTime = QTime::currentTime();
    }
    else
    {
        key = m_startTime.msecsTo(QTime::currentTime()) / 1000.0;
    }

    ui->snrPlot->graph(0)->addData(key, snr);
#if 0
    double avrg = 0.0;
    #define AGVR_SAMPLES 8
    if (ui->snrPlot->graph(0)->data()->size() < AGVR_SAMPLES)
    {
        for (int n = 0; n < ui->snrPlot->graph(0)->data()->size(); ++n)
        {
            avrg += ui->snrPlot->graph(0)->data()->at(n)->value;
        }
        avrg = avrg / ui->snrPlot->graph(0)->data()->size();
    }
    else {
        for (int n = ui->snrPlot->graph(0)->data()->size()-AGVR_SAMPLES; n < ui->snrPlot->graph(0)->data()->size(); ++n)
        {
            avrg += ui->snrPlot->graph(0)->data()->at(n)->value;
        }
        avrg = avrg / AGVR_SAMPLES;
    }
    ui->snrPlot->graph(1)->addData(key, avrg);
#endif
    // make key axis range scroll with the data (at a constant range size of 8):
    if (key < xPlotRange)
    {
        ui->snrPlot->xAxis->setRange(0, xPlotRange);
    }
    else {
        ui->snrPlot->xAxis->setRange(key-xPlotRange, key);
    }
    if (ui->snrPlot->graph(0)->data()->size() > 500)
    {   // remove first points
        qDebug() << "Removing items";
        ui->snrPlot->graph(0)->data()->removeBefore(key - xPlotRange);
    }

    ui->snrPlot->replot();

}
