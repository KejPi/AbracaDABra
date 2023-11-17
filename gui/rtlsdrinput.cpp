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
#include <QLoggingCategory>
#include "rtlsdrinput.h"

Q_LOGGING_CATEGORY(rtlsdrInput, "RtlSdrInput", QtInfoMsg)

RtlSdrInput::RtlSdrInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDeviceId::RTLSDR;

    m_device = nullptr;
    m_worker = nullptr;
    m_gainList = nullptr;
    m_agcLevelMinFactorList = nullptr;

    m_agcLevelMax = RTLSDR_AGC_LEVEL_MAX_DEFAULT;
    m_agcLevelMin = 60;

    m_bandwidth = 0;
    m_frequency = 0;
    m_biasT = false;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &RtlSdrInput::onWatchdogTimeout);
}

RtlSdrInput::~RtlSdrInput()
{
    RtlSdrInput::tune(0);  // this stops data acquisition thread
    if (nullptr != m_device)
    {
        rtlsdr_close(m_device);
    }
    if (nullptr != m_gainList)
    {
        delete m_gainList;
    }
    if (nullptr != m_agcLevelMinFactorList)
    {
        delete m_agcLevelMinFactorList;
    }
}

void RtlSdrInput::tune(uint32_t frequency)
{
    m_frequency = frequency;
    if (0 != frequency)
    {   // tuning
        if (nullptr == m_worker)
        {   // reset endpoint before starting data acquisition
            rtlsdr_reset_buffer(m_device);

            rtlsdr_set_center_freq(m_device, m_frequency*1000);

            // does nothing if not SW AGC
            resetAgc();

            // start worker
            run();

            // tuned(m_frequency) signal is emited when dataReady() from worker
        }
        else
        {   // worker is already running
            rtlsdr_set_center_freq(m_device, m_frequency*1000);

            // does nothing if not SW AGC
            resetAgc();

            // restart data acquisition
            m_worker->restart();

            // tuned(m_frequency) signal is emited when dataReady() from worker
        }
    }
    else
    {   // frequency == 0 ==> go to idle
        stop();

        // tuned(0) will be emitted when worker finishes
    }
}

bool RtlSdrInput::openDevice()
{
    int ret = 0;

    // Get all devices
    uint32_t deviceCount = rtlsdr_get_device_count();
    if (deviceCount == 0)
    {
        qCWarning(rtlsdrInput) << "No devices found";
        return false;
    }
    else
    {
        qCInfo(rtlsdrInput) << "Found" << deviceCount << "devices";
    }

    //	Iterate over all found rtl-sdr devices and try to open it. Stops if one device is successfull opened.
    const char * deviceName;
    for(uint32_t n=0; n<deviceCount; ++n)
    {
        ret = rtlsdr_open(&m_device, n);
        if (ret >= 0)
        {
            deviceName = rtlsdr_get_device_name(n);
            if (NULL == deviceName)
            {
                qCInfo(rtlsdrInput) << "Opening rtl-sdr device" << n;
            }
            else
            {
                qCInfo(rtlsdrInput, "Opening rtl-sdr device #%d: %s", n, deviceName);
            }                        
            break;
        }
        else { /* not successful */ }
    }

    if (ret < 0)
    {   // no device found
        qCCritical(rtlsdrInput) << "Opening rtl-sdr failed";
        return false;
    }
    else { /* some device was opened successfully */ }

    // Set sample rate
    ret = rtlsdr_set_sample_rate(m_device, 2048000);
    if (ret < 0)
    {
        qCCritical(rtlsdrInput) << "Setting sample rate failed";
        return false;
    }
    else { /* OK */ }

    // store device information
    m_deviceDescription.device.name = "rtl-sdr";
    switch (rtlsdr_get_tuner_type(m_device))
    {
    case RTLSDR_TUNER_E4000:
        m_deviceDescription.device.name += " [E4000]";
        break;
    case RTLSDR_TUNER_FC0012:
        m_deviceDescription.device.name += " [FC0012]";
        break;
    case RTLSDR_TUNER_FC0013:
        m_deviceDescription.device.name += " [FC0013]";
        break;
    case RTLSDR_TUNER_FC2580:
        m_deviceDescription.device.name += " [FC2580]";
        break;
    case RTLSDR_TUNER_R820T:
        m_deviceDescription.device.name += " [R820T]";
        break;
    case RTLSDR_TUNER_R828D:
        m_deviceDescription.device.name += " [R828D]";
        break;
    default:
        break;
    }
    if (NULL == deviceName)
    {
        m_deviceDescription.device.model = "Generic RTL2832U OEM";
    }
    else
    {
        m_deviceDescription.device.model = QString(deviceName);
    }
    m_deviceDescription.sample.sampleRate = 2048000;
    m_deviceDescription.sample.channelBits = 8;
    m_deviceDescription.sample.containerBits = 8;
    m_deviceDescription.sample.channelContainer = "uint8";

    // Try to tune -> this is required for successful BW settings
    ret = rtlsdr_set_center_freq(m_device, 200000*1000);
    if (ret < 0)
    {
        qCWarning(rtlsdrInput) << "Setting frequency failed";
        // this is not fatal error
    }
    else { /* OK */ }


    // Get tuner gains
    uint32_t gainsCount = rtlsdr_get_tuner_gains(m_device, NULL);
    qCInfo(rtlsdrInput) << "Supported gain values" << gainsCount;
    int * gains = new int[gainsCount];
    gainsCount = rtlsdr_get_tuner_gains(m_device, gains);

    m_gainList = new QList<int>();
    for (int i = 0; i < gainsCount; i++) {
        m_gainList->append(gains[i]);
    }
    delete [] gains;

    m_agcLevelMinFactorList = new QList<float>();
    for (int i = 1; i < m_gainList->count(); i++) {
        // up step + 0.5dB
        m_agcLevelMinFactorList->append(qPow(10.0, (m_gainList->at(i-1) - m_gainList->at(i) - 5)/200.0));
    }
    // last factor does not matter
    m_agcLevelMinFactorList->append(qPow(10.0, -5.0 / 20.0));

    // set automatic gain
    setGainMode(RtlGainMode::Software);

    emit deviceReady();

    return true;
}

void RtlSdrInput::run()
{
    m_worker = new RtlSdrWorker(m_device, this);
    connect(m_worker, &RtlSdrWorker::agcLevel, this, &RtlSdrInput::onAgcLevel, Qt::QueuedConnection);
    connect(m_worker, &RtlSdrWorker::dataReady, this, [=](){ emit tuned(m_frequency); }, Qt::QueuedConnection);
    connect(m_worker, &RtlSdrWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
    connect(m_worker, &RtlSdrWorker::finished, this, &RtlSdrInput::onReadThreadStopped, Qt::QueuedConnection);
    connect(m_worker, &RtlSdrWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &RtlSdrWorker::destroyed, this, [=]() { m_worker = nullptr; } );

    m_worker->start();
    m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);    
}

void RtlSdrInput::stop()
{
    if (nullptr != m_worker)
    {   // if worker is running, stop worker
        rtlsdr_cancel_async(m_device);

        m_worker->wait(QDeadlineTimer(2000));
        while (!m_worker->isFinished())
        {
            qCWarning(rtlsdrInput) << "Worker thread not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            m_worker->wait(2000);
        }
    }
    else { /* not running - doing nothing */ }
}

void RtlSdrInput::onReadThreadStopped()
{
    m_watchdogTimer.stop();
    if (0 != m_frequency)
    {   // if device should be running then it means reading error thus device is disconnected
        qCCritical(rtlsdrInput) << "Device unplugged.";

        // fill buffer (artificially to avoid blocking of the DAB processing thread)
        inputBuffer.fillDummy();

        m_frequency = 0;

        emit error(InputDeviceErrorCode::DeviceDisconnected);
    }
    else
    { /* do nothing -> go to idle was requested */ }

    emit tuned(0);
}

void RtlSdrInput::setGainMode(RtlGainMode gainMode, int gainIdx)
{
    if (gainMode != m_gainMode)
    {
        // set automatic gain 0 or manual 1
        int ret = rtlsdr_set_tuner_gain_mode(m_device, (RtlGainMode::Hardware != gainMode));
        if (ret != 0)
        {
            qCWarning(rtlsdrInput) << "Failed to set tuner gain";
        }

        m_gainMode = gainMode;

        // does nothing in (GainMode::Software != mode)
        resetAgc();
    }

    if (RtlGainMode::Manual == m_gainMode)
    {
        setGain(gainIdx);

        // always emit gain when switching mode to manual
        emit agcGain(m_gainList->at(gainIdx)*0.1);
    }

    if (RtlGainMode::Hardware == m_gainMode)
    {   // signalize that gain is not available
        emit agcGain(NAN);
    }
}

void RtlSdrInput::setGain(int gIdx)
{
    // force index validity
    if (gIdx < 0)
    {
        gIdx = 0;
    }
    if (gIdx >= m_gainList->size())
    {
        gIdx = m_gainList->size() - 1;
    }

    if (gIdx != m_gainIdx)
    {
        m_gainIdx = gIdx;
        m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;
        // qDebug() << m_agcLevelMax << m_agcLevelMin;
        int ret = rtlsdr_set_tuner_gain(m_device, m_gainList->at(m_gainIdx));
        if (ret != 0)
        {
            qCWarning(rtlsdrInput) << "Failed to set tuner gain";
        }
        else
        {
            emit agcGain(m_gainList->at(m_gainIdx)*0.1);
        }
    }
}

void RtlSdrInput::resetAgc()
{
    if (RtlGainMode::Software == m_gainMode)
    {
        setGain(m_gainList->size() >> 1);
    }
}

void RtlSdrInput::onAgcLevel(float agcLevel)
{
    // qDebug() << agcLevel;
    if (RtlGainMode::Software == m_gainMode)
    {
        if (agcLevel < m_agcLevelMin)
        {
            setGain(m_gainIdx+1);
        }
        if (agcLevel > m_agcLevelMax)
        {
            setGain(m_gainIdx-1);
        }
    }
}

void RtlSdrInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        if (!m_worker->isRunning())
        {  // some problem in data input
            qCCritical(rtlsdrInput) << "Watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDeviceErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void RtlSdrInput::startStopRecording(bool start)
{
    m_worker->startStopRecording(start);
}

void RtlSdrInput::setBW(uint32_t bw)
{   
    if (bw <= 0)
    {   // setting default BW
        bw = INPUTDEVICE_BANDWIDTH;   // 1.53 MHz
    }
    else
    { /* BW set by user */ }

    if (bw != m_bandwidth)
    {
        m_bandwidth = bw;
#ifdef RTLSDR_OLD_DAB
        // this code needs rtlsdr implementation from here: https://github.com/old-dab/rtlsdr
        // how to detect it correctly ???
        uint32_t applied_bw;
        int ret = rtlsdr_set_and_get_tuner_bandwidth(m_device, bw, &applied_bw, 1);
        if (ret != 0)
        {
            qCWarning(rtlsdrInput) << "Failed to set tuner bandwidth" << bw/1000 << "kHz";
        }
        else
        {
            if (applied_bw)
            {
                qCInfo(rtlsdrInput) << "Setting bandwidth" << bw/1000.0 << "kHz resulted to" << applied_bw/1000 << "kHz";
            }
            else
            {
                qCInfo(rtlsdrInput) << "Setting bandwidth" << bw/1000.0 << "kHz";
            }
        }
#else
        int ret = rtlsdr_set_tuner_bandwidth(m_device, bw);
        if (ret != 0)
        {
            qCWarning(rtlsdrInput) << "Failed to set tuner bandwidth" << bw/1000 << "kHz";
        }
        else
        {
            qCInfo(rtlsdrInput) << "Setting bandwidth" << bw/1000.0 << "kHz";
        }
#endif
    }
}

void RtlSdrInput::setBiasT(bool ena)
{
    if (ena != m_biasT)
    {
        int ret = rtlsdr_set_bias_tee(m_device, ena);
        if (ret != 0)
        {
            qCWarning(rtlsdrInput) << "Failed to enable bias-T";
        }
        else
        {
            qCInfo(rtlsdrInput) << "Bias-T" << (ena ? "on" : "off");
            m_biasT = ena;
        }
    }
}

void RtlSdrInput::setAgcLevelMax(float agcLevelMax)
{
    if (agcLevelMax <= 0)
    {   // default value
        agcLevelMax = RTLSDR_AGC_LEVEL_MAX_DEFAULT;
    }
    m_agcLevelMax = agcLevelMax;
    m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;
}

QList<float> RtlSdrInput::getGainList() const
{
    QList<float> ret;
    for (int g = 0; g < m_gainList->size(); ++g)
    {
        ret.append(m_gainList->at(g)/10.0);
    }
    return ret;
}

RtlSdrWorker::RtlSdrWorker(struct rtlsdr_dev * device, QObject *parent) : QThread(parent)
{
    m_isRecording = false;
    m_rtlSdrPtr = parent;
    m_device = device;
}

void RtlSdrWorker::run()
{
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_watchdogFlag = false;  // first callback sets it to true
    m_captureStartCntr = 1;  // first callback resets buffer

    rtlsdr_read_async(m_device, callback, (void*)this, 0, INPUT_CHUNK_IQ_SAMPLES*2*sizeof(uint8_t));
}

void RtlSdrWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void RtlSdrWorker::restart()
{
    m_captureStartCntr = RTLSDR_RESTART_COUNTER;
}

bool RtlSdrWorker::isRunning()
{
    bool flag = m_watchdogFlag;
    m_watchdogFlag = false;
    return flag;
}

void RtlSdrWorker::callback(unsigned char *buf, uint32_t len, void * ctx)
{
    static_cast<RtlSdrWorker *>(ctx)->processInputData(buf, len);
}

void RtlSdrWorker::processInputData(unsigned char *buf, uint32_t len)
{
#if (RTLSDR_DOC_ENABLE > 0)
    int_fast32_t sumI = 0;
    int_fast32_t sumQ = 0;
#endif

    if (m_captureStartCntr > 0)
    {   // reset procedure
        if (0 == --m_captureStartCntr)
        {   // restart finished

            // clear buffer to avoid mixing of channels
            inputBuffer.reset();

            m_dcI = 0.0;
            m_dcQ = 0.0;
            //m_agcLevel = 0.0;

            emit dataReady();
        }
        else
        {   // only reecord if recording
            if (m_isRecording)
            {
                emit recordBuffer(buf, len);
            }
            else { /* not recording */ }

            // reset watchDog flag, timer sets it to false
            m_watchdogFlag = true;

            return;
        }
    }
    else { /* normal operation */ }

    if (m_isRecording)
    {
        emit recordBuffer(buf, len);
    }
    else { /* not recording */ }

    // reset watchDog flag, timer sets it to false
    m_watchdogFlag = true;

    // retrieving memories
#if (RTLSDR_DOC_ENABLE > 0)
    float dcI = m_dcI;
    float dcQ = m_dcQ;
#endif
#if (RTLSDR_AGC_ENABLE > 0)
    float agcLev = m_agcLevel;
#endif

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    if ((INPUT_FIFO_SIZE - count) < len*sizeof(float))
    {
        qCWarning(rtlsdrInput) << "Dropping" << len << "bytes...";
        return;
    }

    // input samples are IQ = [uint8_t uint8_t]
    // going to transform them to [float float] = float _Complex
    // on uint8_t will be transformed to one float

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    uint8_t * inPtr = buf;
    if (bytesTillEnd >= len*sizeof(float))
    {
        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<len; k++)
        {   // convert to float
#if ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLSDR_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLSDR_AGC_ENABLE > 0)

#if (RTLSDR_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLSDR_DOC_ENABLE
#endif  // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (inputBuffer.head + len*sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2; // / sizeof(float);

        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<samplesTillEnd; ++k)
        {   // convert to float
#if ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLSDR_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLSDR_AGC_ENABLE > 0)

#if (RTLSDR_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLSDR_DOC_ENABLE
#endif  // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k=0; k<len-samplesTillEnd; ++k)
        {   // convert to float
#if ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLSDR_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLSDR_AGC_ENABLE > 0)

#if (RTLSDR_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLSDR_DOC_ENABLE
#endif  // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);
    }

#if (RTLSDR_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    m_dcI = sumI * m_doc_c / (len >> 1) + dcI - m_doc_c * dcI;
    m_dcQ = sumQ * m_doc_c / (len >> 1) + dcQ - m_doc_c * dcQ;
#endif

#if (RTLSDR_AGC_ENABLE > 0)
    // store memory
    m_agcLevel = agcLev;

    emit agcLevel(agcLev);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

