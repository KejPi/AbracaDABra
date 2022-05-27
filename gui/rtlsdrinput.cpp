#include <QDir>
#include <QDebug>
//#include <QDeadlineTimer>
#include "rtlsdrinput.h"

void rtlsdrCb(unsigned char *buf, uint32_t len, void *ctx);

RtlSdrInput::RtlSdrInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::RTLSDR;

    device = nullptr;
    deviceUnplugged = true;
    deviceRunning = false;
    gainList = nullptr;
    dumpFile = nullptr;
#if (RTLSDR_WDOG_ENABLE)
    connect(&watchDogTimer, &QTimer::timeout, this, &RtlSdrInput::watchDogTimeout);
#endif
}

RtlSdrInput::~RtlSdrInput()
{
    //qDebug() << Q_FUNC_INFO;
    stop();
    if (!deviceUnplugged)
    {
        rtlsdr_close(device);
    }
    if (nullptr != gainList)
    {
        delete gainList;
    }
}

void RtlSdrInput::tune(uint32_t freq)
{
    frequency = freq;
    if ((deviceRunning) || (0 == freq))
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
        qDebug() << "RTLSDR: No devices found";
        return false;
    }
    else
    {
        qDebug() << "RTLSDR: Found " << deviceCount << " devices. Uses the first working one";
    }

    //	Iterate over all found rtl-sdr devices and try to open it. Stops if one device is successfull opened.
    for(uint32_t n=0; n<deviceCount; ++n)
    {
        ret = rtlsdr_open(&device, n);
        if (ret >= 0)
        {
            qDebug() << "RTLSDR:  Opening rtl-sdr device" << n;
            break;
        }
    }

    if (ret < 0)
    {
        qDebug() << "RTLSDR: Opening rtl-sdr failed";
        return false;
    }

    // Set sample rate
    ret = rtlsdr_set_sample_rate(device, 2048000);
    if (ret < 0)
    {
        qDebug() << "RTLSDR: Setting sample rate failed";
        return false;
    }

    // Get tuner gains
    uint32_t gainsCount = rtlsdr_get_tuner_gains(device, NULL);
    qDebug() << "RTL_SDR: Supported gain values" << gainsCount;
    int * gains = new int[gainsCount];
    gainsCount = rtlsdr_get_tuner_gains(device, gains);

    gainList = new QList<int>();
    for (int i = 0; i < gainsCount; i++) {
        //qDebug() << "RTL_SDR: gain " << (gains[i] / 10.0);
        gainList->append(gains[i]);
    }
    delete [] gains;
    emit gainListAvailable(gainList);

    // set automatic gain
    setGainMode(GainMode::Software);

    deviceUnplugged = false;

    emit deviceReady();

    return true;
}

void RtlSdrInput::run()
{
    // Reset endpoint before we start reading from it (mandatory)
    if (rtlsdr_reset_buffer(device) < 0)
    {
        emit error(InputDeviceErrorCode::Undefined);
    }

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (frequency != 0)
    {   // Tune to new frequency

        rtlsdr_set_center_freq(device, frequency*1000);

        // does nothing if not SW AGC
        resetAgc();

        worker = new RtlSdrWorker(device, this);
        connect(worker, &RtlSdrWorker::agcLevel, this, &RtlSdrInput::updateAgc, Qt::QueuedConnection);
        connect(worker, &RtlSdrWorker::dumpedBytes, this, &InputDevice::dumpedBytes, Qt::QueuedConnection);
        connect(worker, &RtlSdrWorker::finished, this, &RtlSdrInput::readThreadStopped, Qt::QueuedConnection);
        connect(worker, &RtlSdrWorker::finished, worker, &QObject::deleteLater);
#if (RTLSDR_WDOG_ENABLE)
        watchDogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
#endif
        worker->start();

        deviceRunning = true;
    }
    else
    { /* tune to 0 => going to idle */  }

    emit tuned(frequency);
}

void RtlSdrInput::stop()
{
    if (deviceRunning)
    {   // if devise is running, stop worker
        deviceRunning = false;       // this flag say that we want to stop worker intentionally
                                     // (checked in readThreadStopped() slot)

        rtlsdr_cancel_async(device);

        //worker->wait(QDeadlineTimer(2000));  // requires QT >=5.15
        worker->wait(2000);
        while (!worker->isFinished())
        {
            qDebug() << "Worker thread not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            worker->wait(2000);
        }
    }
    else if (0 == frequency)
    {   // going to idle
        emit tuned(0);
    }
}

void RtlSdrInput::setGainMode(GainMode mode, int gainIdx)
{
    if (mode != gainMode)
    {
        // set automatic gain 0 or manual 1
        int ret = rtlsdr_set_tuner_gain_mode(device, (GainMode::Hardware != mode));
        if (ret != 0)
        {
            qDebug() << "RTLSDR: Failed to set tuner gain";
        }

        gainMode = mode;
    }

    if (GainMode::Manual == gainMode)
    {
        setGain(gainIdx);
    }

    if (GainMode::Hardware == gainMode)
    {   // signalize that gain is not available
        emit agcGain(INPUTDEVICE_AGC_GAIN_NA);
    }

    // does nothing in (GainMode::Software != mode)
    resetAgc();
}

void RtlSdrInput::setGain(int gIdx)
{
    // force index vaslidity
    if (gIdx < 0)
    {
        gIdx = 0;
    }
    if (gIdx >= gainList->size())
    {
        gIdx = gainList->size() - 1;
    }

    if (gIdx != gainIdx)
    {
        gainIdx = gIdx;
        int ret = rtlsdr_set_tuner_gain(device, gainList->at(gainIdx));
        if (ret != 0)
        {
            qDebug() << "RTLSDR: Failed to set tuner gain";
        }
        else
        {
            //qDebug() << "RTLSDR: Tuner gain set to" << gainList->at(gainIdx)/10.0;
            emit agcGain(gainList->at(gainIdx));
        }
    }
}

void RtlSdrInput::resetAgc()
{
    if (GainMode::Software == gainMode)
    {
        setGain(gainList->size() >> 1);
    }
}

void RtlSdrInput::updateAgc(float level, int maxVal)
{
    if (GainMode::Software == gainMode)
    {
        // AGC correction
        if (maxVal >= 127)
        {
           setGain(gainIdx-1);
        }
        else if ((level < 50) && (maxVal < 100))
        {  // (maxVal < 100) is required to avoid toggling => change gain only if there is some headroom
           // this could be problem on E4000 tuner with big AGC gain steps
           setGain(gainIdx+1);
        }
    }
}

void RtlSdrInput::readThreadStopped()
{
    //qDebug() << Q_FUNC_INFO << deviceRunning;
#if (RTLSDR_WDOG_ENABLE)
    watchDogTimer.stop();
#endif

    if (deviceRunning)
    {   // if device should be running then it measn reading error thus device is disconnected
        qDebug() << "RTL-SDR is unplugged.";
        deviceUnplugged = true;
        deviceRunning = false;

        // fill buffer (artificially to avoid blocking of the DAB processing thread)
        inputBuffer.fillDummy();

        emit error(InputDeviceErrorCode::DeviceDisconnected);
    }
    else
    {
        //qDebug() << "RTL-SDR Reading thread stopped";

        // in this thread was stopped by to tune to new frequency, there is no other reason to stop the thread
        run();
    }
}

#if (RTLSDR_WDOG_ENABLE)
void RtlSdrInput::watchDogTimeout()
{
    if (nullptr != worker)
    {
        bool isRunning = worker->isRunning();
        if (!isRunning)
        {  // some problem in data input
            qDebug() << Q_FUNC_INFO << "watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDeviceErrorCode::NoDataAvailable);
        }
    }
    else
    {
        watchDogTimer.stop();
    }
}
#endif

void RtlSdrInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpingToFile(true);
    }
}

void RtlSdrInput::stopDumpToFile()
{
    worker->dumpToFileStop();

    fflush(dumpFile);
    fclose(dumpFile);

    emit dumpingToFile(false);
}

void RtlSdrInput::setBW(int bw)
{
    if (bw > 0)
    {
        int ret = rtlsdr_set_tuner_bandwidth(device, bw);
        if (ret != 0)
        {
            qDebug() << "RTLSDR: Failed to set tuner BW";
        }
        else
        {
            qDebug() << "RTLSDR: bandwidth set to" << bw/1000.0 << "kHz";
        }
    }
}

void RtlSdrInput::setBiasT(bool ena)
{
    if (ena)
    {
        int ret = rtlsdr_set_bias_tee(device, ena);
        if (ret != 0)
        {
            qDebug() << "RTLSDR: Failed to enable bias-T";
        }
    }
}

RtlSdrWorker::RtlSdrWorker(struct rtlsdr_dev *d, QObject *parent) : QThread(parent)
{
    enaDumpToFile = false;
    dumpFile = nullptr;
    rtlSdrPtr = parent;
    device = d;    
}

void RtlSdrWorker::run()
{
    //qDebug() << "RTLSDRWorker thread start" << QThread::currentThreadId();

    dcI = 0.0;
    dcQ = 0.0;
    agcLev = 0.0;
    wdogIsRunningFlag = false;  // first callback sets it to true

    rtlsdr_read_async(device, rtlsdrCb, (void*)this, 0, INPUT_CHUNK_IQ_SAMPLES*2*sizeof(uint8_t));

    //qDebug() << "RTLSDRWorker thread end" << QThread::currentThreadId();
}

void RtlSdrWorker::dumpToFileStart(FILE * f)
{
    fileMutex.lock();
    dumpFile = f;
    enaDumpToFile = true;
    fileMutex.unlock();
}

void RtlSdrWorker::dumpToFileStop()
{
    enaDumpToFile = false;
    fileMutex.lock();
    dumpFile = nullptr;
    fileMutex.unlock();
}

void RtlSdrWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

bool RtlSdrWorker::isRunning()
{
    bool flag = wdogIsRunningFlag;
    wdogIsRunningFlag = false;
    return flag;
}

void rtlsdrCb(unsigned char *buf, uint32_t len, void * ctx)
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

    RtlSdrWorker * rtlSdrWorker = static_cast<RtlSdrWorker *>(ctx);
    if (rtlSdrWorker->isDumpingIQ())
    {
        rtlSdrWorker->dumpBuffer(buf, len);
    }

#if (RTLSDR_WDOG_ENABLE)
    // reset watchDog flag, timer sets it to true
    rtlSdrWorker->wdogIsRunningFlag = true;
#endif

    // retrieving memories
#if (RTLSDR_DOC_ENABLE > 0)
    float dcI = rtlSdrWorker->dcI;
    float dcQ = rtlSdrWorker->dcQ;
#endif
#if (RTLSDR_AGC_ENABLE > 0)
    float agcLev = rtlSdrWorker->agcLev;
#endif

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    if ((INPUT_FIFO_SIZE - count) < len*sizeof(float))
    {
        qDebug() << Q_FUNC_INFO << "dropping" << len << "bytes...";
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
#endif  // RTLSDR_DOC_ENABLE
#endif  // ((RTLSDR_DOC_ENABLE == 0) && ((RTLSDR_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);

#if RTLSDR_DOC_ENABLE
        //qDebug() << dcI << dcQ;
#endif
#if (RTLSDR_AGC_ENABLE > 0)
        //qDebug() << agcLev << maxVal;
#endif

    }

#if (RTLSDR_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    rtlSdrWorker->dcI = sumI * DC_C / (len >> 1) + dcI - DC_C * dcI;
    rtlSdrWorker->dcQ = sumQ * DC_C / (len >> 1) + dcQ - DC_C * dcQ;
#endif

#if (RTLSDR_AGC_ENABLE > 0)
    // store memory
    rtlSdrWorker->agcLev = agcLev;

    rtlSdrWorker->emitAgcLevel(agcLev, maxVal);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

