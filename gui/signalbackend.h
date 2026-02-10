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

#ifndef SIGNALBACKEND_H
#define SIGNALBACKEND_H

#include <QDateTime>
#include <QQuickItem>
#include <QTimer>

#include "settings.h"
#include "uicontrolprovider.h"

class LineChartItem;
class SignalBackend : public UIControlProvider
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SignalBackend cannot be instantiated")
    Q_PROPERTY(bool isUndocked READ isUndocked NOTIFY isUndockedChanged FINAL)
    UI_PROPERTY(QString, frequencyLabel)
    UI_PROPERTY(QString, frequencyOffsetLabel)
    UI_PROPERTY(QString, rfLevelLabel)
    UI_PROPERTY(QString, gainLabel)
    UI_PROPERTY(QString, snrValue)
    UI_PROPERTY_SETTINGS(int, spectrumMode, m_settings->signal.spectrumMode)
    UI_PROPERTY_SETTINGS(int, spectrumUpdate, m_settings->signal.spectrumUpdate)
    UI_PROPERTY_SETTINGS(bool, showSNR, m_settings->signal.showSNR)
    UI_PROPERTY_SETTINGS(QVariant, splitterState, m_settings->signal.splitterState)

public:
    explicit SignalBackend(Settings *settings, int freq, QObject *parent = nullptr);
    ~SignalBackend();
    Q_INVOKABLE void registerSpectrumPlot(QQuickItem *item);
    Q_INVOKABLE void registerSnrPlot(QQuickItem *item);

    void setIsActive(bool isActive);
    void setIsUndocked(bool isUndocked);
    bool isUndocked() const { return m_isUndocked; }
    void setInputDevice(InputDevice::Id id);
    void setSignalState(uint8_t sync, float snr);
    void onTuneDone(uint32_t freq);
    void updateRfLevel(float rfLevel, float gain);
    void updateFreqOffset(float offset);
    void onSignalSpectrum(std::shared_ptr<std::vector<float>> data);

signals:
    void setSignalSpectrum(int mode);
    void isUndockedChanged();

protected:
    // void closeEvent(QCloseEvent *event) override;

public:
    enum SpectrumUpdate
    {
        SpectrumUpdateSlow = 0,
        SpectrumUpdateNormal = 1,
        SpectrumUpdateFast = 2,
        SpectrumUpdateVeryFast = 3,
    };
    Q_ENUM(SpectrumUpdate)

private:
    enum
    {
        xPlotRange = 60  // 60 seconds
    };

    Settings *m_settings = nullptr;
    QTime m_startTime;
    QTimer *m_timer = nullptr;

    bool m_isUndocked = false;  // this flag holds the information if the page is undocked

    LineChartItem *m_spectrumPlot = nullptr;
    LineChartItem *m_snrPlot = nullptr;
    int m_spectSeriesId = -1;
    int m_spectLeftMarginId = -1;
    int m_spectRightMarginId = -1;
    int m_spectCenterId = -1;
    int m_snrSeriesId = -1;

    std::vector<float> m_spectrumBuffer;
    int m_frequency = 0;
    float m_rfLevel = NAN;
    float m_tunerGain = NAN;
    float m_offset_dB = 0.0;
    int m_syncSnrLevel = -1;
    float m_spectYRangeMax;
    float m_spectYRangeMin;
    float m_spectYViewMax;
    float m_spectYViewMin;
    bool m_isUserView;
    int m_avrgCntr = 0;
    int m_numAvrg = 10;
    float m_avrgFactor_dB = -10.0;

    void addToPlot(float snr);
    void setFreqRange();
    void reset();

    void setSpectrumUpdate();

    void setRfLevelVisible(bool visible);
    void setGainVisible(bool visible);
};

#endif  // SIGNALBACKEND_H
