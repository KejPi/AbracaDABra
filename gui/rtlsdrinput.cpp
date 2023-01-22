#include <QDir>
#include <QDebug>
#include "rtlsdrinput.h"

RtlSdrInput::RtlSdrInput(QObject *parent) : InputDevice(parent)
{
    m_id = InputDeviceId::RTLSDR;

    m_device = nullptr;
    m_deviceUnpluggedFlag = true;
    m_deviceRunningFlag = false;
    m_gainList = nullptr;
    m_dumpFile = nullptr;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &RtlSdrInput::onWatchdogTimeout);
}

RtlSdrInput::~RtlSdrInput()
{
    stop();
    if (!m_deviceUnpluggedFlag)
    {
        rtlsdr_close(m_device);
    }
    if (nullptr != m_gainList)
    {
        delete m_gainList;
    }
}

void RtlSdrInput::tune(uint32_t frequency)
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

bool RtlSdrInput::openDevice()
{
    int ret = 0;

    // Get all devices
    uint32_t deviceCount = rtlsdr_get_device_count();
    if (deviceCount == 0)
    {
        qDebug() << "RTL-SDR: No devices found";
        return false;
    }
    else
    {
        qDebug() << "RTL-SDR: Found" << deviceCount << "devices";
    }

    //	Iterate over all found rtl-sdr devices and try to open it. Stops if one device is successfull opened.
    for(uint32_t n=0; n<deviceCount; ++n)
    {
        ret = rtlsdr_open(&m_device, n);
        if (ret >= 0)
        {
            qDebug() << "RTL-SDR: Opening rtl-sdr device" << n;
            break;
        }
    }

    if (ret < 0)
    {
        qDebug() << "RTL-SDR: Opening rtl-sdr failed";
        return false;
    }

    // Set sample rate
    ret = rtlsdr_set_sample_rate(m_device, 2048000);
    if (ret < 0)
    {
        qDebug() << "RTL-SDR: Setting sample rate failed";
        return false;
    }

    // Get tuner gains
    uint32_t gainsCount = rtlsdr_get_tuner_gains(m_device, NULL);
    qDebug() << "RTL-SDR: Supported gain values" << gainsCount;
    int * gains = new int[gainsCount];
    gainsCount = rtlsdr_get_tuner_gains(m_device, gains);

    m_gainList = new QList<int>();
    for (int i = 0; i < gainsCount; i++) {
        m_gainList->append(gains[i]);
    }
    delete [] gains;

    // set automatic gain
    setGainMode(RtlGainMode::Software);

    m_deviceUnpluggedFlag = false;

    emit deviceReady();

    return true;
}

void RtlSdrInput::run()
{
    // Reset endpoint before we start reading from it (mandatory)
    if (rtlsdr_reset_buffer(m_device) < 0)
    {
        emit error(InputDeviceErrorCode::Undefined);
    }

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (m_frequency != 0)
    {   // Tune to new frequency

        rtlsdr_set_center_freq(m_device, m_frequency*1000);

        // does nothing if not SW AGC
        resetAgc();

        m_worker = new RtlSdrWorker(m_device, this);
        connect(m_worker, &RtlSdrWorker::agcLevel, this, &RtlSdrInput::onAgcLevel, Qt::QueuedConnection);
        connect(m_worker, &RtlSdrWorker::dumpedBytes, this, &InputDevice::dumpedBytes, Qt::QueuedConnection);
        connect(m_worker, &RtlSdrWorker::finished, this, &RtlSdrInput::onReadThreadStopped, Qt::QueuedConnection);
        connect(m_worker, &RtlSdrWorker::finished, m_worker, &QObject::deleteLater);
        m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);

        m_worker->start();

        m_deviceRunningFlag = true;
    }
    else
    { /* tune to 0 => going to idle */  }

    emit tuned(m_frequency);
}

void RtlSdrInput::stop()
{
    if (m_deviceRunningFlag)
    {   // if devise is running, stop worker
        m_deviceRunningFlag = false;       // this flag say that we want to stop worker intentionally
                                     // (checked in readThreadStopped() slot)

        rtlsdr_cancel_async(m_device);

        m_worker->wait(QDeadlineTimer(2000));
        while (!m_worker->isFinished())
        {
            qDebug() << "RTL-SDR: Worker thread not finished after timeout - this should not happen :-(";

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

void RtlSdrInput::setGainMode(RtlGainMode gainMode, int gainIdx)
{
    if (gainMode != m_gainMode)
    {
        // set automatic gain 0 or manual 1
        int ret = rtlsdr_set_tuner_gain_mode(m_device, (RtlGainMode::Hardware != gainMode));
        if (ret != 0)
        {
            qDebug() << "RTL-SDR: Failed to set tuner gain";
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
    // force index vaslidity
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
        int ret = rtlsdr_set_tuner_gain(m_device, m_gainList->at(m_gainIdx));
        if (ret != 0)
        {
            qDebug() << "RTL-SDR: Failed to set tuner gain";
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

void RtlSdrInput::onAgcLevel(float agcLevel, int maxVal)
{
    if (RtlGainMode::Software == m_gainMode)
    {
        // AGC correction
        if (maxVal >= 127)
        {
           setGain(m_gainIdx-1);
        }
        else if ((agcLevel < 50) && (maxVal < 100))
        {  // (maxVal < 100) is required to avoid toggling => change gain only if there is some headroom
           // this could be problem on E4000 tuner with big AGC gain steps
           setGain(m_gainIdx+1);
        }
    }
}

void RtlSdrInput::onReadThreadStopped()
{
    m_watchdogTimer.stop();
    if (m_deviceRunningFlag)
    {   // if device should be running then it measn reading error thus device is disconnected
        qDebug() << "RTL-SDR: device unplugged.";
        m_deviceUnpluggedFlag = true;
        m_deviceRunningFlag = false;

        // fill buffer (artificially to avoid blocking of the DAB processing thread)
        inputBuffer.fillDummy();

        emit error(InputDeviceErrorCode::DeviceDisconnected);
    }
    else
    {
        // in this thread was stopped by to tune to new frequency, there is no other reason to stop the thread
        run();
    }
}

void RtlSdrInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        bool isRunning = m_worker->isRunning();
        if (!isRunning)
        {  // some problem in data input
            qDebug() << "RTL-SDR: watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDeviceErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void RtlSdrInput::startDumpToFile(const QString & filename)
{
    m_dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != m_dumpFile)
    {
        m_worker->dumpToFileStart(m_dumpFile);
        emit dumpingToFile(true);
    }
}

void RtlSdrInput::stopDumpToFile()
{
    m_worker->dumpToFileStop();

    fflush(m_dumpFile);
    fclose(m_dumpFile);

    emit dumpingToFile(false);
}

void RtlSdrInput::setBW(int bw)
{
    if (bw > 0)
    {
        int ret = rtlsdr_set_tuner_bandwidth(m_device, bw);
        if (ret != 0)
        {
            qDebug() << "RTL-SDR: Failed to set tuner BW";
        }
        else
        {
            qDebug() << "RTL-SDR: bandwidth set to" << bw/1000.0 << "kHz";
        }
    }
}

void RtlSdrInput::setBiasT(bool ena)
{
    if (ena)
    {
        int ret = rtlsdr_set_bias_tee(m_device, ena);
        if (ret != 0)
        {
            qDebug() << "RTL-SDR: Failed to enable bias-T";
        }
    }
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
    m_enaDumpToFile = false;
    m_dumpFile = nullptr;
    m_rtlSdrPtr = parent;
    m_device = device;
}

void RtlSdrWorker::run()
{
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_watchdogFlag = false;  // first callback sets it to true

    rtlsdr_read_async(m_device, callback, (void*)this, 0, INPUT_CHUNK_IQ_SAMPLES*2*sizeof(uint8_t));
}

void RtlSdrWorker::dumpToFileStart(FILE * dumpFile)
{
    m_dumpFileMutex.lock();
    m_dumpFile = dumpFile;
    m_enaDumpToFile = true;
    m_dumpFileMutex.unlock();
}

void RtlSdrWorker::dumpToFileStop()
{
    m_enaDumpToFile = false;
    m_dumpFileMutex.lock();
    m_dumpFile = nullptr;
    m_dumpFileMutex.unlock();
}

void RtlSdrWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    m_dumpFileMutex.lock();
    if (nullptr != m_dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, m_dumpFile);
        emit dumpedBytes(bytes);
    }
    m_dumpFileMutex.unlock();
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
#define DC_C 0.05
#endif

#if (RTLSDR_AGC_ENABLE > 0)
    int maxVal = 0;
#define LEV_CATT 0.1
#define LEV_CREL 0.0001
#endif

    if (isDumpingIQ())
    {
        dumpBuffer(buf, len);
    }

    // reset watchDog flag, timer sets it to true
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
        qDebug() << "RTL-SDR: dropping" << len << "bytes...";
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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
    m_dcI = sumI * DC_C / (len >> 1) + dcI - DC_C * dcI;
    m_dcQ = sumQ * DC_C / (len >> 1) + dcQ - DC_C * dcQ;
#endif

#if (RTLSDR_AGC_ENABLE > 0)
    // store memory
    m_agcLevel = agcLev;

    emit agcLevel(agcLev, maxVal);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

