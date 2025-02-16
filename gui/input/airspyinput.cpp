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

#include "airspyinput.h"

#include <QDebug>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(airspyInput, "AirspyInput", QtInfoMsg)

InputDeviceList AirspyInput::getDeviceList()
{
    InputDeviceList list;

    uint64_t serials[8];
    int deviceCount = airspy_list_devices(serials, 8);
    if (0 == deviceCount)
    {
        qCInfo(airspyInput) << "No devices found";
        return list;
    }
    else
    {
        qCInfo(airspyInput) << "Found " << deviceCount << " devices.";
    }
    int n = 0;
    while (n < deviceCount)
    {
        qDebug("#%d: %llX", n, serials[n]);
        QString dispName = QString("SN: %1").arg(serials[n], 0, 16).toUpper();
        struct InputDeviceDesc desc = {.diplayName = dispName, .id = QVariant(serials[n])};
        list.append(desc);
        ++n;
    }
    return list;
}

AirspyInput::AirspyInput(bool try4096kHz, QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDevice::Id::AIRSPY;

    m_try4096kHz = try4096kHz;
    m_device = nullptr;
    m_isRecording = false;
    m_signalLevelEmitCntr = 0;
    m_src = nullptr;
    m_filterOutBuffer = nullptr;
    m_frequency = 0;
    m_biasT = false;

    connect(this, &AirspyInput::agcLevel, this, &AirspyInput::onAgcLevel, Qt::QueuedConnection);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &AirspyInput::onWatchdogTimeout);
}

AirspyInput::~AirspyInput()
{
    if (nullptr != m_device)
    {
        stop();
        airspy_close(m_device);
        airspy_exit();
    }

    if (nullptr != m_filterOutBuffer)
    {
        operator delete[](m_filterOutBuffer, std::align_val_t(16));
    }
    if (nullptr != m_src)
    {
        delete m_src;
    }
}

void AirspyInput::tune(uint32_t frequency)
{
    m_frequency = frequency;
    if (airspy_is_streaming(m_device) || (0 == frequency))
    {  // airspy is running
        //      sequence in this case is:
        //      1) stop
        //      2) wait to finish
        //      3) start new RX on new frequency
        // ==> this way we can be sure that all buffers are reset and nothing left from previous channel
        stop();
    }
    else
    {  // airspy is not running
        run();
    }
}

bool AirspyInput::openDevice(const QVariant &hwId, bool fallbackConnection)
{
    bool found = false;
    if (hwId.isValid())
    {  // try to open selected device
        uint64_t sn = hwId.toULongLong();
        if (AIRSPY_SUCCESS != airspy_open_sn(&m_device, sn))
        {
            if (fallbackConnection)
            {
                qCInfo(airspyInput, "Unable to open selected AIRSPY device: %s. Trying to find working device...",
                       QString("%1").arg(sn, 0, 16).toUpper().toLatin1().constData());
            }
            else
            {
                qCCritical(airspyInput, "Unable to open selected AIRSPY device: %s", QString("%1").arg(sn, 0, 16).toUpper().toLatin1().constData());
                return false;
            }
        }
        else
        {
            found = true;
        }
    }

    if (!found)
    {  // open first device
        if (AIRSPY_SUCCESS != airspy_open(&m_device))
        {
            qCCritical(airspyInput) << "Failed opening device";
            m_device = nullptr;
            return false;
        }
    }

    // set sample type
    if (AIRSPY_SUCCESS != airspy_set_sample_type(m_device, AIRSPY_SAMPLE_FLOAT32_IQ))
    {
        qCCritical(airspyInput) << "Cannot set sample format";
        return false;
    }

    // Set sample rate
    uint32_t sampleRate = UINT32_MAX;

    // here we try SR=4096kHz that is not oficailly supported by Airspy devices
    // however it works on Airspy Mini
    // it seems that this sample rate leads to lower CPU load
    if (m_try4096kHz && (AIRSPY_SUCCESS == airspy_set_samplerate(m_device, 4096 * 1000)))
    {  // succesfull
        sampleRate = 4096 * 1000;
        qCInfo(airspyInput) << "Sample rate set to" << sampleRate << "Hz";
    }
    else
    {  // find lowest supported sample rated that is >= 2048kHz
        uint32_t samplerateCount;
        airspy_get_samplerates(m_device, &samplerateCount, 0);
        uint32_t *samplerateArray = new uint32_t[samplerateCount];
        airspy_get_samplerates(m_device, samplerateArray, samplerateCount);
        for (int s = 0; s < samplerateCount; ++s)
        {
            if ((samplerateArray[s] >= (2048 * 1000)) && (samplerateArray[s] < sampleRate))
            {
                sampleRate = samplerateArray[s];
            }
        }
        delete[] samplerateArray;

        // Set sample rate
        if (AIRSPY_SUCCESS != airspy_set_samplerate(m_device, sampleRate))
        {
            qCCritical(airspyInput) << "Setting sample rate failed";
            return false;
        }
        else
        {
            qCInfo(airspyInput) << "Sample rate set to" << sampleRate << "Hz";
        }
    }

    // airspy provides 49152 samples in callback in packed modes
    // we cannot produce more samples in SRC => allocating for worst case
    m_filterOutBuffer = new (std::align_val_t(16)) float[49152 * 2];
    m_src = new InputDeviceSRC(sampleRate);

    // set automatic gain
    m_gainMode = AirpyGainMode::Software;
    resetAgc();

    m_deviceDescription.device.name = "AirSpy";

    char version[255];
    if (AIRSPY_SUCCESS == airspy_version_string_read(m_device, version, 255))
    {
        QString tmp(version);
        if (tmp.startsWith("AirSpy MINI", Qt::CaseInsensitive))
        {
            m_deviceDescription.device.model = "Mini";
        }
        else if (tmp.startsWith("AirSpy NOS", Qt::CaseInsensitive))
        {  // TODO: how to detect R2 correctly ???
            m_deviceDescription.device.model = "R2";
        }
        else
        {
            m_deviceDescription.device.model = "Unknown";
        }
    }
    else
    {
        m_deviceDescription.device.model = "Unknown";
    }

    airspy_read_partid_serialno_t partid_serialno;
    if (AIRSPY_SUCCESS == airspy_board_partid_serialno_read(m_device, &partid_serialno))
    {
        m_deviceDescription.device.sn = QString::number(uint64_t(partid_serialno.serial_no[2]) << 32 | partid_serialno.serial_no[3], 16).toUpper();
    }

    m_deviceDescription.sample.sampleRate = 2048000;
#if AIRSPY_RECORD_INT16
    m_deviceDescription.sample.channelBits = sizeof(int16_t) * 8;
    m_deviceDescription.sample.containerBits = sizeof(int16_t) * 8;
    m_deviceDescription.sample.channelContainer = "int16";
#else  // recording in float
    m_deviceDescription.sample.channelBits = sizeof(float) * 8;
    m_deviceDescription.sample.containerBits = sizeof(float) * 8;
    m_deviceDescription.sample.channelContainer = "float";
#endif

    emit deviceReady();

    return true;
}

void AirspyInput::run()
{
    // Reset buffer here - airspy is not running, DAB waits for new data
    inputBuffer.reset();

    m_src->reset();

    if (m_frequency != 0)
    {  // Tune to new frequency
        if (AIRSPY_SUCCESS != airspy_set_freq(m_device, m_frequency * 1000))
        {
            qCCritical(airspyInput, "Tune to %d kHz failed", m_frequency);
            emit error(InputDevice::ErrorCode::DeviceDisconnected);
            return;
        }

        resetAgc();

        m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);

        if (AIRSPY_SUCCESS != airspy_start_rx(m_device, AirspyInput::callback, (void *)this))
        {
            qCCritical(airspyInput, "Failed to start RX");
            emit error(InputDevice::ErrorCode::DeviceDisconnected);
            return;
        }
    }
    else
    { /* tune to 0 => going to idle */
    }

    emit tuned(m_frequency);
}

void AirspyInput::stop()
{
    if (AIRSPY_TRUE == airspy_is_streaming(m_device))
    {  // if devise is running, stop RX
        airspy_stop_rx(m_device);
        m_watchdogTimer.stop();

        QThread::msleep(50);
        while (AIRSPY_TRUE == airspy_is_streaming(m_device))
        {
            qCWarning(airspyInput) << "not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            QThread::msleep(2000);
        }

        run();  // restart
    }
    else if (0 == m_frequency)
    {  // going to idle
        emit tuned(0);
    }
}

void AirspyInput::setGainMode(const AirspyGainStruct &gain)
{
    switch (gain.mode)
    {
        case AirpyGainMode::Hybrid:
            if (m_gainMode == gain.mode)
            {  // do nothing -> mode does not change
                break;
            }
            // mode changes
            m_gainMode = gain.mode;
            airspy_set_lna_agc(m_device, 1);
            airspy_set_mixer_agc(m_device, 1);
            resetAgc();
            break;
        case AirpyGainMode::Manual:
            m_gainMode = gain.mode;
            airspy_set_vga_gain(m_device, gain.ifGainIdx);
            if (gain.lnaAgcEna)
            {
                airspy_set_lna_agc(m_device, 1);
            }
            else
            {
                airspy_set_lna_agc(m_device, 0);
                airspy_set_lna_gain(m_device, gain.lnaGainIdx);
            }
            if (gain.mixerAgcEna)
            {
                airspy_set_mixer_agc(m_device, 1);
            }
            else
            {
                airspy_set_mixer_agc(m_device, 0);
                airspy_set_mixer_gain(m_device, gain.mixerGainIdx);
            }
            break;
        case AirpyGainMode::Software:
            if (m_gainMode == gain.mode)
            {  // do nothing -> mode does not change
                break;
            }
            m_gainMode = gain.mode;
            resetAgc();
            break;
        case AirpyGainMode::Sensitivity:
            m_gainMode = gain.mode;
            airspy_set_sensitivity_gain(m_device, gain.sensitivityGainIdx);
            break;
    }

    emit agcGain(NAN);
}

void AirspyInput::setGain(int gainIdx)
{
    if (AirpyGainMode::Hybrid == m_gainMode)
    {
        if (gainIdx < AIRSPY_HW_AGC_MIN)
        {
            gainIdx = AIRSPY_HW_AGC_MIN;
        }
        if (gainIdx > AIRSPY_HW_AGC_MAX)
        {
            gainIdx = AIRSPY_HW_AGC_MAX;
        }

        if (gainIdx == m_gainIdx)
        {
            return;
        }
        // else
        m_gainIdx = gainIdx;

        if (AIRSPY_SUCCESS != airspy_set_vga_gain(m_device, m_gainIdx))
        {
            qCWarning(airspyInput) << "Failed to set tuner gain";
        }
        else
        {
            qCDebug(airspyInput) << "Tuner VGA gain set to" << gainIdx;
            // emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
    if (AirpyGainMode::Software == m_gainMode)
    {
        if (gainIdx < AIRSPY_SW_AGC_MIN)
        {
            gainIdx = AIRSPY_SW_AGC_MIN;
        }
        if (gainIdx > AIRSPY_SW_AGC_MAX)
        {
            gainIdx = AIRSPY_SW_AGC_MAX;
        }

        if (gainIdx == m_gainIdx)
        {
            return;
        }
        // else
        m_gainIdx = gainIdx;

        if (AIRSPY_SUCCESS != airspy_set_sensitivity_gain(m_device, m_gainIdx))
        {
            qCWarning(airspyInput) << "Failed to set tuner gain";
        }
        else
        {
            qCDebug(airspyInput) << "Tuner Sensitivity gain set to" << gainIdx;
        }
        return;
    }
}

void AirspyInput::resetAgc()
{
    m_src->resetSignalLevel(AIRSPY_LEVEL_RESET);
    m_signalLevelEmitCntr = 0;

    if (AirpyGainMode::Software == m_gainMode)
    {
        m_gainIdx = -1;
        setGain((AIRSPY_SW_AGC_MAX + 1) / 2);  // set it to the middle
        return;
    }
    if (AirpyGainMode::Hybrid == m_gainMode)
    {
        m_gainIdx = -1;
        setGain(6);
    }
}

void AirspyInput::onAgcLevel(float level)
{
    if (level > AIRSPY_LEVEL_THR_MAX)
    {
        setGain(m_gainIdx - 1);
        return;
    }
    if (level < AIRSPY_LEVEL_THR_MIN)
    {
        setGain(m_gainIdx + 1);
    }
}

void AirspyInput::onWatchdogTimeout()
{
    if (AIRSPY_TRUE != airspy_is_streaming(m_device))
    {
        qCCritical(airspyInput) << "watchdog timeout";
        inputBuffer.fillDummy();
        emit error(InputDevice::ErrorCode::NoDataAvailable);
    }
}

void AirspyInput::startStopRecording(bool start)
{
    m_isRecording = start;
}

void AirspyInput::setBiasT(bool ena)
{
    if (ena != m_biasT)
    {
        if (AIRSPY_SUCCESS != airspy_set_rf_bias(m_device, ena))
        {
            qCWarning(airspyInput) << "Failed to enable bias-T";
        }
        else
        {
            qCInfo(airspyInput) << "Bias-T" << (ena ? "on" : "off");
            m_biasT = ena;
        }
    }
}

void AirspyInput::setDataPacking(bool ena)
{
    if (AIRSPY_SUCCESS != airspy_set_packing(m_device, ena))
    {
        qCWarning(airspyInput) << "Failed to set data packing";
    }
}

QVariant AirspyInput::hwId()
{
    airspy_read_partid_serialno_t partid_serialno;
    if (AIRSPY_SUCCESS == airspy_board_partid_serialno_read(m_device, &partid_serialno))
    {
        return QVariant(int64_t(partid_serialno.serial_no[2]) << 32 | partid_serialno.serial_no[3]);
    }
    return QVariant();
}

void AirspyInput::doRecordBuffer(const float *buf, uint32_t len)
{
#if AIRSPY_RECORD_INT16
    // dumping in int16
    int16_t int16Buf[len];
    for (int n = 0; n < len; ++n)
    {
        int16Buf[n] = *buf++ * AIRSPY_RECORD_FLOAT2INT16;
    }
    emit recordBuffer((const uint8_t *)int16Buf, len * sizeof(int16_t));
#else
    // dumping in float
    emit recordBuffer((const uint8_t *)buf, len * sizeof(float));
#endif
}

int AirspyInput::callback(airspy_transfer *transfer)
{
    static_cast<AirspyInput *>(transfer->ctx)->processInputData(transfer);
    return 0;
}

void AirspyInput::processInputData(airspy_transfer *transfer)
{
    if (transfer->dropped_samples > 0)
    {
        qCWarning(airspyInput) << "Dropping" << transfer->dropped_samples << "samples";
    }

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    pthread_mutex_unlock(&inputBuffer.countMutex);

    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz
    int numIQ = m_src->process((float *)transfer->samples, transfer->sample_count, m_filterOutBuffer);
    uint64_t bytesToWrite = numIQ * 2 * sizeof(float);

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite)
    {
        qCWarning(airspyInput) << "Dropping" << transfer->sample_count << "IQ samples...";
        return;
    }

#if (AIRSPY_AGC_ENABLE > 0)
    if (0 == (++m_signalLevelEmitCntr & 0x07))
    {
        emit agcLevel(m_src->signalLevel());
    }
#endif

    if (m_isRecording)
    {
        doRecordBuffer(m_filterOutBuffer, 2 * numIQ);
    }

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

    if (bytesTillEnd >= bytesToWrite)
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *)m_filterOutBuffer, bytesToWrite);
        inputBuffer.head = (inputBuffer.head + bytesToWrite);
    }
    else
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *)m_filterOutBuffer, bytesTillEnd);
        std::memcpy((inputBuffer.buffer), ((uint8_t *)m_filterOutBuffer) + bytesTillEnd, bytesToWrite - bytesTillEnd);
        inputBuffer.head = bytesToWrite - bytesTillEnd;
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + bytesToWrite;
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
