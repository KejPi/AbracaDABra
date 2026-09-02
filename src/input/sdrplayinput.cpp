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

#include "sdrplayinput.h"

#include <QDebug>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(sdrPlayInput, "SDRPlayInput", QtInfoMsg)

InputDeviceList SdrPlayInput::getDeviceList()
{
    InputDeviceList list;

    SoapySDR::KwargsList devs = SoapySDR::Device::enumerate("driver=sdrplay");
    if (devs.size() == 0)
    {
        qCInfo(sdrPlayInput) << "No devices found";
        return list;
    }
    else
    {
        qCInfo(sdrPlayInput) << "Found" << devs.size() << "devices.";
    }
    for (int n = 0; n < devs.size(); ++n)
    {
        auto it = devs[n].find(std::string("label"));
        if (it != devs[n].end())
        {
            list.append({.diplayName = QString::fromStdString(it->second), .id = QVariant(QString::fromStdString(devs[n]["serial"]))});
            // list.append({.diplayName = "Test1", .id = QVariant(QString::fromStdString(devs[n]["serial"]))});
            // list.append({.diplayName = "Test2", .id = QVariant(QString::fromStdString(devs[n]["serial"]))});
        }
    }
    return list;
}

int SdrPlayInput::getNumRxChannels(const QVariant &hwId)
{
    size_t numChannels = 0;
    try
    {
        QString argStr = QString("driver=sdrplay,serial=%1").arg(hwId.toString());
        auto device = SoapySDR::Device::make(argStr.toStdString());
        numChannels = device->getNumChannels(SOAPY_SDR_RX);
        SoapySDR::Device::unmake(device);
    }
    catch (const std::exception &ex)
    {
        qCCritical(sdrPlayInput) << "Error probing device: " << ex.what();
    }

    // qDebug() << "-----------" << hwId << numChannels;

    return numChannels;
}

QStringList SdrPlayInput::getRxAntennas(const QVariant &hwId, const int channel)
{
    QStringList antList;
    try
    {
        QString argStr = QString("driver=sdrplay,serial=%1").arg(hwId.toString());
        auto device = SoapySDR::Device::make(argStr.toStdString());
        auto ant = device->listAntennas(SOAPY_SDR_RX, channel);
        for (auto it = ant.cbegin(); it != ant.cend(); ++it)
        {
            antList.append(QString::fromStdString(*it));
        }
        SoapySDR::Device::unmake(device);
    }
    catch (const std::exception &ex)
    {
        qCWarning(sdrPlayInput) << "Error probing device: " << ex.what();
    }
    return antList;
}

SdrPlayInput::SdrPlayInput(QObject *parent)
    : SoapySdrInput(parent),
      m_rfGainMap{
          {"RSP1", {-43, -19, -24, 0}},
          {"RSP1A", {-62, -57, -38, -32, -26, -20, -18, -12, -6, 0}},
          {"RSP1B", {-62, -57, -38, -32, -26, -20, -18, -12, -6, 0}},
          {"RSP2", {-64, -45, -39, -34, -24, -21, -15, -10, 0}},
          {"RSPduo", {-62, -57, -38, -32, -26, -20, -18, -12, -6, 0}},
          {"RSPdx",
           {-84, -81, -78, -75, -72, -69, -66, -63, -60, -57, -54, -51, -48, -45, -42, -39, -36, -33, -30, -27, -24, -18, -15, -12, -9, -6, -3, 0}},
          {"RSPdx-R2",
           {-84, -81, -78, -75, -72, -69, -66, -63, -60, -57, -54, -51, -48, -45, -42, -39, -36, -33, -30, -27, -24, -18, -15, -12, -9, -6, -3, 0}},
      }
{
    m_deviceDescription.id = InputDevice::Id::SDRPLAY;
    m_devArgs = "driver=sdrplay,rfnotch_ctrl=true,dabnotch_ctrl=false";
    m_biasT = false;
}

bool SdrPlayInput::openDevice(const QVariant &hwId, bool fallbackConnection)
{
    // find all SDRplay devices
    const auto list = SdrPlayInput::getDeviceList();

    bool foundDevice = false;
    if (hwId.isValid() && !hwId.toString().isEmpty())
    {
        // first check if device with given SN is available
        for (auto it = list.cbegin(); it != list.cend(); ++it)
        {
            if (it->id.toString() == hwId.toString())
            {
                foundDevice = true;
                break;
            }
        }
        if (foundDevice == false && fallbackConnection == false)
        {
            qCCritical(sdrPlayInput, "Selected SDRplay device SN %s not found", hwId.toString().toLatin1().data());
            return false;
        }
    }

    bool isConnected = false;
    if (foundDevice)
    {
        SoapySdrInput::setDevArgs(QString("driver=sdrplay,serial=%1,rfnotch_ctrl=true,dabnotch_ctrl=false").arg(hwId.toString()));
        isConnected = SoapySdrInput::openDevice(hwId);
        if (isConnected)
        {
            m_deviceDescription.device.sn = hwId.toString();
            m_hwId = hwId;
        }
        else if (fallbackConnection == false)
        {
            qCCritical(sdrPlayInput, "Unable to open selected SDRplay device: SN %s", hwId.toString().toLatin1().data());
            return false;
        }
    }

    if (!isConnected)
    {  // either SN was not found or connection not successful
        // go through device list and try to connect to first working device
        for (auto it = list.cbegin(); it != list.cend(); ++it)
        {
            if (it->id.isValid() && !it->id.toString().isEmpty())
            {
                SoapySdrInput::setDevArgs(QString("driver=sdrplay,serial=%1,rfnotch_ctrl=true,dabnotch_ctrl=false").arg(it->id.toString()));
                isConnected = SoapySdrInput::openDevice(hwId);
                if (isConnected)
                {
                    m_deviceDescription.device.sn = it->id.toString();
                    m_hwId = it->id;
                    break;
                }
            }
        }
    }

    if (isConnected)
    {
#if SOAPYSDR_TIMING_DEBUG
        qDebug() << "setGainMode (false)" << m_elapsedTimer.restart();
#endif
        m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, false);
#if SOAPYSDR_TIMING_DEBUG
        qDebug() << "---> setGainMode (false)" << m_elapsedTimer.restart();
#endif
        if (m_rfGainMap.contains(m_deviceDescription.device.model))
        {
            m_rfGainList = m_rfGainMap.value(m_deviceDescription.device.model);
            return true;
        }
    }
    return false;
}

SdrPlayWorker::FeedbackMode SdrPlayInput::currentFeedbackMode() const
{
    if (SdrPlayGainMode::Software == m_gainMode)
    {
        return SdrPlayWorker::FeedbackMode::SoftwareFull;
    }
    return m_ifAgcEna ? SdrPlayWorker::FeedbackMode::IfOnly : SdrPlayWorker::FeedbackMode::Off;
}

SoapySdrWorker *SdrPlayInput::createWorker(SoapySDR::Device *device, double sampleRate, int rxChannel, QObject *parent)
{
    auto *worker = new SdrPlayWorker(device, sampleRate, rxChannel, m_rfGainList, parent);

    // seed initial feedback state (handles the very first tune() -> resetAgc()
    // call that happens before this worker exists); safe here because the
    // worker thread has not been started yet
    worker->syncManualRfGain(m_rfGR);
    worker->syncManualIfGain(m_ifGR);
    worker->setFeedbackMode(currentFeedbackMode());

    return worker;
}

void SdrPlayInput::connectWorkerSignals(SoapySdrWorker *worker)
{
    auto *sdrPlayWorker = static_cast<SdrPlayWorker *>(worker);
    connect(sdrPlayWorker, &SdrPlayWorker::agcGainChanged, this, &SdrPlayInput::agcGain, Qt::QueuedConnection);
    connect(sdrPlayWorker, &SdrPlayWorker::rfLevelChanged, this, &SdrPlayInput::rfLevel, Qt::QueuedConnection);
    connect(sdrPlayWorker, &SdrPlayWorker::ifGainChanged, this, &SdrPlayInput::ifGain, Qt::QueuedConnection);
    connect(sdrPlayWorker, &SdrPlayWorker::gainIdxChanged, this, &SdrPlayInput::gainIdx, Qt::QueuedConnection);
}

void SdrPlayInput::setGainMode(const SdrPlayGainStruct &gain)
{
    switch (gain.mode)
    {
        case SdrPlayGainMode::Software:
            if (m_gainMode == gain.mode)
            {  // do nothing -> mode does not change
                break;
            }
            m_gainMode = gain.mode;
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "setGainMode (false)" << m_elapsedTimer.restart();
#endif
            m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, false);
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "---> setGainMode (false)" << m_elapsedTimer.restart();
#endif
            if (m_worker)
            {
                static_cast<SdrPlayWorker *>(m_worker)->setFeedbackMode(currentFeedbackMode());
            }
            break;
        case SdrPlayGainMode::Manual:
            m_gainMode = gain.mode;
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "setGainMode (false)" << m_elapsedTimer.restart();
#endif
            m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, false);
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "---> setGainMode (false)" << m_elapsedTimer.restart();
#endif
            setRFGR(m_rfGainList.size() - 1 - gain.rfGain);
            m_ifAgcEna = gain.ifAgcEna;
            if (m_worker)
            {
                static_cast<SdrPlayWorker *>(m_worker)->setFeedbackMode(currentFeedbackMode());
            }
            if (!m_ifAgcEna)
            {
                setIFGR(-gain.ifGain);
            }
            emit agcGain(getRFGain() - m_ifGR);
            break;
    }
    emit rfLevel(NAN, NAN);
}

void SdrPlayInput::setBiasT(bool ena)
{
    if (ena != m_biasT)
    {
        m_device->writeSetting("biasT_ctrl", ena ? "true" : "false");
        m_biasT = ena;
        qCInfo(sdrPlayInput) << "Bias-T" << (ena ? "on" : "off");
    }
}

void SdrPlayInput::setAntenna(const QString &antenna)
{
    SoapySdrInput::setAntenna(antenna);
    if (m_device)
    {
        try
        {
            m_device->setAntenna(SOAPY_SDR_RX, m_rxChannel, m_antenna.toStdString());
            qCInfo(sdrPlayInput) << "Antenna:" << m_device->getAntenna(SOAPY_SDR_RX, m_rxChannel);
        }
        catch (const std::exception &ex)
        {
            qCWarning(sdrPlayInput) << "Failed to set antenna to" << m_antenna << ex.what();
            return;
        }
    }
}

void SdrPlayInput::resetAgc()
{
    // actual SW-AGC / IF-AGC reset math now runs on the worker thread (see
    // SdrPlayWorker::doReset()) so it never blocks the GUI thread
    if (m_worker)
    {
        static_cast<SdrPlayWorker *>(m_worker)->requestAgcReset();
    }
    emit rfLevel(NAN, NAN);
}

void SdrPlayInput::setRFGR(int rfGR)
{
    if (rfGR < 0)
    {
        rfGR = 0;
    }
    if (rfGR >= m_rfGainList.count())
    {
        rfGR = m_rfGainList.count() - 1;
    }
    if (rfGR != m_rfGR)
    {
        m_rfGR = rfGR;
        try
        {
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << Q_FUNC_INFO << "setGain RFGR =" << m_rfGR << m_elapsedTimer.restart();
#endif
            m_device->setGain(SOAPY_SDR_RX, m_rxChannel, "RFGR", m_rfGR);
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "---> setGain RFGR =" << m_rfGR << m_elapsedTimer.restart();
#endif
            qCDebug(sdrPlayInput) << "RF gain =" << getRFGain();
            emit gainIdx(m_rfGainList.size() - 1 - m_rfGR);
            if (m_worker)
            {
                static_cast<SdrPlayWorker *>(m_worker)->syncManualRfGain(m_rfGR);
            }
        }
        catch (const std::exception &ex)
        {
            qCWarning(sdrPlayInput) << "Failed to set RFGR to" << m_rfGR << ex.what();
            return;
        }
    }
}

void SdrPlayInput::setIFGR(int ifGR)
{
    if (ifGR < m_ifGRmin)
    {
        ifGR = m_ifGRmin;
    }
    if (ifGR > m_ifGRmax)
    {
        ifGR = m_ifGRmax;
    }
    if (ifGR != m_ifGR)
    {
        m_ifGR = ifGR;
        try
        {
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << Q_FUNC_INFO << "setGain IFGR =" << m_ifGR << m_elapsedTimer.restart();
#endif
            m_device->setGain(SOAPY_SDR_RX, m_rxChannel, "IFGR", m_ifGR);
#if SOAPYSDR_TIMING_DEBUG
            qDebug() << "---> setGain IFGR =" << m_ifGR << m_elapsedTimer.restart();
#endif
            qCDebug(sdrPlayInput) << "IF gain =" << -m_ifGR;
            emit ifGain(-m_ifGR);
            if (m_worker)
            {
                static_cast<SdrPlayWorker *>(m_worker)->syncManualIfGain(m_ifGR);
            }
        }
        catch (const std::exception &ex)
        {
            qCWarning(sdrPlayInput) << "Failed to set IFGR to" << m_ifGR << ex.what();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// SdrPlayWorker - owns the SW-AGC / IF-AGC feedback loop on the worker thread
// ---------------------------------------------------------------------------

SdrPlayWorker::SdrPlayWorker(SoapySDR::Device *device, double sampleRate, int rxChannel, const QList<float> &rfGainList,
                             QObject *parent)
    : SoapySdrWorker(device, sampleRate, rxChannel, parent), m_rfGainList(rfGainList)
{
}

void SdrPlayWorker::setFeedbackMode(FeedbackMode mode)
{
    m_feedbackMode = mode;
    if (FeedbackMode::Off != mode)
    {
        // (re)entering a feedback mode always starts from a well-defined
        // state, matching the original resetAgc() semantics
        m_resetRequested = true;
    }
}

void SdrPlayWorker::requestAgcReset()
{
    m_resetRequested = true;
}

void SdrPlayWorker::syncManualRfGain(int rfGR)
{
    m_rfGR = rfGR;
}

void SdrPlayWorker::syncManualIfGain(int ifGR)
{
    m_ifGR = ifGR;
}

void SdrPlayWorker::doReset(FeedbackMode mode)
{
    if (FeedbackMode::SoftwareFull == mode)
    {
        int idx = 0;
        do
        {
            if (m_rfGainList.at(idx) > -40.0)
            {
                break;
            }
        } while (++idx > 0);

        m_rfGR = -1;  // this is to force RF gain settings
        m_ifGR = -1;  // this is to force IF gain settings

        setRFGR(m_rfGainList.size() - 1 - idx);
        setIFGR(40);
        m_agcState = SwAgcState::Converging;
        m_rfGRchangeCntr = 2;
        emit agcGainChanged(getRFGain() - m_ifGR.load());
    }
    else if (FeedbackMode::IfOnly == mode)
    {
        setIFGR(40);
    }
    m_levelEmitCntr = 0;
}

void SdrPlayWorker::setRFGR(int rfGR)
{
    if (rfGR < 0)
    {
        rfGR = 0;
    }
    if (rfGR >= m_rfGainList.count())
    {
        rfGR = m_rfGainList.count() - 1;
    }
    if (rfGR != m_rfGR.load())
    {
        m_rfGR = rfGR;
        try
        {
            m_device->setGain(SOAPY_SDR_RX, m_rxChannel, "RFGR", rfGR);
            emit gainIdxChanged(m_rfGainList.size() - 1 - rfGR);
        }
        catch (const std::exception &ex)
        {
            qCWarning(sdrPlayInput) << "Failed to set RFGR to" << rfGR << ex.what();
            return;
        }
    }
}

void SdrPlayWorker::setIFGR(int ifGR)
{
    if (ifGR < m_ifGRmin)
    {
        ifGR = m_ifGRmin;
    }
    if (ifGR > m_ifGRmax)
    {
        ifGR = m_ifGRmax;
    }
    if (ifGR != m_ifGR.load())
    {
        m_ifGR = ifGR;
        try
        {
            m_device->setGain(SOAPY_SDR_RX, m_rxChannel, "IFGR", ifGR);
            emit ifGainChanged(-ifGR);
        }
        catch (const std::exception &ex)
        {
            qCWarning(sdrPlayInput) << "Failed to set IFGR to" << ifGR << ex.what();
            return;
        }
    }
}

void SdrPlayWorker::onSignalLevel(float agcLevel)
{
    if (m_resetRequested.exchange(false))
    {
        doReset(m_feedbackMode.load());
    }

    m_rfGRchangeCntr = (m_rfGRchangeCntr > 0 ? m_rfGRchangeCntr - 1 : 0);

    const FeedbackMode mode = m_feedbackMode.load();
    if (FeedbackMode::SoftwareFull == mode)
    {
        if (m_agcState == SwAgcState::Running)
        {
            if (agcLevel > SDRPLAY_LEVEL_THR_MAX)
            {  // decrease gain
                if (qMin(m_ifGR.load() + 1, m_ifGRmax) > SDRPLAY_RFGR_UP_THR && m_rfGRchangeCntr <= 0)
                {
                    m_rfGRchangeCntr = 2;
                    float gain = getRFGain();
                    setRFGR(m_rfGR.load() + 1);
                    setIFGR(m_ifGR.load() - (gain - getRFGain()));
                }
                else
                {
                    setIFGR(m_ifGR.load() + 1);
                }
            }
            else if (agcLevel < SDRPLAY_LEVEL_THR_MIN)
            {
                if (qMax(m_ifGR.load() + 1, m_ifGRmin) < SDRPLAY_RFGR_DOWN_THR && (m_rfGR.load() > 0) && (m_rfGRchangeCntr <= 0))
                {
                    m_rfGRchangeCntr = 2;
                    float gain = getRFGain();
                    setRFGR(m_rfGR.load() - 1);
                    setIFGR(m_ifGR.load() + (gain - getRFGain()));
                }
                else
                {
                    setIFGR(m_ifGR.load() - 1);
                }
            }
            else if (m_rfGRchangeCntr <= 0)
            {  // maintenance
                if (m_ifGR.load() >= SDRPLAY_RFGR_UP_THR)
                {
                    m_rfGRchangeCntr = 4;
                    float gain = getRFGain();
                    setRFGR(m_rfGR.load() + 1);
                    setIFGR(m_ifGR.load() - (gain - getRFGain()));
                }
                else if ((m_ifGR.load() < SDRPLAY_RFGR_DOWN_THR) && (m_rfGR.load() > 0))
                {
                    m_rfGRchangeCntr = 4;
                    float gain = getRFGain();
                    setRFGR(m_rfGR.load() - 1);
                    setIFGR(m_ifGR.load() + (gain - getRFGain()));
                }
            }
        }
        else
        {
            if (m_rfGRchangeCntr > 0)
            {
                return;
            }

            // calculate initial value of RF gain
            float gain = getRFGain() - 10 * std::log10(2 * agcLevel / (SDRPLAY_LEVEL_THR_MAX - SDRPLAY_LEVEL_THR_MIN));
            int idx = 0;
            do
            {
                if (m_rfGainList.at(idx) > gain)
                {  // found
                    break;
                }
                idx += 1;
            } while (idx < m_rfGainList.count());

            setRFGR(m_rfGainList.size() - 1 - idx);
            m_rfGRchangeCntr = 4;
            m_agcState = SwAgcState::Running;
        }
    }
    else if (FeedbackMode::IfOnly == mode)
    {  // IF gain control only
        if (agcLevel > SDRPLAY_LEVEL_THR_MAX)
        {  // decrease gain
            setIFGR(m_ifGR.load() + 1);
        }
        else if (agcLevel < SDRPLAY_LEVEL_THR_MIN)
        {
            setIFGR(m_ifGR.load() - 1);
        }
    }

    if (++m_levelEmitCntr > 4)
    {
        m_levelEmitCntr = 0;
        float gain = 112 + getRFGain() - m_ifGR.load();
        emit agcGainChanged(gain);
        emit rfLevelChanged(10 * std::log10(agcLevel) - gain, gain);
    }
}
