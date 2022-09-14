#include <QDir>
#include <QDebug>
#include "soapysdrinput.h"

void soapysdrCb(unsigned char *buf, uint32_t len, void *ctx);

SoapySdrInput::SoapySdrInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::SOAPYSDR;

    device = nullptr;
    deviceUnplugged = true;
    deviceRunning = false;
    gainList = nullptr;
    dumpFile = nullptr;
#if (SOAPYSDR_WDOG_ENABLE)
    connect(&watchDogTimer, &QTimer::timeout, this, &SoapySdrInput::watchDogTimeout);
#endif
}

SoapySdrInput::~SoapySdrInput()
{    
    stop();
    if (nullptr != device)
    {
        SoapySDR::Device::unmake(device);
    }
    if (nullptr != gainList)
    {
        delete gainList;
    }
}

bool SoapySdrInput::openDevice()
{
    int ret = 0;

    if (devArgs.isEmpty())
    {   // no device args provided => try to open first device
        SoapySDR::KwargsList devs = SoapySDR::Device::enumerate();
        if (0 == devs.size())
        {
            qDebug() << "SOAPYSDR: No devices found";
            return false;
        }
        else
        {
            qDebug() << "SOAPYSDR: Found " << devs.size() << " devices. Using the first working one";
        }

        // iterate through th devices and open first working
        for( int n = 0; n < devs.size(); ++n)
        {
            device = SoapySDR::Device::make(devs[n]);
            if (nullptr != device)
            {
                qDebug("SOAPYSDR: Making device #%d", n);
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
        device = SoapySDR::Device::make(devArgs.toStdString());
        if (nullptr == device)
        {
            qDebug() << "SOAPYSDR: Failed to make device with args:" << devArgs;
            return false;
        }
        else { /* success */ }
    }

    if (nullptr == device)
    {
        qDebug() << "SOAPYSDR: Failed to make device";
        return false;
    }
    else { /* success */ }

    if (rxChannel >= device->getNumChannels(SOAPY_SDR_RX))
    {
        qDebug("SOAPYSDR: channel #%d not supported. Using channel 0", rxChannel);
        rxChannel = 0;
    }
    else { /* OK */ }

    // check sample format -> need CF32
    std::vector<std::string> formats = device->getStreamFormats(SOAPY_SDR_RX, rxChannel);
    if (std::find(formats.begin(), formats.end(), SOAPY_SDR_CF32) == formats.end())
    {   // not found
        qDebug() << "SOAPYSDR: Failed to open device. CF32 format not supported.";
        SoapySDR::Device::unmake(device);
        device = nullptr;
        return false;
    }
    else { /* CF32 supported */ }

    // Set sample rate - prefered rates: 2048, 4096 and then the lowest above 2048
    SoapySDR::RangeList srRanges = device->getSampleRateRange( SOAPY_SDR_RX, rxChannel);

    qDebug() << "SOAPYSDR: sample rate ranges:";
    for(int n = 0; n < srRanges.size(); ++n)
    {
        qDebug("[%g Hz - %g Hz], ", srRanges[n].minimum(), srRanges[n].maximum());
    }

    double sampleRate = 10e8;  // dummy high value
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
            if (sampleRate > srRanges[n].minimum())
            {
                sampleRate = srRanges[n].minimum();
            }
        }
        else { /* SR range is below 2048kHz */ }
    }

    // lets evaluate results
    if (has2048)
    {
        sampleRate = 2048e3;
    }
    else if (has4096)
    {
        sampleRate = 4096e3;
    }
    else { /* nether 2048 nor 4096 kHz supported - using the lowest possible */ }

    try
    {
        device->setSampleRate(SOAPY_SDR_RX, rxChannel, sampleRate);
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SOAPYSDR: Error setting sample rate" << ex.what();
        SoapySDR::Device::unmake(device);
        device = nullptr;
        return false;
    }
    qDebug() << "SOAPYSDR: Sample rate set to" << sampleRate << "Hz";

#warning "Set SRC here"
    // airspy provides 49152 samples in callback in packed modes
    // we cannot produce more samples in SRC => allocating for worst case
//    filterOutBuffer = new ( std::align_val_t(16) ) float[49152*2];
//    src = new InputDeviceSRC(sampleRate);

    // Get gains
    SoapySDR::Range gainRange = device->getGainRange(SOAPY_SDR_RX, rxChannel);

    double gainStep = gainRange.step();
    if (0.0 == gainStep)
    {   // expecting step 1
        gainStep = 1.0;
    }
    gainList = new QList<int>();
    double gain = gainRange.minimum();
    while (gain <= gainRange.maximum())
    {
        gainList->append(round(gain*10));
        gain += gainStep;
    }
    emit gainListAvailable(gainList);

    if (device->hasGainMode(SOAPY_SDR_RX, rxChannel))
    {   // set automatic gain
        setGainMode(SoapyGainMode::Hardware);
    }
    else
    {
        setGainMode(SoapyGainMode::Software);
    }

    try
    {
        stream = device->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32, std::vector<size_t>(rxChannel));
    }
    catch(const std::exception &ex)
    {
        qDebug() << "SOAPYSDR: Error setting stream" << ex.what();
        SoapySDR::Device::unmake(device);
        device = nullptr;
        return false;
    }

    if (nullptr == stream)
    {
        qDebug() << "SOAPYSDR: failed to setup stream";
        SoapySDR::Device::unmake(device);
        device = nullptr;
        return false;
    }

    deviceUnplugged = false;

    emit deviceReady();

    return true;
}

void SoapySdrInput::tune(uint32_t freq)
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

void SoapySdrInput::run()
{
    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (frequency != 0)
    {   // Tune to new frequency
        device->setFrequency(SOAPY_SDR_RX, rxChannel, frequency*1000);

        // does nothing if not SW AGC
        resetAgc();

        worker = new SoapySdrWorker(device, stream, this);
        connect(worker, &SoapySdrWorker::agcLevel, this, &SoapySdrInput::updateAgc, Qt::QueuedConnection);
        connect(worker, &SoapySdrWorker::dumpedBytes, this, &InputDevice::dumpedBytes, Qt::QueuedConnection);
        connect(worker, &SoapySdrWorker::finished, this, &SoapySdrInput::readThreadStopped, Qt::QueuedConnection);
        connect(worker, &SoapySdrWorker::finished, worker, &QObject::deleteLater);
#if (SOAPYSDR_WDOG_ENABLE)
        watchDogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
#endif
        worker->start();

        deviceRunning = true;
    }
    else
    { /* tune to 0 => going to idle */  }

    emit tuned(frequency);
}

void SoapySdrInput::stop()
{
    if (deviceRunning)
    {   // if devise is running, stop worker
        deviceRunning = false;       // this flag say that we want to stop worker intentionally
                                     // (checked in readThreadStopped() slot)
        // request to stop reading data
        worker->stop();

        // wait until thread finishes
        worker->wait(QDeadlineTimer(2000));
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

void SoapySdrInput::setGainMode(SoapyGainMode mode, int gainIdx)
{
    if (mode != gainMode)
    {
#warning "What to do when AGC not supported"

        // enable AGC for HW mode
        try
        {
            device->setGainMode(SOAPY_SDR_RX, rxChannel, (SoapyGainMode::Hardware == mode));
        }
        catch(const std::exception &ex)
        {
            qDebug() << "SOAPYSDR: Error AGC mode" << ex.what();
        }

        gainMode = mode;
    }

    if (SoapyGainMode::Manual == gainMode)
    {
        setGain(gainIdx);
    }

    if (SoapyGainMode::Hardware == gainMode)
    {   // signalize that gain is not available
        emit agcGain(INPUTDEVICE_AGC_GAIN_NA);
    }

    // does nothing in (GainMode::Software != mode)
    resetAgc();
}

void SoapySdrInput::setGain(int gIdx)
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
        device->setGain(SOAPY_SDR_RX, rxChannel, gainList->at(gainIdx));
        emit agcGain(gainList->at(gainIdx));
    }
}

void SoapySdrInput::resetAgc()
{
    if (SoapyGainMode::Software == gainMode)
    {
        setGain(gainList->size() >> 1);
    }
}

void SoapySdrInput::updateAgc(float level, int maxVal)
{
    if (SoapyGainMode::Software == gainMode)
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

void SoapySdrInput::readThreadStopped()
{
    //qDebug() << Q_FUNC_INFO << deviceRunning;
#if (SOAPYSDR_WDOG_ENABLE)
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
        // in this case thread was stopped by to tune to new frequency, there is no other reason to stop the thread
        run();
    }
}

#if (SOAPYSDR_WDOG_ENABLE)
void SoapySdrInput::watchDogTimeout()
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

void SoapySdrInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpingToFile(true);
    }
}

void SoapySdrInput::stopDumpToFile()
{
    worker->dumpToFileStop();

    fflush(dumpFile);
    fclose(dumpFile);

    emit dumpingToFile(false);
}

void SoapySdrInput::setBW(int bw)
{
    if (bw > 0)
    {
        try
        {
            device->setBandwidth(SOAPY_SDR_RX, rxChannel, bw);
        }
        catch(const std::exception &ex)
        {
            qDebug() << "SOAPYSDR: failed to set banwidth" << bw << ":" << ex.what();
            return;
        }
        qDebug() << "SOAPYSDR: bandwidth set to" << bw/1000.0 << "kHz";
    }
}

SoapySdrWorker::SoapySdrWorker(SoapySDR::Device *d, SoapySDR::Stream *s, QObject *parent) : QThread(parent)
{
    enaDumpToFile = false;
    dumpFile = nullptr;
    soapySdrPtr = parent;
    device = d;
    stream = s;
}

void SoapySdrWorker::run()
{
    //qDebug() << "SoapySdrWorker thread start" << QThread::currentThreadId();
    doReadIQ = true;

    dcI = 0.0;
    dcQ = 0.0;
    agcLev = 0.0;
    wdogIsRunningFlag = false;  // first callback sets it to true

    device->activateStream(stream);

    std::complex<float> inputBuffer[SOAPYSDR_INPUT_SAMPLES];
    void *buffs[] = {inputBuffer};

    while (doReadIQ)
    {
        int flags;
        long long time_ns;

        // read samples with timeout 100 ms
        int ret = device->readStream(stream, buffs, SOAPYSDR_INPUT_SAMPLES, flags, time_ns, 100000);

#if (SOAPYSDR_WDOG_ENABLE)
        // reset watchDog flag, timer sets it to false
        wdogIsRunningFlag = true;
#endif

        if (ret <= 0)
        {
            if (ret == SOAPY_SDR_TIMEOUT)
            {
                qDebug() << "SOARYSDR: Stream timeout";
                break;
            }
            if (ret == SOAPY_SDR_OVERFLOW)
            {
                qDebug() << "SOARYSDR: Stream overflow";
                continue;
            }
            if (ret == SOAPY_SDR_UNDERFLOW)
            {
                qDebug() << "SOARYSDR: Stream underflow";
                continue;
            }
            if (ret < 0)
            {
                qDebug() << "Unexpected stream error" << SoapySDR_errToStr(ret);
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
    device->deactivateStream(stream, 0, 0);

    // exit of the thread
    qDebug() << "SoapySdrWorker thread end" << QThread::currentThreadId();
}

void SoapySdrWorker::dumpToFileStart(FILE * f)
{
    fileMutex.lock();
    dumpFile = f;
    enaDumpToFile = true;
    fileMutex.unlock();
}

void SoapySdrWorker::dumpToFileStop()
{
    enaDumpToFile = false;
    fileMutex.lock();
    dumpFile = nullptr;
    fileMutex.unlock();
}

void SoapySdrWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

bool SoapySdrWorker::isRunning()
{
    bool flag = wdogIsRunningFlag;
    wdogIsRunningFlag = false;
    return flag;
}

void SoapySdrWorker::stop()
{
    qDebug() << Q_FUNC_INFO;
    doReadIQ = false;
}

void SoapySdrWorker::processInputData(std::complex<float> buff[], size_t numSamples)
{
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    pthread_mutex_unlock(&inputBuffer.countMutex);

    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz

#warning "SRC missing"
    //int numIQ = src->process((float*) transfer->samples, transfer->sample_count, filterOutBuffer);
    float * filterOutBuffer = (float *) buff;
    int numIQ = numSamples;

    uint64_t bytesToWrite = numIQ * 2 * sizeof(float);

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite)
    {
        qDebug() << Q_FUNC_INFO << "dropping" << numSamples << "IQ samples...";
        return;
    }

#if (AIRSPY_AGC_ENABLE > 0)
    if (0 == (++signalLevelEmitCntr & 0x07))
    {
        //qDebug() << signalLevel;
        emit agcLevel(src->signalLevel());
    }
#endif

    if (isDumpingIQ())
    {
        dumpBuffer((unsigned char *) filterOutBuffer, bytesToWrite);
    }

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

    if (bytesTillEnd >= bytesToWrite)
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *) filterOutBuffer, bytesToWrite);
        inputBuffer.head = (inputBuffer.head + bytesToWrite);
    }
    else
    {
        std::memcpy((inputBuffer.buffer + inputBuffer.head), (uint8_t *) filterOutBuffer, bytesTillEnd);
        std::memcpy((inputBuffer.buffer), ((uint8_t *) filterOutBuffer)+bytesTillEnd, bytesToWrite-bytesTillEnd);
        inputBuffer.head = bytesToWrite-bytesTillEnd;
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + bytesToWrite;
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

void soapysdrCb(unsigned char *buf, uint32_t len, void * ctx)
{
#if (SOAPYSDR_DOC_ENABLE > 0)
    int_fast32_t sumI = 0;
    int_fast32_t sumQ = 0;
#define DC_C 0.05
#endif

#if (SOAPYSDR_AGC_ENABLE > 0)
    int maxVal = 0;
#define LEV_CATT 0.1
#define LEV_CREL 0.0001
#endif

    SoapySdrWorker * soapySdrWorker = static_cast<SoapySdrWorker *>(ctx);
    if (soapySdrWorker->isDumpingIQ())
    {
        soapySdrWorker->dumpBuffer(buf, len);
    }

#if (SOAPYSDR_WDOG_ENABLE)
    // reset watchDog flag, timer sets it to true
    soapySdrWorker->wdogIsRunningFlag = true;
#endif

    // retrieving memories
#if (SOAPYSDR_DOC_ENABLE > 0)
    float dcI = soapySdrWorker->dcI;
    float dcQ = soapySdrWorker->dcQ;
#endif
#if (SOAPYSDR_AGC_ENABLE > 0)
    float agcLev = soapySdrWorker->agcLev;
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
#if ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (SOAPYSDR_AGC_ENABLE > 0)
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
#endif  // (SOAPYSDR_AGC_ENABLE > 0)

#if (SOAPYSDR_DOC_ENABLE > 0)
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
#endif  // SOAPYSDR_DOC_ENABLE
#endif  // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
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
#if ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (SOAPYSDR_AGC_ENABLE > 0)
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
#endif  // (SOAPYSDR_AGC_ENABLE > 0)

#if (SOAPYSDR_DOC_ENABLE > 0)
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
#endif  // SOAPYSDR_DOC_ENABLE
#endif  // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k=0; k<len-samplesTillEnd; ++k)
        {   // convert to float
#if ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (SOAPYSDR_AGC_ENABLE > 0)
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
#endif  // (SOAPYSDR_AGC_ENABLE > 0)

#if (SOAPYSDR_DOC_ENABLE > 0)
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
#endif  // SOAPYSDR_DOC_ENABLE
#endif  // ((SOAPYSDR_DOC_ENABLE == 0) && ((SOAPYSDR_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);
    }

#if (SOAPYSDR_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    soapySdrWorker->dcI = sumI * DC_C / (len >> 1) + dcI - DC_C * dcI;
    soapySdrWorker->dcQ = sumQ * DC_C / (len >> 1) + dcQ - DC_C * dcQ;
#endif

#if (SOAPYSDR_AGC_ENABLE > 0)
    // store memory
    soapySdrWorker->agcLev = agcLev;

    soapySdrWorker->emitAgcLevel(agcLev, maxVal);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

