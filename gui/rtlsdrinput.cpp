#include <QDir>
#include <QDebug>
//#include <QDeadlineTimer>
#include "rtlsdrinput.h"

void rtlsdrCb(unsigned char *buf, uint32_t len, void *ctx);

RtlSdrInput::RtlSdrInput(QObject *parent) : InputDevice(parent)
{
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
    setGainAutoMode(true);

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

        worker = new RtlSdrWorker(device, this);
        connect(worker, &RtlSdrWorker::readExit, this, &RtlSdrInput::readThreadStopped, Qt::QueuedConnection);
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

void RtlSdrInput::setGainAutoMode(bool enable)
{
    gainAutoMode = enable;
    // set automatic gain 0 or manual 1
    int ret = rtlsdr_set_tuner_gain_mode(device, (false == enable));
    if (ret != 0)
    {
        qDebug() << "RTLSDR: Failed to set tuner gain";
    }
    else
    {
        qDebug() << "RTLSDR: Tuner gain auto mode" << enable;
    }
}

void RtlSdrInput::setGain(int gainVal)
{
    if (gainAutoMode)
    {   // set to manual mode
        setGainAutoMode(false);
    }
    int ret = rtlsdr_set_tuner_gain(device, gainVal);
    if (ret != 0)
    {
        qDebug() << "RTLSDR: Failed to set tuner gain";
    }
    else
    {
        qDebug() << "RTLSDR: Tuner gain set to" << gainVal/10.0;
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

void RtlSdrInput::dumpToFileStart(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "w");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpToFileState(true);
    }
}

void RtlSdrInput::dumpToFileStop()
{
    worker->dumpToFileStop();

    fclose(dumpFile);

    emit dumpToFileState(false);
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
#define DOC_ENABLE 2
#if (DOC_ENABLE == 1)
    static float dcI = 0.0;
    static float Izm1 = 0.0;
    static float dcQ = 0.0;
    static float Qzm1 = 0.0;
#define DC_C 1e-6
#endif
#if (DOC_ENABLE == 2)
    static float dcI = 0.0;
    static float dcQ = 0.0;
    int_fast32_t sumI = 0;
    int_fast32_t sumQ = 0;
#define DC_C 0.05
#endif

    if (nullptr != ctx)
    {
        RtlSdrWorker * rtlSdrWorker = static_cast<RtlSdrWorker *>(ctx);
        if (rtlSdrWorker->isDumpingIQ())
        {
            rtlSdrWorker->dumpBuffer(buf, len);
        }
    }

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
    }

    // input samples are IQ = [uint8_t uint8_t]
    // going to transform them to [float float] = float _Complex
    // on euint8_t will be transformed to one float

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    uint8_t * inPtr = buf;
    if (bytesTillEnd >= len*sizeof(float))
    {
        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<len; k++)
        {   // convert to float
#if (DOC_ENABLE == 0)
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else
#if (DOC_ENABLE == 1)

            float tmp = float(*inPtr++ - 128);  // I or Q
            if (k & 0x1)
            {  // Q
                dcQ = (tmp + Qzm1)*DC_C + dcQ - 2*DC_C*dcQ;
                Qzm1 = tmp;
                *outPtr++ = tmp - dcQ;
            }
            else
            {  // I
                dcI = (tmp + Izm1)*DC_C + dcI - 2*DC_C*dcI;
                Izm1 = tmp;
                *outPtr++ = tmp - dcI;
            }
#else
#if (DOC_ENABLE == 2)
            int_fast8_t tmp = *inPtr++ - 128; // I or Q
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
#endif
#endif
#endif
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
#if (DOC_ENABLE == 0)
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else
#if (DOC_ENABLE == 1)
            float tmp = float(*inPtr++ - 128);  // I or Q
            if (k & 0x1)
            {  // Q
                dcQ = (tmp + Qzm1)*DC_C + dcQ - 2*DC_C*dcQ;
                Qzm1 = tmp;
                *outPtr++ = tmp - dcQ;
            }
            else
            {  // I
                dcI = (tmp + Izm1)*DC_C + dcI - 2*DC_C*dcI;
                Izm1 = tmp;
                *outPtr++ = tmp - dcI;
            }
#else
#if (DOC_ENABLE == 2)
            int_fast8_t tmp = *inPtr++ - 128; // I or Q
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
#endif
#endif
#endif
        }
        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k=0; k<len-samplesTillEnd; ++k)
        {   // convert to float
#if (DOC_ENABLE == 0)
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else
#if (DOC_ENABLE == 1)
            float tmp = float(*inPtr++ - 128);  // I or Q
            if (k & 0x1)
            {  // Q
                dcQ = (tmp + Qzm1)*DC_C + dcQ - 2*DC_C*dcQ;
                Qzm1 = tmp;
                *outPtr++ = tmp - dcQ;
            }
            else
            {  // I
                dcI = (tmp + Izm1)*DC_C + dcI - 2*DC_C*dcI;
                Izm1 = tmp;
                *outPtr++ = tmp - dcI;
            }
#else
#if (DOC_ENABLE == 2)
            int_fast8_t tmp = *inPtr++ - 128; // I or Q
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
#endif
#endif
#endif
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);
#if DOC_ENABLE
        //qDebug() << dcI << dcQ;
#endif
    }

#if (DOC_ENABLE == 2)
    //dcI = sumI * 1.0 / (len >> 1);
    //dcQ = sumQ * 1.0 / (len >> 1);
    dcI = sumI * DC_C / (len >> 1) + dcI - DC_C * dcI;
    dcQ = sumQ * DC_C / (len >> 1) + dcQ - DC_C * dcQ;
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

