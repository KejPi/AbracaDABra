#include <QDir>
#include <QDebug>
#include "airspyinput.h"

AirspyInput::AirspyInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::AIRSPY;

    device = nullptr;
    dumpFile = nullptr;
#if AIRSPY_WORKER
    for (int n = 0; n < 4; ++n)
    {
        //inBuffer[n] = new float[65536*2];
        inBuffer[n] = new ( std::align_val_t(16) ) float[65536*2];
    }
    bufferIdx = 0;

    worker = new AirspyWorker();
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &AirspyInput::doInputDataProcessing, worker, &AirspyWorker::processInputData, Qt::QueuedConnection);
    connect(this, &AirspyInput::doReset, worker, &AirspyWorker::reset, Qt::QueuedConnection);
    connect(this, &AirspyInput::doReset, this, &AirspyInput::resetAgc);
    connect(worker, &AirspyWorker::agcLevel, this, &AirspyInput::updateAgc, Qt::QueuedConnection);
    workerThread.start();
#else
    filterOutBuffer = new float[65536];
    filter = new AirspyDSFilter();

    connect(this, &AirspyInput::agcLevel, this, &AirspyInput::updateAgc, Qt::QueuedConnection);
#endif

#if (AIRSPY_WDOG_ENABLE)
    connect(&watchDogTimer, &QTimer::timeout, this, &AirspyInput::watchDogTimeout);
#endif
}

AirspyInput::~AirspyInput()
{
    //qDebug() << Q_FUNC_INFO;
    if (nullptr != device)
    {
        stop();
        airspy_close(device);
        airspy_exit();
    }

#if AIRSPY_WORKER
    workerThread.quit();  // this deletes worker
    workerThread.wait();

    for (int n = 0; n < 4; ++n)
    {
        //delete [] inBuffer[n];
        operator delete [] (inBuffer[n], std::align_val_t{16});
    }
#else
    delete [] filterOutBuffer;
    delete filter;
#endif
}

void AirspyInput::tune(uint32_t freq)
{
    qDebug() << Q_FUNC_INFO << freq;
    frequency = freq;
    if (airspy_is_streaming(device) || (0 == freq))
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

bool AirspyInput::openDevice()
{
    int ret = 0;

    // open first device
    ret = airspy_open(&device);
    if (AIRSPY_SUCCESS != ret)
    {
        qDebug() << "AIRSPY:  Failed opening device";
        device = nullptr;
        return false;
    }

    // set sample type
    ret = airspy_set_sample_type(device, AIRSPY_SAMPLE_FLOAT32_IQ);
    if (AIRSPY_SUCCESS != ret)
    {
        qDebug() << "AIRSPY:  Cannot set sample format";
        return false;
    }

    airspy_set_packing(device, 1);

    // Set sample rate
    ret = airspy_set_samplerate(device, 4096000);
    if (AIRSPY_SUCCESS != ret)
    {
        qDebug() << "AIRSPY: Setting sample rate failed";
        return false;
    }

    // set automatic gain
    setGainMode(GainMode::Software);
    //setGainMode(GainMode::Hardware);

    emit deviceReady();

    return true;
}

void AirspyInput::run()
{
    qDebug() << Q_FUNC_INFO;

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();
#if !AIRSPY_WORKER
    filter->reset();
#endif

    if (frequency != 0)
    {   // Tune to new frequency

        int ret = airspy_set_freq(device, frequency*1000);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug("AIRSPY: Tune to %d kHz failed", frequency);
            emit error(InputDeviceErrorCode::DeviceDisconnected);
            return;
        }

#if AIRSPY_WORKER
        emit doReset();
#else
        resetAgc();
#endif

#if (AIRSPY_WDOG_ENABLE)
        watchDogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
#endif
        ret = airspy_start_rx(device, AirspyInput::callback, (void*)this);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug("AIRSPY: Failed to start RX");
            emit error(InputDeviceErrorCode::DeviceDisconnected);
            return;
        }
    }
    else
    { /* tune to 0 => going to idle */  }

    emit tuned(frequency);
}

void AirspyInput::stop()
{
    qDebug() << Q_FUNC_INFO;

    if (AIRSPY_TRUE == airspy_is_streaming(device))
    {   // if devise is running, stop worker
        airspy_stop_rx(device);

#if (AIRSPY_WDOG_ENABLE)
        watchDogTimer.stop();
#endif

        QThread::msleep(50);
        while (AIRSPY_TRUE == airspy_is_streaming(device))
        {
            qDebug() << "Airspy not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            QThread::msleep(2000);
        }

        run(); // restart
    }
    else if (0 == frequency)
    {   // going to idle
        emit tuned(0);
    }
}

void AirspyInput::setGainMode(GainMode mode, int lnaIdx, int mixerIdx, int ifIdx)
{
    switch (mode)
    {
    case GainMode::Hardware:
        if (gainMode == mode)
        {   // do nothing -> mode does not change
            break;
        }
        // mode changes
        gainMode = mode;
        airspy_set_lna_agc(device, 1);
        airspy_set_mixer_agc(device, 1);
        resetAgc();
        break;
    case GainMode::Manual:
        gainMode = mode;
        airspy_set_vga_gain(device, ifIdx);
        if (lnaIdx < 0)
        {
            airspy_set_lna_agc(device, 1);
        }
        else
        {
            airspy_set_lna_agc(device, 0);
            airspy_set_lna_gain(device, lnaIdx);
        }
        if (mixerIdx < 0)
        {
            airspy_set_mixer_agc(device, 1);
        }
        else
        {
            airspy_set_mixer_agc(device, 0);
            airspy_set_mixer_gain(device, mixerIdx);
        }
        break;
    case GainMode::Software:
        if (gainMode == mode)
        {   // do nothing -> mode does not change
            break;
        }
        gainMode = mode;
        resetAgc();
        break;
    }
}

void AirspyInput::setGain(int gIdx)
{
    if (GainMode::Hardware == gainMode)
    {
        if (gIdx < AIRSPY_HW_AGC_MIN)
        {
            gIdx = AIRSPY_HW_AGC_MIN;
        }
        if (gIdx > AIRSPY_HW_AGC_MAX)
        {
            gIdx = AIRSPY_HW_AGC_MAX;
        }

        if (gIdx == gainIdx)
        {
            return;
        }
        // else
        gainIdx = gIdx;

        int ret = airspy_set_vga_gain(device, gainIdx);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug() << "AIRSPY: Failed to set tuner gain";
        }
        else
        {
            qDebug() << "AIRSPY: Tuner VGA gain set to" << gainIdx;
            //emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
    if (GainMode::Software == gainMode)
    {
        if (gIdx < AIRSPY_SW_AGC_MIN)
        {
            gIdx = AIRSPY_SW_AGC_MIN;
        }
        if (gIdx > AIRSPY_SW_AGC_MAX)
        {
            gIdx = AIRSPY_SW_AGC_MAX;
        }

        if (gIdx == gainIdx)
        {
            return;
        }
        // else
        gainIdx = gIdx;

        int ret = airspy_set_sensitivity_gain(device, gainIdx);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug() << "AIRSPY: Failed to set tuner gain";
        }
        else
        {
            qDebug() << "AIRSPY: Tuner Sensitivity gain set to" << gainIdx;
            //emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
}

void AirspyInput::resetAgc()
{
#if !AIRSPY_WORKER
    signalLevel = 0.008;
#endif
    if (GainMode::Software == gainMode)
    {
        gainIdx = -1;
        setGain((AIRSPY_SW_AGC_MAX+1)/2); // set it to the middle
        return;
    }
    if (GainMode::Hardware == gainMode)
    {
        gainIdx = -1;
        setGain(6);
    }
}

void AirspyInput::updateAgc(float level)
{
    //qDebug() << Q_FUNC_INFO << level;
    if (level > 0.1)
    {
        //qDebug()  << Q_FUNC_INFO << level << "==> sensitivity down";
        setGain(gainIdx-1);
        return;
    }
    if (level < 0.005)
    {
        //qDebug()  << Q_FUNC_INFO << level << "==> sensitivity up";
        setGain(gainIdx+1);
    }
}

#if (AIRSPY_WDOG_ENABLE)
void AirspyInput::watchDogTimeout()
{
    if (AIRSPY_TRUE != airspy_is_streaming(device))
    {
        qDebug() << Q_FUNC_INFO << "watchdog timeout";
        inputBuffer.fillDummy();
        emit error(InputDeviceErrorCode::NoDataAvailable);
    }
}
#endif

void AirspyInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != dumpFile)
    {
#if AIRSPY_WORKER
        worker->dumpToFileStart(dumpFile);
#else
        fileMutex.lock();
        enaDumpToFile = true;
        fileMutex.unlock();
#endif
        emit dumpingToFile(true, 2*sizeof(float));
    }
}

void AirspyInput::stopDumpToFile()
{
#if AIRSPY_WORKER
    worker->dumpToFileStop();
#else
    enaDumpToFile = false;
    fileMutex.lock();
    dumpFile = nullptr;
    fileMutex.unlock();
#endif
    fflush(dumpFile);
    fclose(dumpFile);

    emit dumpingToFile(false);
}

void AirspyInput::setBiasT(bool ena)
{
    if (ena)
    {
        int ret = airspy_set_rf_bias(device, ena);
        if (ret != 0)
        {
            qDebug() << "AIRSPY: Failed to enable bias-T";
        }
    }
}

#if !AIRSPY_WORKER
void AirspyInput::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}
#endif

int AirspyInput::callback(airspy_transfer* transfer)
{
    AirspyInput * thisPtr = static_cast<AirspyInput *>(transfer->ctx);
    thisPtr->processInputData(transfer);
    return 0;
}

void AirspyInput::processInputData(airspy_transfer *transfer)
{    
    //qDebug() << Q_FUNC_INFO << transfer->sample_count;

    if (transfer->dropped_samples > 0)
    {
        qDebug() << "AIRSPY: dropping" << transfer->dropped_samples << "samples";
    }

#if AIRSPY_WORKER
    float * inBufferPtr = inBuffer[bufferIdx++];
    bufferIdx &= 0x03;

    std::memcpy(inBufferPtr, transfer->samples, transfer->sample_count*2*sizeof(float));
    emit doInputDataProcessing(inBufferPtr, transfer->sample_count);
#else
    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    uint64_t bytesToWrite = transfer->sample_count*sizeof(float); //*2/2 (considering downsampling by 2)

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite) //*2/2 (considering downsampling by 2)
    {
        qDebug() << Q_FUNC_INFO << "dropping" << transfer->sample_count << "IQ samples...";
        return;
    }

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz
    float maxAbs2;
    filter->process((float*) transfer->samples, filterOutBuffer, transfer->sample_count, maxAbs2);

#if (AIRSPY_AGC_ENABLE > 0)
#define LEV_C 0.01
    // calculate signal level (rectifier, fast attack slow release)
    signalLevel = LEV_C * maxAbs2 + signalLevel - LEV_C * signalLevel;

    static uint_fast8_t cntr = 0;
    if (0 == (++cntr & 0x07))
    {
        //qDebug() << signalLevel;
        emit agcLevel(signalLevel);
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
#endif
}

AirspyDSFilter::AirspyDSFilter()
{
#if AIRSPY_FILTER_IQ_INTERLEAVED
    buffer = new float[4*taps];  // I Q interleaved, double size for circular buffer
    bufferPtr = buffer;
#else
    bufferI = new float[2*taps];
    bufferQ = new float[2*taps];
    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;
#endif
    reset();
}

AirspyDSFilter::~AirspyDSFilter()
{
#if AIRSPY_FILTER_IQ_INTERLEAVED
    delete [] buffer;
#else
    delete [] bufferI;
    delete [] bufferQ;
#endif
}

void AirspyDSFilter::reset()
{
#if AIRSPY_FILTER_IQ_INTERLEAVED
    bufferPtr = buffer;
    for (int n = 0; n < 4*taps; ++n)
    {
        buffer[n] = 0;
    }
#else
    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;
    for (int n = 0; n < 2*taps; ++n)
    {
        bufferI[n] = 0;
    }
    for (int n = 0; n < 2*taps; ++n)
    {
        bufferQ[n] = 0;
    }
#endif
}

void AirspyDSFilter::process(float *inDataIQ, float *outDataIQ, int numIQ, float & maxAbs2)
{

#if (AIRSPY_AGC_ENABLE > 0)
    maxAbs2 = 0;
#endif
#if AIRSPY_FILTER_IQ_INTERLEAVED
    //Q_ASSERT(numIQ > 4*len);

    //    if (0 != (uint64_t(inDataIQ) & 0x00F))
    //    {
    //        qDebug("Data not aligned %8.8X", inDataIQ);
    //    }

    // prolog
    std::memcpy(buffer+tapsX2-2, inDataIQ, (tapsX2+2)*sizeof(float));

    for (int n = 0; n<taps+1; n+=2)
    {
        float * fwd = buffer + 2*n;
        float * rev = buffer + 2*n + tapsX2 - 2 + 1;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (taps+1)/4; ++c)
        {
            accI += *fwd++ * coef[c];
            accQ += *rev-- * coef[c];
            accQ += *fwd++ * coef[c];
            accI += *rev-- * coef[c];

            fwd += 2;  // zero coe
            rev -= 2;  // zero coe
        }

        accI += *(fwd-2) * coef[(taps+1)/4];
        accQ += *(fwd-1) * coef[(taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;
        if (maxAbs2 < abs2)
        {
            maxAbs2 = abs2;
        }
#endif
    }

    // main loop
    for (int n = taps+1; n<numIQ; n+=2)
    {
        //float * fwd = inDataIQ + (lenX2+2) + 2*n;
        //float * rev = inDataIQ + (lenX2+2) + lenX2 - 2 + 2*n;
        float * fwd = inDataIQ + 2*n - (tapsX2 - 2);
        float * rev = inDataIQ + 2*n + 1;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (taps+1)/4; ++c)
        {
            accI += *fwd++ * coef[c];
            accQ += *rev-- * coef[c];
            accQ += *fwd++ * coef[c];
            accI += *rev-- * coef[c];

            fwd += 2;  // zero coe
            rev -= 2;  // zero coe
        }

        accI += *(fwd-2) * coef[(taps+1)/4];
        accQ += *(fwd-1) * coef[(taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;
        if (maxAbs2 < abs2)
        {
            maxAbs2 = abs2;
        }
#endif
    }

    // epilog
    std::memcpy(buffer, inDataIQ + (2*numIQ) - (tapsX2-2), (tapsX2-2)*sizeof(float));

#else
    for (int n = 0; n<numIQ/2; ++n)
    {
        float * fwdI = bufferPtrI;
        float * revI = bufferPtrI + taps;
        float * fwdQ = bufferPtrQ;
        float * revQ = bufferPtrQ + taps;


        *fwdI++ = *inDataIQ;
        *revI = *inDataIQ++;
        *fwdQ++ = *inDataIQ;
        *revQ = *inDataIQ++;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (taps+1)/4; ++c)
        {
            accI += (*fwdI + *revI)*coef[c];
            fwdI += 2;  // every other coeff is zero
            revI -= 2;  // every other coeff is zero
            accQ += (*fwdQ + *revQ)*coef[c];
            fwdQ += 2;  // every other coeff is zero
            revQ -= 2;  // every other coeff is zero
        }

        accI += *(fwdI-1) * coef[(taps+1)/4];
        accQ += *(fwdQ-1) * coef[(taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;
        if (maxAbs2 < abs2)
        {
            maxAbs2 = abs2;
        }
#endif

        bufferPtrQ += 1;
        if (++bufferPtrI == bufferI + taps)
        {
            bufferPtrI = bufferI;
            bufferPtrQ = bufferQ;
        }

        // insert new samples to delay line
        *(bufferPtrI + taps) = *inDataIQ;   // I
        *bufferPtrI++ = *inDataIQ++;       // I
        *(bufferPtrQ + taps) = *inDataIQ;   // I
        *bufferPtrQ++ = *inDataIQ++;       // I

        if (bufferPtrI == bufferI + taps)
        {
            bufferPtrI = bufferI;
            bufferPtrQ = bufferQ;
        }
    }

#endif
}

AirspyWorker::AirspyWorker(QObject *parent)
{
    filterOutBuffer = new float[65536];
    filter = new AirspyDSFilter();
    reset();
}

AirspyWorker::~AirspyWorker()
{
    delete [] filterOutBuffer;
    delete filter;
}

void AirspyWorker::dumpToFileStart(FILE * f)
{
    fileMutex.lock();
    dumpFile = f;
    enaDumpToFile = true;
    fileMutex.unlock();
}

void AirspyWorker::dumpToFileStop()
{
    enaDumpToFile = false;
    fileMutex.lock();
    dumpFile = nullptr;
    fileMutex.unlock();
}

void AirspyWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

void AirspyWorker::processInputData(float * inBufferIQ, int numIQ)
{
    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    uint64_t bytesToWrite = numIQ*sizeof(float); //*2/2 (considering downsampling by 2)

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite) //*2/2 (considering downsampling by 2)
    {
        qDebug() << Q_FUNC_INFO << "dropping" << numIQ << "IQ samples...";
        return;
    }

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz
    float maxAbs2;
    filter->process((float*) inBufferIQ, filterOutBuffer, numIQ, maxAbs2);

#if (AIRSPY_AGC_ENABLE > 0)
#define LEV_C 0.01
    // calculate signal level (rectifier, fast attack slow release)
    signalLevel = LEV_C * maxAbs2 + signalLevel - LEV_C * signalLevel;

    static uint_fast8_t cntr = 0;
    if (0 == (++cntr & 0x07))
    {
        //qDebug() << signalLevel;
        emit agcLevel(signalLevel);
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

void AirspyWorker::reset()
{
    filter->reset();
    signalLevel = 0.008;
}
