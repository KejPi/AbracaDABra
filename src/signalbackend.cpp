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

#include "signalbackend.h"

#include <QtCore/qnamespace.h>

#include "linechartitem.h"
#include "radiocontrol.h"
#include "waterfallitem.h"

SignalBackend::SignalBackend(Settings *settings, int freq, QObject *parent) : UIControlProvider(parent), m_settings(settings), m_frequency(freq)
{
    frequencyLabel(m_frequency > 0 ? QString::number(m_frequency) + " kHz" : "");

    m_signalSpectrumBuffer.assign(2048, 0.0);
    m_nullSpectrumBuffer.assign(2048, 0.0);
    m_spectYRangeMin = -140;
    m_spectYRangeMax = 0.0;
    m_spectYViewMin = m_spectYRangeMin;
    m_spectYViewMax = m_spectYRangeMax;

    m_timer = new QTimer;
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, [this]() { setSignalState(0, 0.0); });

    connect(this, &SignalBackend::spectrumUpdateChanged, this, &SignalBackend::setSpectrumUpdate);
    connect(this, &SignalBackend::showNULLChanged, this,
            [this]()
            {
                if (m_settings->signal.showNULL == false && m_spectrumPlotItems.isEmpty() == false)
                {
                    for (LineChartItem *plot : std::as_const(m_spectrumPlotItems))
                    {
                        plot->clear(m_nullSpectSeriesId);
                    }
                }
            });
    setSpectrumUpdate();
}

SignalBackend::~SignalBackend()
{
    m_timer->stop();
    delete m_timer;
}

void SignalBackend::registerSpectrumPlot(QQuickItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    LineChartItem *spectrum = dynamic_cast<LineChartItem *>(item);
    if (spectrum && m_spectrumPlotItems.contains(spectrum) == false)
    {
        if (m_spectrumPlotItems.isEmpty())
        {
            emit startSignalSpectrum(true);
        }

        m_spectrumPlotItems.append(spectrum);
        spectrum->setXLabelDecimals(1);
        spectrum->setYLabelDecimals(0);
        spectrum->setYMin(-200);
        spectrum->setYMax(10);
        spectrum->setDefaultYMin(-200);
        spectrum->setDefaultYMax(10);
        spectrum->setAutoRangeX(true);
        spectrum->setAutoRangeY(true);
        spectrum->resetToAutoRangeY();

        spectrum->setMinXSpan(0.064);
        spectrum->setMaxXSpan(2.048);

        spectrum->setXAxisTitle(tr("Frequency [MHz]"));
        spectrum->setYAxisTitle(tr("dBFS"));

        m_signalSpectSeriesId = spectrum->addSeries("signal", Qt::cyan, 1.0);
        spectrum->setSeriesStyle(m_signalSpectSeriesId, (int)LineSeries::Normal);

        m_nullSpectSeriesId = spectrum->addSeries("null", QColor(0xdfd274), 1.0);
        spectrum->setSeriesStyle(m_nullSpectSeriesId, (int)LineSeries::Normal);

        // Add a vertical marker lines
        m_spectLeftMarginId = spectrum->addMarkerLine(true, -768e-3);
        m_spectRightMarginId = spectrum->addMarkerLine(true, 768e-3);
        m_spectCenterId = spectrum->addMarkerLine(true, 0.0);

        QPen pen;
        pen.setColor(Qt::white);
        pen.setWidth(1);
        pen.setStyle(Qt::DotLine);
        spectrum->setMarkerLinePen(m_spectLeftMarginId, pen);
        spectrum->setMarkerLinePen(m_spectRightMarginId, pen);

        pen.setColor(QColor(255, 84, 84));
        pen.setWidth(1);
        pen.setStyle(Qt::SolidLine);
        spectrum->setMarkerLinePen(m_spectCenterId, pen);

        connect(spectrum, &LineChartItem::autoRangeYChanged, this,
                [this, spectrum]()
                {
                    if (spectrum->autoRangeY())
                    {
                        spectrum->setProgrammaticYRange(m_spectYViewMin, m_spectYViewMax);
                    }
                });
        connect(spectrum, &QObject::destroyed, this, [this, spectrum]() { m_spectrumPlotItems.removeAll(spectrum); });

        setFreqRange();
    }
}

void SignalBackend::unregisterSpectrumPlot(QQuickItem *item)
{
    m_spectrumPlotItems.removeAll(dynamic_cast<LineChartItem *>(item));
    if (m_spectrumPlotItems.isEmpty())
    {
        emit startSignalSpectrum(false);
    }
}

void SignalBackend::registerWaterfallPlot(QQuickItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    WaterfallItem *wf = dynamic_cast<WaterfallItem *>(item);
    if (wf && m_waterfallPlotItems.contains(wf) == false)
    {
        m_waterfallPlotItems.append(wf);
        wf = wf;
        wf->setDataRange((-1024 + m_frequency) * 0.001, (1024 + m_frequency) * 0.001);
        wf->setMarkerLeft((-768 + m_frequency) * 0.001);
        wf->setMarkerRight((768 + m_frequency) * 0.001);
        wf->setMarkerCenter(m_frequency * 0.001);
        connect(wf, &QObject::destroyed, this, [this, wf]() { m_waterfallPlotItems.removeAll(wf); });
    }
}

void SignalBackend::unregisterWaterfallPlot(QQuickItem *item)
{
    m_waterfallPlotItems.removeAll(dynamic_cast<WaterfallItem *>(item));
}

void SignalBackend::registerSnrPlot(QQuickItem *item)
{
    if (item == nullptr)
    {
        m_snrPlot = nullptr;
        return;
    }

    LineChartItem *snr = dynamic_cast<LineChartItem *>(item);
    if (snr && snr != m_snrPlot)
    {
        m_snrPlot = snr;
        // Single sine series using FilledWithStroke style
        // m_snrPlot->setXLabelDecimals(0);
        // m_snrPlot->setYLabelDecimals(0);
        m_snrPlot->setYMin(0);
        m_snrPlot->setYMax(36);
        m_snrPlot->setDefaultYMin(0);
        m_snrPlot->setDefaultYMax(36);
        m_snrPlot->setMajorTickStepY(5);
        m_snrPlot->setMinorSectionsY(5);
        m_snrPlot->setYLabelFormat("fixed");
        m_snrPlot->setYLabelDecimals(0);

        m_snrPlot->setXAxisTitle(tr("Time"));
        m_snrPlot->setYAxisTitle(tr("SNR [dB]"));

        m_snrPlot->setXMin(0);                    // start at 0
        m_snrPlot->setXMax(xPlotRange);           // show initial range
        m_snrPlot->setXLabelFormat("time_mmss");  // Format as mm:ss
        m_snrPlot->setMajorTickStepX(10);         // Tick every 10 seconds
        // m_snrPlot->setMaxXSpan(600);
        m_snrPlot->setHistoryCapacity(10 * xPlotRange);  // 10 minutes history

        // Enable followTail for live scrolling
        m_snrPlot->setFollowTail(true);

        m_snrPlot->setDecimationEnabled(false);

        m_snrSeriesId = m_snrPlot->addSeries("snr", Qt::cyan, 1.0);
        m_snrPlot->setSeriesStyle(m_snrSeriesId, (int)LineSeries::FilledWithStroke);
        m_snrPlot->setSeriesBaseline(m_snrSeriesId, 0.0);
        m_snrPlot->setSeriesBrush(m_snrSeriesId, QBrush(QColor(0, 255, 255, 100)));
        m_snrPlot->setSeriesPen(m_snrSeriesId, QPen(Qt::cyan, 1.0));

        // m_snrPlot->addMarkerLine(false, static_cast<double>(DabSnrThreshold::LowSNR), "LowSNR", Qt::white, 1.0);
        // m_snrPlot->addMarkerLine(false, static_cast<double>(DabSnrThreshold::GoodSNR), "GoodSNR", Qt::white, 1.0);

        m_startTimeMsec = 0;
        m_timer->start();
    }
}

void SignalBackend::setIsUndocked(bool isUndocked)
{
    if (m_isUndocked == isUndocked)
    {
        return;
    }
    m_isUndocked = isUndocked;
    emit isUndockedChanged();
}

void SignalBackend::setInputDevice(InputDevice::Id id)
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

    // ui->menuLabel->setEnabled(id != InputDevice::Id::UNDEFINED);

    // enable spectrum
    emit startSignalSpectrum(id == InputDevice::Id::UNDEFINED ? false : true);
}

void SignalBackend::setSignalState(uint8_t sync, float snr)
{
    snrValue(QString("%1 dB").arg(snr, 0, 'f', 1));
    addToPlot(snr);
    m_timer->start();
}

void SignalBackend::setFreqRange()
{
    for (auto spectrumPlot : std::as_const(m_spectrumPlotItems))
    {
        spectrumPlot->setDefaultXMin((-1024 + m_frequency) * 0.001);
        spectrumPlot->setDefaultXMax((1024 + m_frequency) * 0.001);
        spectrumPlot->setXMin((-1024 + m_frequency) * 0.001);
        spectrumPlot->setXMax((1024 + m_frequency) * 0.001);

        spectrumPlot->resetZoomX();
        spectrumPlot->resetToAutoRangeY();
        spectrumPlot->setMarkerLinePosition(m_spectLeftMarginId, (-768 + m_frequency) * 0.001);
        spectrumPlot->setMarkerLinePosition(m_spectRightMarginId, (768 + m_frequency) * 0.001);
        spectrumPlot->setMarkerLinePosition(m_spectCenterId, (0 + m_frequency) * 0.001);
    }
    for (auto waterfallItem : std::as_const(m_waterfallPlotItems))
    {
        waterfallItem->setDataRange((-1024 + m_frequency) * 0.001, (1024 + m_frequency) * 0.001);
        waterfallItem->setMarkerLeft((-768 + m_frequency) * 0.001);
        waterfallItem->setMarkerRight((768 + m_frequency) * 0.001);
        waterfallItem->setMarkerCenter(m_frequency * 0.001);
    }
}

void SignalBackend::reset()
{
    m_signalSpectrumBuffer.assign(2048, 0.0);
    m_nullSpectrumBuffer.assign(2048, 0.0);
    m_signalAvrgCntr = 0;
    m_spectYRangeMin = -140;
    m_spectYRangeMax = 0;
    m_spectYViewMin = m_spectYRangeMin;
    m_spectYViewMax = m_spectYRangeMax;
    for (auto spectrumPlot : std::as_const(m_spectrumPlotItems))
    {
        spectrumPlot->clear(m_signalSpectSeriesId);
        spectrumPlot->clear(m_nullSpectSeriesId);
        spectrumPlot->setYMin(m_spectYRangeMin);
        spectrumPlot->setYMax(m_spectYRangeMax);
    }
    m_isUserView = false;
    // ui->menuLabel->setEnabled(false);
    updateRfLevel(NAN, NAN);
    isRfLevelVisible(false);
    setGainVisible(false);
    setSignalState(0, 0.0);
    frequencyOffsetLabel(tr("N/A"));
}

void SignalBackend::setSpectrumUpdate()
{
    switch (m_settings->signal.spectrumUpdate)
    {
        case SpectrumUpdateSlow:  // 1.4 sec ==> 14
            m_numAvrg = 14;
            m_avrgFactor_dB = -11.461;  // 10 *log10(1/14)
            break;
        case SpectrumUpdateFast:  // 600 ms ==> 6
            m_numAvrg = 6;
            m_avrgFactor_dB = -7.7815;  // 10 *log10(1/6)
            break;
        case SpectrumUpdateVeryFast:  // 400 ms ==> 4
            m_numAvrg = 4;
            m_avrgFactor_dB = -6.0206;  // 10 *log10(1/4)
            break;
        default:  // 1000 ms ==> 10
            m_numAvrg = 10;
            m_avrgFactor_dB = -10;  // 10 *log10(1/10)
            break;
    }
}

void SignalBackend::setGainVisible(bool visible)
{
    if (visible == false)
    {
        gainLabel("");
    }
    for (auto spectrumPlot : std::as_const(m_spectrumPlotItems))
    {
        spectrumPlot->setYAxisTitle(visible ? "dBm" : "dBFS");
    }
}

void SignalBackend::onTuneDone(uint32_t freq)
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
        frequencyLabel(QString::number(m_frequency) + " kHz");
        // ui->menuLabel->setEnabled(true);
    }
    else
    {
        frequencyLabel("");
    }
}

void SignalBackend::updateRfLevel(float rfLevel, float gain)
{
    m_rfLevel = rfLevel;
    m_tunerGain = gain;
    if (std::isnan(gain))
    {
        setGainVisible(false);
    }
    else
    {
        gainLabel(QString::number(static_cast<int>(gain)) + " dB");
        setGainVisible(true);
    }

    if (std::isnan(rfLevel))
    {  // level is not available (input device in HW mode or not RTL-SDR)
        rfLevelLabel("");
        isRfLevelVisible(false);
        m_spectYRangeMin = -140;
        m_spectYRangeMax = 0;
    }
    else
    {
        rfLevelLabel(QString::number(double(rfLevel), 'f', 1) + " dBm");
        isRfLevelVisible(true);
        m_spectYRangeMin = floorf((-100 + rfLevel) / 10.0) * 10.0;
        m_spectYRangeMax = ceilf((0 + rfLevel) / 10.0) * 10.0;
    }
}

void SignalBackend::updateFreqOffset(float offset)
{
    frequencyOffsetLabel(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void SignalBackend::onSignalSpectrum(std::shared_ptr<std::vector<float>> data, int type)
{
    if (type == RadioControl::SPECT_SIGNAL)
    {
        int idx = 0;
        for (auto it = m_signalSpectrumBuffer.begin(); it != m_signalSpectrumBuffer.end(); ++it)
        {
            *it += data->at(idx++);
        }

        if (++m_signalAvrgCntr >= m_numAvrg)
        {
            // qDebug() << *std::min_element(m_spectrumBuffer.cbegin(), m_spectrumBuffer.cend())*0.1 <<
            // *std::max_element(m_spectrumBuffer.cbegin(), m_spectrumBuffer.cend())*0.1; qDebug() << Q_FUNC_INFO;
            m_signalAvrgCntr = 0;
            float offset_dB = m_offset_dB + m_avrgFactor_dB;
            if (!std::isnan(m_tunerGain))
            {
                offset_dB -= m_tunerGain;
            }

            QVector<QPointF> bins;
            bins.reserve(2048);
            bins.resize(2048);
            auto binsIt = bins.begin() + 1024;
            float freq = 0;
            float minVal = 1000;
            float maxVal = -1000;
            for (auto it = m_signalSpectrumBuffer.begin(); it != m_signalSpectrumBuffer.end(); ++it)
            {
                float val = 10 * std::log10(*it) + offset_dB;
                if (val < -200)
                {
                    val = -200;
                }
                minVal = (val < minVal) ? val : minVal;
                maxVal = (val > maxVal) ? val : maxVal;

                // bins.append(QPointF((freq + m_frequency) * 0.001, val));
                *binsIt++ = QPointF((freq + m_frequency) * 0.001, val);
                freq += 1.0;

                if (freq == 1024.0)
                {
                    freq = -1024.0;
                    binsIt = bins.begin();
                }
                *it = 0.0;
            }

            if (m_spectrumPlotItems.size() > 0)
            {
                auto spectrumPlot = m_spectrumPlotItems.first();
                if ((minVal - spectrumPlot->yMin()) < 0 || (minVal - spectrumPlot->yMin()) > 20)
                {  // set minumum to at least minVal + 10, use multiples of 10
                    m_spectYViewMin = std::fmaxf(ceilf((minVal - 10) / 10.0) * 10.0, m_spectYRangeMin);
                }
                if ((spectrumPlot->yMax() - maxVal) < 0 || (spectrumPlot->yMax() - maxVal) > 20)
                {
                    m_spectYViewMax = std::fminf(floorf((maxVal + 10) / 10.0) * 10.0, m_spectYRangeMax);
                }
                for (auto spectrumPlot : std::as_const(m_spectrumPlotItems))
                {
                    spectrumPlot->replaceBuffer(m_signalSpectSeriesId, bins);
                    if (spectrumPlot->userModifiedY() == false)
                    {
                        spectrumPlot->setProgrammaticYRange(m_spectYViewMin, m_spectYViewMax);
                    }

                    if (m_settings->signal.showNULL && m_nullSpectrumUpdated == false)
                    {  // no sync clear NULL spectrum
                        spectrumPlot->clear(m_nullSpectSeriesId);
                        m_nullSpectrumBuffer.assign(2048, 0.0);
                    }
                }
                m_nullSpectrumUpdated = false;
            }

            for (auto waterfallItem : std::as_const(m_waterfallPlotItems))
            {
                waterfallItem->addRow(bins);
            }
        }
    }
    else if (m_settings->signal.showNULL)
    {
        int idx = 0;
        for (auto it = m_nullSpectrumBuffer.begin(); it != m_nullSpectrumBuffer.end(); ++it)
        {
            *it += data->at(idx++);
        }

        if (m_signalAvrgCntr == m_numAvrg - 1)
        {
            float offset_dB = m_offset_dB + m_avrgFactor_dB;
            if (!std::isnan(m_tunerGain))
            {
                offset_dB -= m_tunerGain;
            }

#if 1
            // only -768 ...768 bins are used for display
            QVector<QPointF> bins;
            bins.reserve(1536 + 1);
            bins.resize(1536 + 1);
            auto binsIt = bins.begin() + 768;
            float freq = 0;
            auto it = m_nullSpectrumBuffer.begin();
            while (it != m_nullSpectrumBuffer.end())
            {
                float val = 10 * std::log10(*it) + offset_dB;
                if (val < -200)
                {
                    val = -200;
                }

                // bins.append(QPointF((freq + m_frequency) * 0.001, val));
                *binsIt++ = QPointF((freq + m_frequency) * 0.001, val);
                freq += 1.0;

                if (freq > 768.0)
                {
                    freq = -768.0;
                    binsIt = bins.begin();

                    *it = 0;
                    it += (2048 - 1536 - 1);  // skip upper part of the spectrum
                }
                *it++ = 0.0;
                if (freq == 0)
                {
                    break;
                }
            }
#else
            QVector<QPointF> bins;
            bins.reserve(2048);
            bins.resize(2048);
            auto binsIt = bins.begin() + 1024;
            float freq = 0;
            for (auto it = m_nullSpectrumBuffer.begin(); it != m_nullSpectrumBuffer.end(); ++it)
            {
                float val = 10 * std::log10(*it) + offset_dB;
                if (val < -200)
                {
                    val = -200;
                }

                // bins.append(QPointF((freq + m_frequency) * 0.001, val));
                *binsIt++ = QPointF((freq + m_frequency) * 0.001, val);
                freq += 1.0;

                if (freq == 1024.0)
                {
                    freq = -1024.0;
                    binsIt = bins.begin();
                }
                *it = 0.0;
            }
#endif

            for (auto spectrumPlot : std::as_const(m_spectrumPlotItems))
            {
                spectrumPlot->replaceBuffer(m_nullSpectSeriesId, bins);
            }
            m_nullSpectrumUpdated = true;
        }
    }
}

void SignalBackend::addToPlot(float snr)
{
    if (m_snrPlot == nullptr)
    {
        return;
    }
    double key = 0.0;
    if (m_startTimeMsec == 0)
    {
        m_startTimeMsec = QDateTime::currentMSecsSinceEpoch();
    }
    else
    {
        key = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMsec) * 0.001;  // convert to seconds
    }

    m_snrPlot->appendPoints(m_snrSeriesId, {QPointF(key, snr)});
}
