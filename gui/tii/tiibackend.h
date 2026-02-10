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

#ifndef TIIBACKEND_H
#define TIIBACKEND_H

#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QQmlApplicationEngine>

#include "radiocontrol.h"
#include "settings.h"
#include "txmapbackend.h"

#define TIIBACKEND_PLOT 0

class LineChartItem;
class TxDataItem;

class TIIBackend : public TxMapBackend
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TIIBackend cannot be instantiated")
    UI_PROPERTY_SETTINGS(QVariant, splitterState, m_settings->tii.splitterState)
public:
    explicit TIIBackend(Settings *settings, QObject *parent = nullptr);
    ~TIIBackend();
    void onTiiData(const RadioControlTIIData &data) override;
    void onSignalState(uint8_t, float snr) { m_snr = snr; };
    void onChannelSelection();
    void onEnsembleInformation(const RadioControlEnsemble &ens) override;
    void loadSettings();
    void onSettingsChanged() override;
    Q_INVOKABLE void startStopLog() override;
    Q_INVOKABLE void registerTiiSpectrumPlot(QQuickItem *item);
    void setIsActive(bool isActive);

protected:
    void onSelectedRowChanged() override;

private:
    enum GraphRange
    {
        MinX = 0,
        MaxX = 192,
        MinY = 0,
        MaxY = 1
    };

    // UI
    QTimer *m_inactiveCleanupTimer = nullptr;
    QFile *m_logFile = nullptr;
    float m_snr = 0.0;

    bool m_exportCoordinates = false;
    TxTableModel::TxTableModelRoles m_exportRole = TxTableModel::TxTableModelRoles::ExportRole;

    void logTiiData() const;
    void addToPlot(const RadioControlTIIData &data);
    void updateTiiPlot();

    LineChartItem *m_tiiSpectrumPlot = nullptr;
    int m_sSpect = -1;
    int m_sTii = -1;
};

#endif  // TIIBACKEND_H
