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

#ifndef BANDSCANBACKEND_H
#define BANDSCANBACKEND_H

#include "radiocontrol.h"
#include "servicelistid.h"
#include "settings.h"
#include "uicontrolprovider.h"

namespace BandScanBackendResult
{
enum
{
    Cancelled = 0,
    Interrupted = 1,
    Done = 2
};
}

enum class BandScanState
{
    Idle = 0,
    Init,
    WaitForTune,
    WaitForSync,
    WaitForEnsemble,
    WaitForServices,
    Interrupted
};

class BandScanBackend : public QObject
{
    Q_OBJECT
    UI_PROPERTY_DEFAULT(bool, isScanning, false)
    UI_PROPERTY(QString, ensemblesFound)
    UI_PROPERTY(QString, servicesFound)
    UI_PROPERTY(QString, currentChannel)
    UI_PROPERTY(QString, channelProgress)
    UI_PROPERTY_DEFAULT(float, progress, 0.0)
    UI_PROPERTY_SETTINGS(bool, keepServiceListOnScan, m_settings->keepServiceListOnScan)

public:
    explicit BandScanBackend(bool autoStart, Settings *settings, QObject *parent = nullptr);
    ~BandScanBackend();

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE void cancelScan();

    void onTuneDone(uint32_t freq);
    void onSyncStatus(uint8_t sync, float);
    void onEnsembleFound(const RadioControlEnsemble &ens);
    void onServiceListComplete(const RadioControlEnsemble &);
    void onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &);
signals:
    void done(int result);
    void scanStarts();
    void tuneChannel(uint32_t freq);

private:
    QTimer *m_timer = nullptr;
    Settings *m_settings = nullptr;

    BandScanState m_state = BandScanState::Idle;

    int m_channelCounter = 0;
    int m_numEnsemblesFound = 0;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;

    void scanStep();
};

#endif  // BANDSCANBACKEND_H
