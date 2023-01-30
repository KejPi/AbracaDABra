/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDir>
#include <QDebug>
#include "soapysdrinput.h"

SoapySdrInput::SoapySdrInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDeviceId::SOAPYSDR;

    m_device = nullptr;
    m_deviceUnpluggedFlag = true;
    m_deviceRunningFlag = false;
    m_gainList = nullptr;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &SoapySdrInput::onWatchdogTimeout);
}

SoapySdrInput::~SoapySdrInput()
{    
    stop();
    if (nullptr != m_device)
    {
        SoapySDR::Device::unmake(m_device);
    }
    if (nullptr != m_gainList)
    {
        delete m_gainList;
    }
}

bool SoapySdrInput::openDevice()
{
    int ret = 0;

    if (m_devArgs.isEmpty())
    {   // no device args provided => try to open first device
        SoapySDR::KwargsList devs = SoapySDR::Device::enumerate();
        if (0 == devs.size())
        {
            qDebug() << "SoapySDR: No devices found";
            return false;
        }
        else
        {
            qDebug() << "SoapySDR: Found " << devs.size() << " devices. Using the first working one";
        }

        // iterate through th devices and open first working
        for( int n = 0; n < devs.size(); ++n)
        {
            m_device = SoapySDR::Device::make(devs[n]);
            if (nullptr != m_device)
            {
                qDebug("SoapySDR: Making device #%d", n);
                SoapySDR::Kwargs::iterator it;
                for( it = devs[n].begin(); it != devs[n].end(); ++it)
                {
                    qDebug("    %s = %s\n", it->first.c_str(), it->second.c_str());
                }
                break;
            }
            else { /* failed */ }
        }
    }
    else
    {   // device args provided
        m_device = SoapySDR::Device::make(m_devArgs.toStdString());
        if (nullptr == m_device)
        {
            qDebug() << "SoapySDR: Failed to make device with args:" << m_devArgs;
            return false;
        }
        else { /* success */ }
    }

    if (nullptr == m_device)
    {
        qDebug() << "SoapySDR: Failed to make device";
        return false;
    }
    else { /* success */ }

    if (m_rxChannel >= m_device->getNumChannels(SOAPY_SDR_RX))
    {
        qDebug("SoapySDR: channel #%d not supported. Using channel 0", m_rxChannel);
        m_rxChannel = 0;
    }
    else { /* OK */ }

    try
    {
        m_device->setAntenna(SOAPY_SDR_RX, m_rxChannel, m_antenna.toStdString());
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SoapySDR: Error setting antenna" << ex.what();
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        return false;
    }

    // check sample format -> need CF32
    std::vector<std::string> formats = m_device->getStreamFormats(SOAPY_SDR_RX, m_rxChannel);
    if (std::find(formats.begin(), formats.end(), SOAPY_SDR_CF32) == formats.end())
    {   // not found
        qDebug() << "SoapySDR: Failed to open device. CF32 format not supported.";
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        return false;
    }
    else { /* CF32 supported */ }

    // Set sample rate - prefered rates: 2048, 4096 and then the lowest above 2048
    SoapySDR::RangeList srRanges = m_device->getSampleRateRange( SOAPY_SDR_RX, m_rxChannel);

    qDebug() << "SoapySDR: sample rate ranges:";
    for(int n = 0; n < srRanges.size(); ++n)
    {
        qDebug("[%g Hz - %g Hz], ", srRanges[n].minimum(), srRanges[n].maximum());
    }

    m_sampleRate = 10e8;  // dummy high value
    bool has2048 = false;
    bool has4096 = false;
    for(int n = 0; n < srRanges.size(); ++n)
    {
        if (2048e3 <= srRanges[n].maximum())
        {   // max >= 2048e3
            if (2048e3 >= srRanges[n].minimum())
            {   // 2048kHz is in the range => lets use it, nothing else is important
                has2048 = true;
                break;
            }
            else { /* 2048kHz is not in this range */ }

            if ((4096e3 >= srRanges[n].minimum()) && (4096e3 <= srRanges[n].maximum()))
            {   // 4096kHz is in the range
                has4096 = true;
            }
            else { /* 4096kHz not in this range */ }

            // we know that max >= 2048e3 && min >= 2048e3

            // lets take minimum
            if (m_sampleRate > srRanges[n].minimum())
            {
                m_sampleRate = srRanges[n].minimum();
            }
        }
        else { /* SR range is below 2048kHz */ }
    }

    // lets evaluate results
    if (has2048)
    {
        m_sampleRate = 2048e3;
    }
    else if (has4096)
    {
        m_sampleRate = 4096e3;
    }
    else { /* neither 2048 nor 4096 kHz supported - using the lowest possible */ }

    try
    {
        m_device->setSampleRate(SOAPY_SDR_RX, m_rxChannel, m_sampleRate);
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SoapySDR: Error setting sample rate" << ex.what();
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        return false;
    }
    qDebug() << "SoapySDR: Sample rate set to" << m_sampleRate << "Hz";

    // Get gains
    SoapySDR::Range gainRange = m_device->getGainRange(SOAPY_SDR_RX, m_rxChannel);

    double gainStep = gainRange.step();
    if (0.0 == gainStep)
    {   // expecting step 1
        gainStep = 1.0;
    }
    m_gainList = new QList<float>();
    double gain = gainRange.minimum();
    while (gain <= gainRange.maximum())
    {
        m_gainList->append(gain);
        gain += gainStep;
    }

    if (m_device->hasGainMode(SOAPY_SDR_RX, m_rxChannel))
    {   // set automatic gain
        setGainMode(SoapyGainMode::Hardware);
    }
    else
    {
        setGainMode(SoapyGainMode::Software);
    }

    if (m_device->hasDCOffsetMode(SOAPY_SDR_RX, m_rxChannel))
    {   // set DC offset correction
        m_device->setDCOffsetMode(SOAPY_SDR_RX, m_rxChannel, true);
    }
    else { /* DC offset correction not available */ }

    // set bandwidth
    setBW(SOAPYSDR_BANDWIDTH);

    m_deviceDescription.device.name = "SoapySDR | " + QString(m_device->getDriverKey().c_str());
    m_deviceDescription.device.model = QString(m_device->getHardwareKey().c_str());
    m_deviceDescription.sample.sampleRate = 2048000;
#if SOAPYSDR_RECORD_INT16
    m_deviceDescription.sample.channelBits = sizeof(int16_t) * 8;
    m_deviceDescription.sample.containerBits = sizeof(int16_t) * 8;
    m_deviceDescription.sample.channelContainer = "int16";
#else // recording in float
    m_deviceDescription.sample.channelBits = sizeof(float) * 8;
    m_deviceDescription.sample.containerBits = sizeof(float) * 8;
    m_deviceDescription.sample.channelContainer = "float";
#endif

    SoapySDR::Stream *stream;
    try
    {
        stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32, std::vector<size_t>(m_rxChannel));
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SoapySDR: Error setting stream" << ex.what();
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        return false;
    }

    if (nullptr == stream)
    {
        qDebug() << "SoapySDR: failed to setup stream";
        SoapySDR::Device::unmake(m_device);
        m_device = nullptr;
        return false;
    }
    else
    {   // stream is functional -> closing
        m_device->closeStream(stream);
    }

    m_deviceUnpluggedFlag = false;

    emit deviceReady();

    return true;
}

void SoapySdrInput::tune(uint32_t frequency)
{
    m_frequency = frequency;
    if ((m_deviceRunningFlag) || (0 == frequency))
    {   // worker is running
        //      sequence in this case is:
        //      1) stop
        //      2) wait for thread to finish
        //      3) start new worker on new frequency
        // ==> this way we can be sure that all buffers are reset and nothing left from previous channel
        stop();
    }
    else
    {   // worker is not running
        run();
    }
}

void SoapySdrInput::run()
{
    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (m_frequency != 0)
    {   // Tune to new frequency
        m_device->setFrequency(SOAPY_SDR_RX, m_rxChannel, m_frequency*1000);

        // does nothing if manual AGC
        resetAgc();

        m_worker = new SoapySdrWorker(m_device, m_sampleRate, m_rxChannel, this);
        connect(m_worker, &SoapySdrWorker::agcLevel, this, &SoapySdrInput::onAgcLevel, Qt::QueuedConnection);
        connect(m_worker, &SoapySdrWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
        connect(m_worker, &SoapySdrWorker::finished, this, &SoapySdrInput::onReadThreadStopped, Qt::QueuedConnection);
        connect(m_worker, &SoapySdrWorker::finished, m_worker, &QObject::deleteLater);

        m_worker->start();
        m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
        m_deviceRunningFlag = true;
    }
    else
    { /* tune to 0 => going to idle */  }

    emit tuned(m_frequency);
}

void SoapySdrInput::stop()
{
    if (m_deviceRunningFlag)
    {   // if devise is running, stop worker
        m_deviceRunningFlag = false;       // this flag say that we want to stop worker intentionally
                                     // (checked in readThreadStopped() slot)
        // request to stop reading data
        m_worker->stop();

        // wait until thread finishes
        m_worker->wait(QDeadlineTimer(2000));
        while (!m_worker->isFinished())
        {
            qDebug() << "SoapySDR: Worker thread not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            m_worker->wait(2000);
        }
    }
    else if (0 == m_frequency)
    {   // going to idle
        emit tuned(0);
    }
}

void SoapySdrInput::setGainMode(SoapyGainMode gainMode, int gainIdx)
{
    if (gainMode != m_gainMode)
    {
        if (SoapyGainMode::Hardware == gainMode)
        {
            if (m_device->hasGainMode(SOAPY_SDR_RX, m_rxChannel))
            {
                m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, true);
            }
            else
            {
                m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, false);
                gainMode = SoapyGainMode::Software;   // SW AC is fallback
            }
        }
        else
        {
            m_device->setGainMode(SOAPY_SDR_RX, m_rxChannel, false);
        }

        m_gainMode = gainMode;

        // does nothing in (GainMode::Software != mode)
        resetAgc();
    }

    if (SoapyGainMode::Manual == m_gainMode)
    {
        setGain(gainIdx);

        // always emit gain when switching mode to manual
        emit agcGain(m_gainList->at(gainIdx));
    }

    if (SoapyGainMode::Hardware == m_gainMode)
    {   // signalize that gain is not available
        emit agcGain(NAN);
    }
}

void SoapySdrInput::setGain(int gainIdx)
{
    // force index vaslidity
    if (gainIdx < 0)
    {
        gainIdx = 0;
    }
    if (gainIdx >= m_gainList->size())
    {
        gainIdx = m_gainList->size() - 1;
    }

    if (gainIdx != m_gainIdx)
    {
        m_gainIdx = gainIdx;
        m_device->setGain(SOAPY_SDR_RX, m_rxChannel, m_gainList->at(m_gainIdx));
        emit agcGain(m_gainList->at(m_gainIdx));
    }
}

void SoapySdrInput::resetAgc()
{
    if (SoapyGainMode::Software == m_gainMode)
    {
        m_gainIdx = -1;
        setGain(m_gainList->size() >> 1);
    }
}

void SoapySdrInput::onAgcLevel(float agcLevel)
{
    if (SoapyGainMode::Software == m_gainMode)
    {
        if (agcLevel > SOAPYSDR_LEVEL_THR_MAX)
        {
            setGain(m_gainIdx-1);
            return;
        }
        if (agcLevel < SOAPYSDR_LEVEL_THR_MIN)
        {
            setGain(m_gainIdx+1);
        }
    }
}

void SoapySdrInput::onReadThreadStopped()
{
    m_watchdogTimer.stop();

    if (m_deviceRunningFlag)
    {   // if device should be running then it measn reading error thus device is disconnected
        qDebug() << "SoapySDR: device is unplugged.";
        m_deviceUnpluggedFlag = true;
        m_deviceRunningFlag = false;

        // fill buffer (artificially to avoid blocking of the DAB processing thread)
        inputBuffer.fillDummy();

        emit error(InputDeviceErrorCode::DeviceDisconnected);
    }
    else
    {
        // in this case thread was stopped by to tune to new frequency, there is no other reason to stop the thread
        run();
    }
}

void SoapySdrInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        bool isRunning = m_worker->isRunning();
        if (!isRunning)
        {  // some problem in data input
            qDebug() << "SoapySDR: watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDeviceErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void SoapySdrInput::startStopRecording(bool start)
{
    m_worker->startStopRecording(start);
}

void SoapySdrInput::setBW(int bw)
{
    if (bw > 0)
    {
        try
        {
            m_device->setBandwidth(SOAPY_SDR_RX, m_rxChannel, bw);
        }
        catch(const std::exception &ex)
        {
            qDebug() << "SoapySDR: failed to set bandwidth" << bw << ":" << ex.what();
            return;
        }
        qDebug() << "SoapySDR: bandwidth set to" << bw/1000.0 << "kHz";
    }
}

SoapySdrWorker::SoapySdrWorker(SoapySDR::Device * device, double sampleRate, int rxChannel, QObject *parent)
    : QThread(parent)
{
    m_isRecording = false;
    m_device =  device;
    m_rxChannel = rxChannel;

    // we cannot produce more samples in SRC
    m_filterOutBuffer = (float *) operator new[](sizeof(float) * SOAPYSDR_INPUT_SAMPLES*2, (std::align_val_t)(16));
    m_src = new InputDeviceSRC(sampleRate);
}

SoapySdrWorker::~SoapySdrWorker()
{
    delete m_src;
    operator delete [] (m_filterOutBuffer, std::align_val_t(16));
}

void SoapySdrWorker::run()
{
    m_doReadIQ = true;

    m_agcLevel = 0.0;
    m_watchdogFlag = false;  // first callback sets it to true
    m_signalLevelEmitCntr = 0;

    SoapySDR::Stream *stream = nullptr;
    try
    {
        stream = m_device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32, std::vector<size_t>(m_rxChannel));
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SoapySDR: Error setup stream" << ex.what();
        return;
    }

    m_device->activateStream(stream);

    std::complex<float> inputBuffer[SOAPYSDR_INPUT_SAMPLES];
    void *buffs[] = {inputBuffer};

    while (m_doReadIQ)
    {
        int flags;
        long long time_ns;

        // read samples with timeout 100 ms
        int ret = m_device->readStream(stream, buffs, SOAPYSDR_INPUT_SAMPLES, flags, time_ns, 100000);

        // reset watchDog flag, timer sets it to false
        m_watchdogFlag = true;

        if (ret <= 0)
        {
            if (ret == SOAPY_SDR_TIMEOUT)
            {
                qDebug() << "SoapySDR: Stream timeout";
                break;
            }
            if (ret == SOAPY_SDR_OVERFLOW)
            {
                qDebug() << "SoapySDR: Stream overflow";
                continue;
            }
            if (ret == SOAPY_SDR_UNDERFLOW)
            {
                qDebug() << "SoapySDR: Stream underflow";
                continue;
            }
            if (ret < 0)
            {
                qDebug() << "SoapySDR: Unexpected stream error" << SoapySDR_errToStr(ret);
                break;
            }
        }
        else
        {
            // OK, process data
            processInputData(inputBuffer, ret);
        }
    }

    // deactivate stream
    m_device->deactivateStream(stream, 0, 0);
    m_device->closeStream(stream);

    // exit of the thread
}

void SoapySdrWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void SoapySdrWorker::doRecordBuffer(const float *buf, uint32_t len)
{
#if SOAPYSDR_RECORD_INT16
    // dumping in int16
    int16_t int16Buf[len];
    for (int n = 0; n < len; ++n)
    {
        int16Buf[n] = int16_t(*buf++ * SOAPYSDR_RECORD_FLOAT2INT16);
    }
    emit recordBuffer((const uint8_t *) int16Buf, len * sizeof(int16_t));
#else
    // dumping in float
    emit recordBuffer((const uint8_t *) buf, len * sizeof(float));
#endif
}

bool SoapySdrWorker::isRunning()
{
    bool flag = m_watchdogFlag;
    m_watchdogFlag = false;
    return flag;
}

void SoapySdrWorker::stop()
{
    m_doReadIQ = false;
}

void SoapySdrWorker::processInputData(std::complex<float> buff[], size_t numSamples)
{
    static float signalLevel = SOAPYSDR_LEVEL_RESET;

    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    pthread_mutex_unlock(&inputBuffer.countMutex);

    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    // input samples are IQ = [float float] @ sampleRate
    // going to transform them to [float float] @ 2048kHz  
    int numOutputIQ = m_src->process((float*) buff, numSamples, m_filterOutBuffer);

    uint64_t bytesToWrite = numOutputIQ * 2 * sizeof(float);

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite)
    {
        qDebug() << "SoapySDR: dropping" << numSamples << "IQ samples...";
        return;
    }

    if (0 == (++m_signalLevelEmitCntr & 0x0F))
    {
        emit agcLevel(m_src->signalLevel());
    }

    if (m_isRecording)
    {
        doRecordBuffer(m_filterOutBuffer, 2*numOutputIQ);
    }

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

    if (bytesTillEnd >= bytesToWrite)
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *) m_filterOutBuffer, bytesToWrite);
        inputBuffer.head = (inputBuffer.head + bytesToWrite);
    }
    else
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *) m_filterOutBuffer, bytesTillEnd);
        std::memcpy((inputBuffer.buffer), ((uint8_t *) m_filterOutBuffer)+bytesTillEnd, bytesToWrite-bytesTillEnd);
        inputBuffer.head = bytesToWrite-bytesTillEnd;
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + bytesToWrite;
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

