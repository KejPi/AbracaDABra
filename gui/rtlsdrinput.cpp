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
}

RtlSdrInput::~RtlSdrInput()
{
    qDebug() << Q_FUNC_INFO;
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
    if (deviceRunning)
    {
        stop();
    }
    else
    {
        run();
    }
}

bool RtlSdrInput::isAvailable()
{
    return (0 != rtlsdr_get_device_count());
}

void RtlSdrInput::openDevice()
{
    int ret = 0;

    // Get all devices
    uint32_t deviceCount = rtlsdr_get_device_count();
    if (deviceCount == 0)
    {
        qDebug() << "RTLSDR: No devices found";
        return;
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
        return;
    }

    // Set sample rate
    ret = rtlsdr_set_sample_rate(device, 2048000);
    if (ret < 0)
    {
        qDebug() << "RTLSDR: Setting sample rate failed";
        throw 0;
    }

//    ret = rtlsdr_set_tuner_bandwidth(device, 1536000/2);
//    if (ret < 0)
//    {
//        qDebug() << "RTLSDR: Bandwidth setting failed";
//    }

    // Get tuner gains
    uint32_t gainsCount = rtlsdr_get_tuner_gains(device, NULL);
    qDebug() << "RTL_SDR: Supported gain values" << gainsCount;
    int * gains = new int[gainsCount];
    //gains.resize(gainsCount);
    gainsCount = rtlsdr_get_tuner_gains(device, gains);

    gainList = new QList<int>();
    for (int i = 0; i < gainsCount; i++) {
        qDebug() << "RTL_SDR: gain " << (gains[i] / 10.0);
        gainList->append(gains[i]);
    }
    delete [] gains;
    emit gainListAvailable(gainList);

    // set automatic gain
    setGainMode(GainMode::Software);

    deviceUnplugged = false;

    emit deviceReady();
}

void RtlSdrInput::run()
{
    int ret;

    if(deviceUnplugged)
    {
        openDevice();
        if (deviceUnplugged)
        {
            return;
        }
    }

    // Reset endpoint before we start reading from it (mandatory)
    ret = rtlsdr_reset_buffer(device);
    if (ret < 0)
    {
        return;
    }

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (frequency != 0)
    {   // Tune to new frequency

        rtlsdr_set_center_freq(device, frequency*1000);

        // does nothing if not SW AGC
        resetAgc();

        worker = new RtlSdrWorker(device, this);
        connect(worker, &RtlSdrWorker::readExit, this, &RtlSdrInput::readThreadStopped, Qt::QueuedConnection);
        connect(worker, &RtlSdrWorker::agcLevel, this, &RtlSdrInput::updateAgc, Qt::QueuedConnection);
        connect(worker, &RtlSdrWorker::finished, worker, &QObject::deleteLater);
        worker->start();

        deviceRunning = true;
    }

    emit tuned(frequency);
    return;
}

void RtlSdrInput::stop()
{
    if (deviceRunning)
    {
        deviceRunning = false;
        qDebug() << Q_FUNC_INFO << deviceRunning;
        rtlsdr_cancel_async(device);
        //worker->wait(QDeadlineTimer(2000));  // requires QT >=5.15
        worker->wait(2000);
        while  (!worker->isFinished())
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
            qDebug() << "RTLSDR: Tuner gain set to" << gainList->at(gainIdx)/10.0;
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

void RtlSdrInput::setDAGC(bool ena)
{
    int ret = rtlsdr_set_agc_mode(device, ena);
    if (ret != 0)
    {
        qDebug() << "RTLSDR: Failed to set DAGC";
    }
    else
    {
        qDebug() << "RTLSDR: DAGC enable:" << ena;
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
    qDebug() << Q_FUNC_INFO << deviceRunning;
    if (deviceRunning)
    {
        qDebug() << "RTL-SDR is unplugged.";
        deviceUnplugged = true;
    }
    else
    {
        qDebug() << "RTL-SDR Reading thread stopped";

        // in this thread was stopped by to tune to new frequency, there is no other reason to stop the thread
        run();
    }
}

void RtlSdrInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "w");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpingToFile(true);
    }
}

void RtlSdrInput::stopDumpToFile()
{
    worker->dumpToFileStop();

    fclose(dumpFile);

    emit dumpingToFile(false);
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
    qDebug() << "RTLSDRWorker thread start" << QThread::currentThreadId();

    dcI = 0.0;
    dcQ = 0.0;
    agcLev = 0.0;

    rtlsdr_read_async(device, rtlsdrCb, (void*)this, 0, INPUT_CHUNK_IQ_SAMPLES*2*sizeof(uint8_t));

    qDebug() << "RTLSDRWorker thread end" << QThread::currentThreadId();

    emit readExit();
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
        fwrite(buf, 1, len, dumpFile);
    }
    fileMutex.unlock();
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
#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
#endif
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

#if INPUT_USE_PTHREADS
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.unlock();
#endif

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

#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    inputBuffer.active = (len == INPUT_CHUNK_SAMPLES);
    inputBuffer.countChanged.wakeAll();
    inputBuffer.mutex.unlock();
#endif
}

