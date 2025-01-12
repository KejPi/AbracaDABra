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

#include "signaldialog.h"

#include "radiocontrol.h"
#include "ui_signaldialog.h"

const char *SignalDialog::syncLevelLabels[] = {QT_TR_NOOP("No signal"), QT_TR_NOOP("Signal found"), QT_TR_NOOP("Sync")};
const QStringList SignalDialog::snrLevelColors = {"white", "#ff4b4b", "#ffb527", "#5bc214"};
const QString SignalDialog::templateSvgFill =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18"><circle cx="9" cy="9" r="7" fill="%1"/></svg>)";
const QString SignalDialog::templateSvgOutline =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18"><circle cx="9" cy="9" r="6" stroke="%1" stroke-width="2" fill="white"/></svg>)";

SignalDialog::SignalDialog(Settings *settings, int freq, QWidget *parent)
    : QDialog(parent), ui(new Ui::SignalDialog), m_settings(settings), m_frequency(freq)
{
    ui->setupUi(this);

#ifndef Q_OS_MAC
    // Set window flags to add minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
#endif

    int w = ui->snrValue->fontMetrics().boundingRect(" 36.0 dB").width();
    ui->snrValue->setFixedWidth(w);
    ui->snrValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->snrValue->setToolTip(QString(tr("DAB signal SNR")));
    ui->snrValue->setText("");
    ui->snrLabel->setToolTip(ui->snrValue->toolTip());

    w = ui->rfLevelValue->fontMetrics().boundingRect(" -100.0 dBm").width();
    ui->rfLevelValue->setFixedWidth(w);
    ui->rfLevelValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->rfLevelValue->setToolTip(QString(tr("Estimated RF level")));
    ui->rfLevelValue->setText("");
    ui->rfLevelLabel->setToolTip(ui->rfLevelValue->toolTip());

    w = ui->gainValue->fontMetrics().boundingRect(" 88 dB").width();
    ui->gainValue->setFixedWidth(w);
    ui->gainValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->gainValue->setToolTip(QString(tr("Tuner gain")));
    ui->gainValue->setText("");
    ui->gainLabel->setToolTip(ui->gainValue->toolTip());

    w = ui->freqValue->fontMetrics().boundingRect(" 227360 kHz").width();
    ui->freqValue->setFixedWidth(w);
    ui->freqValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->freqValue->setToolTip(QString(tr("Tuned frequency")));
    if (m_frequency > 0)
    {
        ui->freqValue->setText(QString::number(m_frequency) + " kHz");
    }
    else
    {
        ui->freqValue->setText("");
    }
    ui->freqLabel->setToolTip(ui->freqValue->toolTip());

    w = ui->freqOffsetValue->fontMetrics().boundingRect(" -12345.5 Hz").width();
    ui->freqOffsetValue->setFixedWidth(w);
    ui->freqOffsetValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->freqOffsetValue->setToolTip(QString(tr("Estimated frequency offset")));
    ui->freqOffsetValue->setText("");
    ui->freqOffsetLabel->setToolTip(ui->freqOffsetValue->toolTip());

    ui->syncLabel->setText("");
    ui->syncLabel->setFixedSize(18 + 10, 18);

    // ui->snrPlot->plotLayout()->insertRow(0);
    // QCPTextElement *title = new QCPTextElement(ui->snrPlot, "SNR Plot", QFont("sans", 17, QFont::Bold));
    // ui->snrPlot->plotLayout()->addElement(0, 0, title);

    ui->snrPlot->addGraph();
    ui->snrPlot->graph(0)->setLineStyle(QCPGraph::lsStepCenter);
    ui->snrPlot->xAxis->grid()->setSubGridVisible(true);
    ui->snrPlot->yAxis->grid()->setSubGridVisible(true);
    ui->snrPlot->yAxis->setLabel(tr("SNR [dB]"));

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    ui->snrPlot->xAxis->setTicker(timeTicker);
    ui->snrPlot->axisRect()->setupFullAxesBox();
    ui->snrPlot->xAxis->setRange(0, xPlotRange);
    ui->snrPlot->yAxis->setRange(0, 36);
    // ui->snrPlot->xAxis->setLabel(tr("time"));
    ui->snrPlot->setMinimumHeight(180);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->snrPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->snrPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->snrPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->snrPlot->yAxis2, SLOT(setRange(QCPRange)));

    m_spectrumBuffer.assign(2048, 0.0);

    // ui->spectrumPlot->plotLayout()->insertRow(0);
    // title = new QCPTextElement(ui->spectrumPlot, "Signal spectrum", QFont("sans", 17, QFont::Bold));
    // ui->spectrumPlot->plotLayout()->addElement(0, 0, title);

    ui->spectrumPlot->setLocale(QLocale(QLocale::English, QLocale::UnitedKingdom));  // decimal point
    ui->spectrumPlot->setAttribute(Qt::WA_OpaquePaintEvent);
    ui->spectrumPlot->addGraph();
    ui->spectrumPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
    ui->spectrumPlot->xAxis->grid()->setSubGridVisible(true);
    ui->spectrumPlot->yAxis->grid()->setSubGridVisible(true);

    ui->spectrumPlot->axisRect()->setupFullAxesBox();
    ui->spectrumPlot->xAxis->setRange(-1.024, 1.024);
    ui->spectrumPlot->yAxis->setRange(-100, 0);
    ui->spectrumPlot->xAxis2->setRange(-1.024, 1.024);
    ui->spectrumPlot->yAxis2->setRange(-100, 0);

    m_spectYRangeSet = false;

    ui->spectrumPlot->xAxis->setLabel(tr("frequency [MHz]"));
    ui->spectrumPlot->yAxis->setLabel("dBFS");
    ui->spectrumPlot->setMinimumHeight(200);

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    freqTicker->setTickStep(0.2);
    ui->spectrumPlot->xAxis->setTicker(freqTicker);

    QSharedPointer<QCPAxisTickerFixed> levelTicker(new QCPAxisTickerFixed);
    levelTicker->setTickStep(10);
    ui->spectrumPlot->yAxis->setTicker(levelTicker);

    QCPItemStraightLine *verticalLine;
    for (int n = -1; n <= 1; ++n)
    {
        verticalLine = new QCPItemStraightLine(ui->spectrumPlot);
        m_spectLineList.append(verticalLine);
    }
    setFreqRange();

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->spectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->spectrumPlot->xAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->spectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->spectrumPlot->yAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));

    // render level icons
    m_snrLevelIcons[0].loadFromData(templateSvgFill.arg(snrLevelColors.at(0)).toUtf8(), "svg");
    for (int n = 1; n < 4; ++n)
    {
        m_snrLevelIcons[n].loadFromData(templateSvgOutline.arg(snrLevelColors.at(n)).toUtf8(), "svg");
        m_snrLevelIcons[n + 3].loadFromData(templateSvgFill.arg(snrLevelColors.at(n)).toUtf8(), "svg");
    }

    m_timer = new QTimer;
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, [this]() { setSignalState(0, 0.0); });
    setSignalState(0, 0.0);

    if (!m_settings->signal.splitterState.isEmpty())
    {
        ui->splitter->restoreState(m_settings->signal.splitterState);
    }

    QSize sz = QSize(750, 500);
    if (!m_settings->signal.geometry.isEmpty())
    {
        restoreGeometry(m_settings->signal.geometry);
        sz = size();
    }
    QTimer::singleShot(10, this, [this, sz]() { resize(sz); });

    QTimer::singleShot(100, this,
                       [this]()
                       {
                           setRfLevelVisible(false);
                           setGainVisible(false);
                           emit setSignalSpectrum(true);
                       });
}

SignalDialog::~SignalDialog()
{
    m_timer->stop();
    delete m_timer;

    delete ui;
}

void SignalDialog::setSignalState(uint8_t sync, float snr)
{
    ui->snrValue->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
    int syncSnrLevel = 0;
    if (sync > static_cast<int>(DabSyncLevel::NoSync))
    {
        syncSnrLevel = 3;
        if (snr < 7.0)
        {
            syncSnrLevel = 1;
        }
        else if (snr < 10.0)
        {
            syncSnrLevel = 2;
        }
        if (sync == static_cast<int>(DabSyncLevel::FullSync))
        {
            syncSnrLevel += 3;
        }
    }
    if (syncSnrLevel != m_syncSnrLevel)
    {
        m_syncSnrLevel = syncSnrLevel;
        ui->syncLabel->setPixmap(m_snrLevelIcons[m_syncSnrLevel]);
        ui->syncLabel->setToolTip(syncLevelLabels[sync]);
    }

    addToPlot(snr);

    m_timer->start();
}

void SignalDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
        auto plot = ui->snrPlot;
        for (int p = 0; p < 2; ++p)
        {
            plot->xAxis->setBasePen(QPen(Qt::white, 0));
            plot->yAxis->setBasePen(QPen(Qt::white, 0));
            plot->xAxis2->setBasePen(QPen(Qt::white, 0));
            plot->yAxis2->setBasePen(QPen(Qt::white, 0));
            plot->xAxis->setTickPen(QPen(Qt::white, 0));
            plot->yAxis->setTickPen(QPen(Qt::white, 0));
            plot->xAxis2->setTickPen(QPen(Qt::white, 0));
            plot->yAxis2->setTickPen(QPen(Qt::white, 0));
            plot->xAxis->setSubTickPen(QPen(Qt::white, 0));
            plot->yAxis->setSubTickPen(QPen(Qt::white, 0));
            plot->xAxis2->setSubTickPen(QPen(Qt::white, 0));
            plot->yAxis2->setSubTickPen(QPen(Qt::white, 0));
            plot->xAxis->setTickLabelColor(Qt::white);
            plot->yAxis->setTickLabelColor(Qt::white);
            plot->xAxis2->setTickLabelColor(Qt::white);
            plot->yAxis2->setTickLabelColor(Qt::white);
            plot->xAxis->setLabelColor(Qt::white);
            plot->yAxis->setLabelColor(Qt::white);
            plot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            plot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine));
            plot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            plot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->setBackground(QBrush(Qt::black));

            plot = ui->spectrumPlot;
        }

        ui->snrPlot->graph(0)->setPen(QPen(Qt::cyan, 2));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 255, 255, 100)));
        ui->snrPlot->replot();

        ui->spectrumPlot->graph(0)->setPen(QPen(Qt::cyan));

        for (int n = 0; n < 3; ++n)
        {
            if (n == 1)
            {
                m_spectLineList.at(n)->setPen(QPen(Qt::magenta, 1, Qt::SolidLine));
            }
            else
            {
                m_spectLineList.at(n)->setPen(QPen(Qt::white, 1, Qt::DotLine));
            }
        }

        ui->spectrumPlot->replot();
    }
    else
    {
        auto plot = ui->snrPlot;
        for (int p = 0; p < 2; ++p)
        {
            plot->xAxis->setBasePen(QPen(Qt::black, 0));
            plot->yAxis->setBasePen(QPen(Qt::black, 0));
            plot->xAxis2->setBasePen(QPen(Qt::black, 0));
            plot->yAxis2->setBasePen(QPen(Qt::black, 0));
            plot->xAxis->setTickPen(QPen(Qt::black, 0));
            plot->yAxis->setTickPen(QPen(Qt::black, 0));
            plot->xAxis2->setTickPen(QPen(Qt::black, 0));
            plot->yAxis2->setTickPen(QPen(Qt::black, 0));
            plot->xAxis->setSubTickPen(QPen(Qt::black, 0));
            plot->yAxis->setSubTickPen(QPen(Qt::black, 0));
            plot->xAxis2->setSubTickPen(QPen(Qt::black, 0));
            plot->yAxis2->setSubTickPen(QPen(Qt::black, 0));
            plot->xAxis->setTickLabelColor(Qt::black);
            plot->yAxis->setTickLabelColor(Qt::black);
            plot->xAxis2->setTickLabelColor(Qt::black);
            plot->yAxis2->setTickLabelColor(Qt::black);
            plot->xAxis->setLabelColor(Qt::black);
            plot->yAxis->setLabelColor(Qt::black);
            plot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            plot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
            plot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            plot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->setBackground(QBrush(Qt::white));

            plot = ui->spectrumPlot;
        }

        ui->snrPlot->graph(0)->setPen(QPen(Qt::blue, 2));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 0, 255, 100)));
        ui->snrPlot->replot();

        ui->spectrumPlot->setBackground(QBrush(Qt::white));
        ui->spectrumPlot->graph(0)->setPen(QPen(Qt::blue));
        for (int n = 0; n < 3; ++n)
        {
            if (n == 1)
            {
                m_spectLineList.at(n)->setPen(QPen(Qt::red, 1, Qt::SolidLine));
            }
            else
            {
                m_spectLineList.at(n)->setPen(QPen(Qt::darkGray, 1, Qt::DotLine));
            }
        }

        ui->spectrumPlot->replot();
    }
}

void SignalDialog::setFreqRange()
{
    for (int n = 0; n < 3; ++n)
    {
        m_spectLineList.at(n)->point1->setCoords(((n - 1) * 768 + m_frequency) * 0.001, -100);
        m_spectLineList.at(n)->point2->setCoords(((n - 1) * 768 + m_frequency) * 0.001, 0);
    }
    ui->spectrumPlot->xAxis->setRange((-1024 + m_frequency) * 0.001, (1024 + m_frequency) * 0.001);
}

void SignalDialog::reset()
{
    m_spectrumBuffer.assign(2048, 0.0);
    m_avrgCntr = 0;
    ui->spectrumPlot->graph(0)->data()->clear();
    ui->spectrumPlot->yAxis->setRange(-100, 0);
    m_spectYRangeSet = false;
}

void SignalDialog::setRfLevelVisible(bool visible)
{
    if (visible != ui->rfLevelValue->isVisible())
    {
        ui->rfLevelLabel->setVisible(visible);
        ui->rfLevelValue->setVisible(visible);
        ui->rfLevelVLine->setVisible(visible);
    }
}

void SignalDialog::setGainVisible(bool visible)
{
    if (visible != ui->gainValue->isVisible())
    {
        ui->gainLabel->setVisible(visible);
        ui->gainValue->setVisible(visible);
        ui->gainVLine->setVisible(visible);

        // change plot label
        if (visible)
        {
            ui->spectrumPlot->yAxis->setLabel("dBm");
        }
        else
        {
            ui->spectrumPlot->yAxis->setLabel("dBFS");
        }
    }
}

void SignalDialog::onTuneDone(uint32_t freq)
{
    if (freq == m_frequency)
    {
        return;
    }
    m_frequency = freq;
    reset();
    setFreqRange();
    ui->freqValue->setText(QString::number(m_frequency) + " kHz");
    ui->spectrumPlot->replot();
}

void SignalDialog::updateRfLevel(float rfLevel, float gain)
{
    m_rfLevel = rfLevel;
    m_tunerGain = gain;
    if (std::isnan(gain))
    {
        ui->rfLevelValue->setText(tr("N/A"));
        setGainVisible(false);
    }
    else
    {
        ui->gainValue->setText(QString::number(static_cast<int>(gain)) + " dB");
        setGainVisible(true);
    }

    if (std::isnan(rfLevel))
    {  // level is not available (input device in HW mode or not RTL-SDR)
        ui->rfLevelValue->setText(tr("N/A"));
        setRfLevelVisible(false);
    }
    else
    {
        ui->rfLevelValue->setText(QString::number(double(rfLevel), 'f', 1) + " dBm");
        setRfLevelVisible(true);
    }
}

void SignalDialog::updateFreqOffset(float offset)
{
    ui->freqOffsetValue->setText(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void SignalDialog::onSignalSpectrum(std::shared_ptr<std::vector<float> > data)
{
    // static QElapsedTimer timer;
    // qDebug() << Q_FUNC_INFO << timer.restart();

    int idx = 0;
    for (auto it = m_spectrumBuffer.begin(); it != m_spectrumBuffer.end(); ++it)
    {
        *it += data->at(idx++);
    }

    if (++m_avrgCntr >= 10)
    {
        // qDebug() << *std::min_element(m_spectrumBuffer.cbegin(), m_spectrumBuffer.cend())*0.1 << *std::max_element(m_spectrumBuffer.cbegin(),
        // m_spectrumBuffer.cend())*0.1; qDebug() << Q_FUNC_INFO;
        m_avrgCntr = 0;

        // -10 dB is averaging factor 1/10
        // 66.227dB is FFT gain 2048
        float offset_dB = -10.0 - 66.227;
        if (m_settings->inputDevice == InputDeviceId::RTLSDR || m_settings->inputDevice == InputDeviceId::RTLTCP ||
            m_settings->inputDevice == InputDeviceId::RAWFILE)
        {  // input is -128 .. +127  ==> * 1/128 = -42.144 dB
            offset_dB = offset_dB - 42.144;
        }
        if (!std::isnan(m_tunerGain))
        {
            offset_dB -= m_tunerGain - 3;
        }

        ui->spectrumPlot->graph(0)->data()->clear();
        float freq = 0;
        // float sum1536 = -m_spectrumBuffer.front();
        float minVal = 1000;
        float maxVal = -1000;
        for (auto it = m_spectrumBuffer.begin(); it != m_spectrumBuffer.end(); ++it)
        {
            float val = 10 * std::log10(*it) + offset_dB;
            if (val < -200)
            {
                val = -200;
            }
            minVal = (val < minVal) ? val : minVal;
            maxVal = (val > maxVal) ? val : maxVal;

            ui->spectrumPlot->graph(0)->addData((freq + m_frequency) * 0.001, val);

            // if ((freq >= -768.0) && (freq <= 768.0))
            // {
            //     sum1536 += (*it) * (*it);
            // }

            freq += 1.0;

            if (freq == 1024.0)
            {
                freq = -1024.0;
            }
            *it = 0.0;
        }

        auto range = ui->spectrumPlot->yAxis->range();
        if (m_spectYRangeSet)
        {
            if ((minVal - range.lower) < 0)
            {  // set minumum to at least minVal + 10, use multiples of 10
                range.lower = std::fmaxf(ceilf((minVal - 10) / 10.0) * 10.0, -120.0);
            }
            if ((range.upper - maxVal) < 0)
            {
                range.upper = std::fminf(floorf((maxVal + 10) / 10.0) * 10.0, 0.0);
            }
        }
        else
        {
            if ((minVal - range.lower) < 0 || (minVal - range.lower) > 20)
            {  // set minumum to at least minVal + 10, use multiples of 10
                range.lower = std::fmaxf(ceilf((minVal - 10) / 10.0) * 10.0, -120.0);
            }
            if ((range.upper - maxVal) < 0 || (range.upper - maxVal) > 20)
            {
                range.upper = std::fminf(floorf((maxVal + 10) / 10.0) * 10.0, 0);
            }
            m_spectYRangeSet = true;
        }

        ui->spectrumPlot->yAxis->setRange(range);
        ui->spectrumPlot->replot();
    }
}

void SignalDialog::closeEvent(QCloseEvent *event)
{
    emit setSignalSpectrum(false);

    m_settings->signal.geometry = saveGeometry();
    m_settings->signal.splitterState = ui->splitter->saveState();

    QDialog::closeEvent(event);
}

void SignalDialog::addToPlot(float snr)
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
    else
    {
        ui->snrPlot->xAxis->setRange(key - xPlotRange, key);
    }
    if (ui->snrPlot->graph(0)->data()->size() > 500)
    {  // remove first points
        ui->snrPlot->graph(0)->data()->removeBefore(key - xPlotRange);
    }

    ui->snrPlot->replot();
}
