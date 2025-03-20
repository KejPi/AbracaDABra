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

    w = ui->rfLevelValue->fontMetrics().boundingRect("-100.0 dBm").width();
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

    w = ui->freqOffsetValue->fontMetrics().boundingRect("-12345.5 Hz").width();
    ui->freqOffsetValue->setFixedWidth(w);
    ui->freqOffsetValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->freqOffsetValue->setToolTip(QString(tr("Estimated frequency offset")));
    ui->freqOffsetValue->setText("");
    ui->freqOffsetLabel->setToolTip(ui->freqOffsetValue->toolTip());

    ui->syncLabel->setText("");
    ui->syncLabel->setFixedSize(18 + 10, 18);

    ui->snrPlot->addGraph();
    ui->snrPlot->graph(0)->setLineStyle(QCPGraph::lsStepCenter);
    ui->snrPlot->xAxis->grid()->setSubGridVisible(true);
    ui->snrPlot->yAxis->grid()->setSubGridVisible(false);
    ui->snrPlot->yAxis->setLabel(tr("SNR [dB]"));

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->snrPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->snrPlot->xAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->snrPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->snrPlot->yAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s");
    ui->snrPlot->xAxis->setTicker(timeTicker);
    ui->snrPlot->xAxis2->setTicker(timeTicker);
    ui->snrPlot->axisRect()->setupFullAxesBox();
    ui->snrPlot->xAxis->setRange(0, xPlotRange);
    ui->snrPlot->yAxis->setRange(0, 36);
    ui->snrPlot->setMinimumHeight(180);

    QFont font = ui->snrPlot->font();
    font.setBold(true);
    font.setPointSize(50);
    m_snrPlotText = new QCPTextElement(ui->snrPlot, "", font);
    m_snrPlotText->setMaximumSize(250, 10000);
    m_snrPlotText->setVisible(false);

    m_spectrumBuffer.assign(2048, 0.0);

    ui->spectrumPlot->setLocale(QLocale(QLocale::English, QLocale::UnitedKingdom));  // decimal point
    ui->spectrumPlot->setAttribute(Qt::WA_OpaquePaintEvent);
    ui->spectrumPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);
    ui->spectrumPlot->addGraph();
    ui->spectrumPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
    ui->spectrumPlot->xAxis->grid()->setSubGridVisible(true);
    ui->spectrumPlot->xAxis2->grid()->setSubGridVisible(true);
    ui->spectrumPlot->yAxis->grid()->setSubGridVisible(true);
    ui->spectrumPlot->yAxis2->grid()->setSubGridVisible(true);

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->spectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->spectrumPlot->xAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->spectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->spectrumPlot->yAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->spectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &SignalDialog::onSpectXRangeChanged);
    connect(ui->spectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &SignalDialog::onSpectYRangeChanged);

    m_spectYRangeSet = false;
    m_spectYRangeMin = -140;
    m_spectYRangeMax = 0.0;

    ui->spectrumPlot->axisRect()->setupFullAxesBox();
    ui->spectrumPlot->xAxis->setRange(-1.024, 1.024);
    ui->spectrumPlot->yAxis->setRange(m_spectYRangeMin, m_spectYRangeMax);
    ui->spectrumPlot->xAxis->setLabel(tr("frequency [MHz]"));
    ui->spectrumPlot->yAxis->setLabel("dBFS");
    ui->spectrumPlot->setMinimumHeight(200);

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    freqTicker->setTickStep(0.2);
    ui->spectrumPlot->xAxis->setTicker(freqTicker);
    ui->spectrumPlot->xAxis2->setTicker(freqTicker);

    QSharedPointer<QCPAxisTickerFixed> levelTicker(new QCPAxisTickerFixed);
    levelTicker->setTickStep(10);
    ui->spectrumPlot->yAxis->setTicker(levelTicker);
    ui->spectrumPlot->yAxis2->setTicker(levelTicker);

    QCPItemStraightLine *verticalLine;
    for (int n = -1; n <= 1; ++n)
    {
        verticalLine = new QCPItemStraightLine(ui->spectrumPlot);
        m_spectLineList.append(verticalLine);
    }
    setFreqRange();

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(ui->spectrumPlot, &QCustomPlot::selectionChangedByUser, this, &SignalDialog::onSpectPlotSelectionChanged);
    // connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
    connect(ui->spectrumPlot, &QCustomPlot::mousePress, this, &SignalDialog::onSpectPlotMousePress);
    connect(ui->spectrumPlot, &QCustomPlot::mouseWheel, this, &SignalDialog::onSpectPlotMouseWheel);

    // setup policy and connect slot for context menu popup:
    ui->spectrumPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->spectrumPlot, &QCustomPlot::customContextMenuRequested, this, &SignalDialog::onSpectContextMenuRequest);
    connect(ui->spectrumPlot, &QCustomPlot::mouseMove, this, &SignalDialog::spectShowPointToolTip);

    m_isUserView = false;

    auto menu = new QMenu(this);
    auto fitInViewAction = new QAction(tr("Fit spectrum in view"), menu);
    connect(fitInViewAction, &QAction::triggered, this, [this]() { spectFitInView(); });
    menu->addAction(fitInViewAction);
    menu->addSeparator();

    auto syncSpectAction = new QAction(tr("Frequency offset correction"), menu);
    syncSpectAction->setCheckable(true);
    syncSpectAction->setChecked(m_settings->signal.spectrumMode == 2);
    connect(syncSpectAction, &QAction::triggered, this,
            [this](bool checked)
            {
                m_settings->signal.spectrumMode = checked ? 2 : 1;
                emit setSignalSpectrum((m_settings->signal.spectrumMode));
            });

    menu->addAction(syncSpectAction);
    menu->addSeparator();
    auto slow = new QAction(tr("Slow update (1 sec)"), menu);
    auto norm = new QAction(tr("Normal update (500 msec)"), menu);
    auto fast = new QAction(tr("Fast update (300 msec)"), menu);
    auto faster = new QAction(tr("Very fast update (100 msec)"), menu);
    slow->setData(SpectrumUpdateSlow);
    norm->setData(SpectrumUpdateNormal);
    fast->setData(SpectrumUpdateFast);
    faster->setData(SpectrumUpdateVeryFast);
    slow->setCheckable(true);
    norm->setCheckable(true);
    fast->setCheckable(true);
    faster->setCheckable(true);
    auto actGroup = new QActionGroup(menu);
    actGroup->addAction(slow);
    actGroup->addAction(norm);
    actGroup->addAction(fast);
    actGroup->addAction(faster);
    menu->addActions(actGroup->actions());
    menu->addSeparator();

    // using the fact that only one can be checked in the group
    norm->setChecked(true);  // default
    slow->setChecked(m_settings->signal.spectrumUpdate == SpectrumUpdateSlow);
    fast->setChecked(m_settings->signal.spectrumUpdate == SpectrumUpdateFast);
    faster->setChecked(m_settings->signal.spectrumUpdate == SpectrumUpdateVeryFast);

    connect(actGroup, &QActionGroup::triggered, this, [this](QAction *action) { setSpectrumUpdate(action->data().toInt()); });
    setSpectrumUpdate(m_settings->signal.spectrumUpdate);

    auto snrTextAction = new QAction(tr("Show SNR value"), menu);
    snrTextAction->setCheckable(true);
    snrTextAction->setChecked(m_settings->signal.showSNR);
    if (m_settings->signal.showSNR)
    {
        setSnrValueText();
    }
    connect(snrTextAction, &QAction::triggered, this,
            [this]()
            {
                m_settings->signal.showSNR = !m_settings->signal.showSNR;
                setSnrValueText();
            });
    menu->addAction(snrTextAction);

    ui->menuLabel->setToolTip(tr("Configuration"));
    ui->menuLabel->setMenu(menu);

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
                       });
}

SignalDialog::~SignalDialog()
{
    m_timer->stop();
    delete m_timer;

    delete ui;
}

void SignalDialog::setInputDevice(InputDevice::Id id)
{
    // 66.227dB is FFT gain 2048
    m_offset_dB = -66.227;

    switch (id)
    {
        case InputDevice::Id::RTLSDR:
            // input is -128 .. +127  ==> * 1/128 = -42.144 dB
            m_offset_dB = m_offset_dB - 42.144 + 3;  // +3 is empirical correction factor -> not clear where it comes from
            break;
        case InputDevice::Id::RTLTCP:
            // input is -128 .. +127  ==> * 1/128 = -42.144 dB
            m_offset_dB = m_offset_dB - 42.144;
            break;
        case InputDevice::Id::RAWFILE:
            // distinguish input format
            if (m_settings->rawfile.format == RawFileInputFormat::SAMPLE_FORMAT_S16)
            {  // input is -32768 .. +32767  ==> * 1/32768 = -90.309 dB
                m_offset_dB = m_offset_dB - 90.309;
            }
            else
            {  // input is -128 .. +127  ==> * 1/128 = -42.144 dB
                m_offset_dB = m_offset_dB - 42.144;
            }

            break;
        case InputDevice::Id::SDRPLAY:
            m_offset_dB = m_offset_dB + 8;  // +8 is empirical correction factor -> not clear where it comes from
            break;
        case InputDevice::Id::RARTTCP:
        case InputDevice::Id::AIRSPY:
        case InputDevice::Id::SOAPYSDR:
        case InputDevice::Id::UNDEFINED:
            break;
    }

    ui->menuLabel->setEnabled(id != InputDevice::Id::UNDEFINED);

    // enable spectrum
    emit setSignalSpectrum(id == InputDevice::Id::UNDEFINED ? 0 : m_settings->signal.spectrumMode);
}

void SignalDialog::setSignalState(uint8_t sync, float snr)
{
    ui->snrValue->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
    m_snrPlotText->setText(ui->snrValue->text());
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
            plot->xAxis->setBasePen(QPen(Qt::white, 1));
            plot->yAxis->setBasePen(QPen(Qt::white, 1));
            plot->xAxis2->setBasePen(QPen(Qt::white, 1));
            plot->yAxis2->setBasePen(QPen(Qt::white, 1));
            plot->xAxis->setTickPen(QPen(Qt::white, 1));
            plot->yAxis->setTickPen(QPen(Qt::white, 1));
            plot->xAxis2->setTickPen(QPen(Qt::white, 1));
            plot->yAxis2->setTickPen(QPen(Qt::white, 1));
            plot->xAxis->setSubTickPen(QPen(Qt::white, 1));
            plot->yAxis->setSubTickPen(QPen(Qt::white, 1));
            plot->xAxis2->setSubTickPen(QPen(Qt::white, 1));
            plot->yAxis2->setSubTickPen(QPen(Qt::white, 1));
            plot->xAxis->setTickLabelColor(Qt::white);
            plot->yAxis->setTickLabelColor(Qt::white);
            plot->xAxis2->setTickLabelColor(Qt::white);
            plot->yAxis2->setTickLabelColor(Qt::white);
            plot->xAxis->setLabelColor(Qt::white);
            plot->yAxis->setLabelColor(Qt::white);
            plot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            if (devicePixelRatio() > 1)
            {
                plot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine));
            }
            else
            {
                plot->yAxis->grid()->setPen(QPen(Qt::white, 1, Qt::DashLine));
            }
            plot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            plot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
            plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->setBackground(QBrush(Qt::black));

            QColor axisSelectionColor(Qt::red);
            axisSelectionColor = qApp->style()->standardPalette().color(QPalette::Active, QPalette::Link);
            plot->xAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedTickLabelColor(axisSelectionColor);
            plot->xAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedTickLabelColor(axisSelectionColor);
            plot->yAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedTickLabelColor(axisSelectionColor);
            plot->yAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedTickLabelColor(axisSelectionColor);

            plot = ui->spectrumPlot;
        }

        ui->snrPlot->graph(0)->setPen(QPen(Qt::cyan, 1 + 1));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 255, 255, 100)));
        m_snrPlotText->setTextColor(Qt::white);
        ui->snrPlot->replot();

        ui->spectrumPlot->graph(0)->setPen(QPen(Qt::cyan));

        for (int n = 0; n < 3; ++n)
        {
            if (n == 1)
            {
                m_spectLineList.at(n)->setPen(QPen(QColor(255, 84, 84), 1, Qt::SolidLine));
            }
            else
            {
                if (devicePixelRatio() > 1)
                {
                    m_spectLineList.at(n)->setPen(QPen(Qt::white, 1, Qt::DotLine));
                }
                else
                {
                    m_spectLineList.at(n)->setPen(QPen(Qt::white, 1, Qt::DashLine));
                }
            }
        }

        ui->spectrumPlot->replot();
        ui->menuLabel->setIcon(":/resources/menu_dark.png");
    }
    else
    {
        auto plot = ui->snrPlot;
        for (int p = 0; p < 2; ++p)
        {
            plot->xAxis->setBasePen(QPen(Qt::black, 1));
            plot->yAxis->setBasePen(QPen(Qt::black, 1));
            plot->xAxis2->setBasePen(QPen(Qt::black, 1));
            plot->yAxis2->setBasePen(QPen(Qt::black, 1));
            plot->xAxis->setTickPen(QPen(Qt::black, 1));
            plot->yAxis->setTickPen(QPen(Qt::black, 1));
            plot->xAxis2->setTickPen(QPen(Qt::black, 1));
            plot->yAxis2->setTickPen(QPen(Qt::black, 1));
            plot->xAxis->setSubTickPen(QPen(Qt::black, 1));
            plot->yAxis->setSubTickPen(QPen(Qt::black, 1));
            plot->xAxis2->setSubTickPen(QPen(Qt::black, 1));
            plot->yAxis2->setSubTickPen(QPen(Qt::black, 1));
            plot->xAxis->setTickLabelColor(Qt::black);
            plot->yAxis->setTickLabelColor(Qt::black);
            plot->xAxis2->setTickLabelColor(Qt::black);
            plot->yAxis2->setTickLabelColor(Qt::black);
            plot->xAxis->setLabelColor(Qt::black);
            plot->yAxis->setLabelColor(Qt::black);
            plot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            if (devicePixelRatio() > 1)
            {
                plot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 1, Qt::DotLine));
            }
            else
            {
                plot->yAxis->grid()->setPen(QPen(Qt::black, 1, Qt::DashLine));
            }
            plot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            plot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
            plot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
            plot->setBackground(QBrush(Qt::white));

            QColor axisSelectionColor(48, 140, 198);
            axisSelectionColor = qApp->style()->standardPalette().color(QPalette::Active, QPalette::Link);
            plot->xAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis->setSelectedTickLabelColor(axisSelectionColor);
            plot->xAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->xAxis2->setSelectedTickLabelColor(axisSelectionColor);
            plot->yAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis->setSelectedTickLabelColor(axisSelectionColor);
            plot->yAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
            plot->yAxis2->setSelectedTickLabelColor(axisSelectionColor);

            plot = ui->spectrumPlot;
        }

        ui->snrPlot->graph(0)->setPen(QPen(Qt::blue, 1 + 1));
        ui->snrPlot->graph(0)->setBrush(QBrush(QColor(0, 0, 255, 100)));
        m_snrPlotText->setTextColor(Qt::black);
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
                if (devicePixelRatio() > 1)
                {
                    m_spectLineList.at(n)->setPen(QPen(Qt::darkGray, 1, Qt::DotLine));
                }
                else
                {
                    m_spectLineList.at(n)->setPen(QPen(Qt::black, 1, Qt::DashLine));
                }
            }
        }

        ui->spectrumPlot->replot();
        ui->menuLabel->setIcon(":/resources/menu.png");
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
    ui->spectrumPlot->xAxis2->setRange(ui->spectrumPlot->xAxis->range());
}

void SignalDialog::reset()
{
    m_spectrumBuffer.assign(2048, 0.0);
    m_avrgCntr = 0;
    ui->spectrumPlot->graph(0)->data()->clear();
    m_spectYRangeSet = false;
    m_spectYRangeMin = -140;
    m_spectYRangeMax = 0;
    ui->spectrumPlot->yAxis->setRange(m_spectYRangeMin, m_spectYRangeMax);
    m_isUserView = false;
    ui->menuLabel->setEnabled(false);
    updateRfLevel(NAN, NAN);
    setRfLevelVisible(false);
    setGainVisible(false);
    setSignalState(0, 0.0);
    ui->freqOffsetValue->setText(tr("N/A"));
}

void SignalDialog::setSpectrumUpdate(int mode)
{
    switch (mode)
    {
        case SpectrumUpdateSlow:
            m_settings->signal.spectrumUpdate = SpectrumUpdateSlow;
            m_numAvrg = 20;
            m_avrgFactor_dB = -13.010;  // 10 *log10(1/20)
            break;
        case SpectrumUpdateFast:
            m_settings->signal.spectrumUpdate = SpectrumUpdateFast;
            m_numAvrg = 6;
            m_avrgFactor_dB = -7.7815;  // 10 *log10(1/6)
            break;
        case SpectrumUpdateVeryFast:
            m_settings->signal.spectrumUpdate = SpectrumUpdateVeryFast;
            m_numAvrg = 2;
            m_avrgFactor_dB = -3.0103;  // 10 *log10(1/2)
            break;
        default:
            m_settings->signal.spectrumUpdate = SpectrumUpdateNormal;
            m_numAvrg = 10;
            m_avrgFactor_dB = -10;  // 10 *log10(1/10)
            break;
    }
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
    if (m_frequency != 0)
    {
        ui->freqValue->setText(QString::number(m_frequency) + " kHz");
        ui->menuLabel->setEnabled(true);
    }
    else
    {
        ui->freqValue->setText("");
    }
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
        m_spectYRangeMin = -140;
        m_spectYRangeMax = 0;
    }
    else
    {
        ui->rfLevelValue->setText(QString::number(double(rfLevel), 'f', 1) + " dBm");
        setRfLevelVisible(true);
        m_spectYRangeMin = floorf((-100 + rfLevel) / 10.0) * 10.0;
        m_spectYRangeMax = ceilf((0 + rfLevel) / 10.0) * 10.0;
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

    if (++m_avrgCntr >= m_numAvrg)
    {
        // qDebug() << *std::min_element(m_spectrumBuffer.cbegin(), m_spectrumBuffer.cend())*0.1 << *std::max_element(m_spectrumBuffer.cbegin(),
        // m_spectrumBuffer.cend())*0.1; qDebug() << Q_FUNC_INFO;
        m_avrgCntr = 0;
        float offset_dB = m_offset_dB + m_avrgFactor_dB;
        if (!std::isnan(m_tunerGain))
        {
            offset_dB -= m_tunerGain;
        }

        ui->spectrumPlot->graph(0)->data()->clear();
        float freq = 0;
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
            freq += 1.0;

            if (freq == 1024.0)
            {
                freq = -1024.0;
            }
            *it = 0.0;
        }

        // qDebug() << maxVal << offset_dB;
        if (m_isUserView == false)
        {
            auto range = ui->spectrumPlot->yAxis->range();
            // if (m_spectYRangeSet)
            // {
            //     if (minVal < range.lower)
            //     {  // set minumum to at least minVal + 10, use multiples of 10
            //         range.lower = std::fmaxf(ceilf((minVal - 10) / 10.0) * 10.0, m_spectYRangeMin);
            //     }
            //     if (range.upper < maxVal)
            //     {
            //         range.upper = std::fminf(floorf((maxVal + 10) / 10.0) * 10.0, m_spectYRangeMax);
            //     }
            // }
            // else
            // {
            if ((minVal - range.lower) < 0 || (minVal - range.lower) > 20)
            {  // set minumum to at least minVal + 10, use multiples of 10
                range.lower = std::fmaxf(ceilf((minVal - 10) / 10.0) * 10.0, m_spectYRangeMin);
            }
            if ((range.upper - maxVal) < 0 || (range.upper - maxVal) > 20)
            {
                range.upper = std::fminf(floorf((maxVal + 10) / 10.0) * 10.0, m_spectYRangeMax);
            }
            m_spectYRangeSet = true;
            //            }
            ui->spectrumPlot->yAxis->setRange(range);
        }
        ui->spectrumPlot->replot();
    }
}

void SignalDialog::closeEvent(QCloseEvent *event)
{
    emit setSignalSpectrum(0);

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

    // make key axis range scroll with the data
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

void SignalDialog::setSnrValueText()
{
    if (m_settings->signal.showSNR)
    {
        ui->snrPlot->plotLayout()->insertColumn(ui->snrPlot->plotLayout()->columnCount());
        ui->snrPlot->plotLayout()->addElement(0, ui->snrPlot->plotLayout()->columnCount() - 1, m_snrPlotText);
        m_snrPlotText->setVisible(true);
    }
    else
    {
        m_snrPlotText->setVisible(false);
        ui->snrPlot->plotLayout()->take(m_snrPlotText);
        ui->snrPlot->plotLayout()->simplify();
    }
    ui->snrPlot->replot();
}

void SignalDialog::onSpectPlotSelectionChanged()
{
    // normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
    // the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
    // and the axis base line together.

    // The selection state of the left and right axes shall be synchronized as well as the state of the
    // bottom and top axes.

    // make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (ui->spectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
        ui->spectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        ui->spectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxisLabel) ||
        ui->spectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
        ui->spectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        ui->spectrumPlot->xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->spectrumPlot->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels | QCPAxis::spAxisLabel);
    }
    // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (ui->spectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
        ui->spectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        ui->spectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
        ui->spectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        ui->spectrumPlot->yAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        ui->spectrumPlot->yAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
    }
}

void SignalDialog::onSpectPlotMousePress(QMouseEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be dragged
    // if no axis is selected, both directions may be dragged

    if (ui->spectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->spectrumPlot->axisRect()->setRangeDrag(ui->spectrumPlot->xAxis->orientation());
    }
    else if (ui->spectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->spectrumPlot->axisRect()->setRangeDrag(ui->spectrumPlot->yAxis->orientation());
    }
    else
    {
        ui->spectrumPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    }
}

void SignalDialog::onSpectPlotMouseWheel(QWheelEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be zoomed
    // if no axis is selected, both directions may be zoomed
    if (ui->spectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->spectrumPlot->axisRect()->setRangeZoom(ui->spectrumPlot->xAxis->orientation());
    }
    else if (ui->spectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->spectrumPlot->axisRect()->setRangeZoom(ui->spectrumPlot->yAxis->orientation());
    }
    else
    {
        ui->spectrumPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    }
    m_isUserView = true;
}

void SignalDialog::onSpectXRangeChanged(const QCPRange &newRange)
{
    double lowerBound = (-1024 + m_frequency) * 0.001;
    double upperBound = (1024 + m_frequency) * 0.001;
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        ui->spectrumPlot->xAxis->setRange(fixedRange);
        m_isUserView = true;
    }
    else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        ui->spectrumPlot->xAxis->setRange(fixedRange);
        m_isUserView = true;
    }
}

void SignalDialog::onSpectYRangeChanged(const QCPRange &newRange)
{
    double lowerBound = -200;
    double upperBound = 10;
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        ui->spectrumPlot->yAxis->setRange(fixedRange);
        m_isUserView = true;
    }
    else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        ui->spectrumPlot->yAxis->setRange(fixedRange);
        m_isUserView = true;
    }
}

void SignalDialog::spectShowPointToolTip(QMouseEvent *event)
{
    double x = ui->spectrumPlot->xAxis->pixelToCoord(event->pos().x());
    auto xrange = ui->spectrumPlot->xAxis->range();
    if (x >= xrange.lower && x <= xrange.upper)
    {
        double y = ui->spectrumPlot->yAxis->pixelToCoord(event->pos().y());
        auto yrange = ui->spectrumPlot->yAxis->range();
        if (y >= yrange.lower && y <= yrange.upper)
        {
            ui->spectrumPlot->setToolTip(
                QString(tr("%1 MHz<br>%2 %3")).arg(x, 6, 'f', 3, QChar('0')).arg(y, 4, 'f', 1, QChar('0')).arg(ui->spectrumPlot->yAxis->label()));
            return;
        }
    }
    ui->spectrumPlot->setToolTip("");
}

void SignalDialog::onSpectContextMenuRequest(QPoint pos)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto fitInViewAction = new QAction(tr("Fit in view"), this);
    connect(fitInViewAction, &QAction::triggered, this, [this]() { spectFitInView(); });

    connect(menu, &QWidget::destroyed, [=]() { // delete actions
            fitInViewAction->deleteLater();
        });

    menu->addAction(fitInViewAction);
    menu->popup(ui->spectrumPlot->mapToGlobal(pos));
}

void SignalDialog::spectFitInView()
{
    ui->spectrumPlot->rescaleAxes();
    ui->spectrumPlot->deselectAll();
    ui->spectrumPlot->replot();
    m_spectYRangeSet = false;
    m_isUserView = false;
}
