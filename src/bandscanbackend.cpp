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

#include "bandscanbackend.h"

#include <QDebug>

#include "dabtables.h"

BandScanBackend::BandScanBackend(bool autoStart, Settings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    ensemblesFound(QString("%1").arg(m_numEnsemblesFound));
    servicesFound(QString("%1").arg(m_numServicesFound));
    if (m_settings->cableChannelsEna)
    {
        m_numChannels = DabTables::channelList.size();
    }
    else
    {
        m_numChannels = 0;
        for (auto it = DabTables::channelList.constBegin(); it != DabTables::channelList.constEnd(); ++it)
        {
            if (it.key() <= 239200)
            {
                m_numChannels += 1;
            }
        }
    }
    channelProgress(QString("0 / %1").arg(m_numChannels));
    if (autoStart)
    {
        isScanning(true);
        currentChannel(tr(""));
        QTimer::singleShot(1, this, [this]() { startScan(); });
    }
    else
    {
        currentChannel(tr("Press Start to perform band scan."));
    }
}

BandScanBackend::~BandScanBackend()
{
    if (nullptr != m_timer)
    {
        m_timer->stop();
        delete m_timer;
    }
}

void BandScanBackend::stopScan()
{
    // the state machine has 4 possible states
    // 1. wait for tune (event)
    // 2. wait for sync (timer or event)
    // 4. wait for ensemble (timer or event)
    // 5. wait for services (timer)
    if (m_timer->isActive())
    {  // state 2, 3, 4
        m_timer->stop();
        emit done(BandScanBackendResult::Interrupted);
    }
    // timer not running -> state 1
    m_state = BandScanState::Interrupted;  // ==> it will be finished when tune is complete
}

void BandScanBackend::cancelScan()
{
    emit done(BandScanBackendResult::Cancelled);
}

void BandScanBackend::startScan()
{
    isScanning(true);
    currentChannel(tr(""));

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BandScanBackend::scanStep);

    m_state = BandScanState::Init;

    // using timer for mainwindow to cleanup and tune to 0 potentially (no timeout in case)
#ifdef Q_OS_WIN
    m_timer->start(6000);
#else
    m_timer->start(2000);
#endif
    emit scanStarts();
}

void BandScanBackend::scanStep()
{
    if (BandScanState::Init == m_state)
    {  // first step
        m_channelIt = DabTables::channelList.constBegin();
    }
    else
    {  // next step
        ++m_channelIt;
    }
    m_channelCounter += 1;

    if (m_channelCounter >= m_numChannels)
    {
        // scan finished
        m_state = BandScanState::Idle;
        emit done(BandScanBackendResult::Done);
        return;
    }

    progress(m_channelCounter * 100.0 / m_numChannels);
    channelProgress(QString("%1 / %2").arg(m_channelCounter).arg(m_numChannels));
    currentChannel(tr("Scanning channel:") + " " + m_channelIt.value());
    m_state = BandScanState::WaitForTune;
    emit tuneChannel(m_channelIt.key());
}

void BandScanBackend::onTuneDone(uint32_t freq)
{
    if (BandScanState::Idle == m_state)
    {  // no action in idle
        return;
    }

    if (BandScanState::Init == m_state)
    {
        if (m_timer->isActive())
        {
            m_timer->stop();
        }
        scanStep();
    }
    else if (BandScanState::Interrupted == m_state)
    {  // exit
        m_state = BandScanState::Idle;
        done(BandScanBackendResult::Interrupted);
    }
    else
    {  // tuned to some frequency -> wait for sync
        m_state = BandScanState::WaitForSync;
        m_timer->start(3000);
    }
}

void BandScanBackend::onSyncStatus(uint8_t sync, float)
{
    if (BandScanState::Idle == m_state)
    {  // do nothing
        return;
    }

    if (DabSyncLevel::NullSync <= DabSyncLevel(sync))
    {
        if (BandScanState::WaitForSync == m_state)
        {  // if we are waiting for sync (move to next step)
            m_timer->stop();
            m_state = BandScanState::WaitForEnsemble;
            m_timer->start(6000);
        }
    }
}

void BandScanBackend::onEnsembleFound(const RadioControlEnsemble &)
{
    if (BandScanState::Idle == m_state)
    {  // do nothing
        return;
    }

    m_timer->stop();
    ensemblesFound(QString("%1").arg(++m_numEnsemblesFound));

    m_state = BandScanState::WaitForServices;

    // this can be interrupted by ensemble complete signal (ensembleConfiguration)
    m_timer->start(8000);
}

void BandScanBackend::onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &)
{
    if (isScanning())
    {
        servicesFound(QString("%1").arg(++m_numServicesFound));
    }
}

void BandScanBackend::onServiceListComplete(const RadioControlEnsemble &)
{  // this means that ensemble information is complete => stop timer and do next set
    if (BandScanState::Idle == m_state)
    {  // no action in idle
        return;
    }

    if ((nullptr != m_timer) && (m_timer->isActive()))
    {
        m_timer->stop();
        scanStep();
    }
}
