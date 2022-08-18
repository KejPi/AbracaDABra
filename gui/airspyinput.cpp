#include <QDir>
#include <QDebug>
#include "airspyinput.h"

#if HAVE_ARM_NEON
#include <arm_neon.h>
#endif

AirspyInput::AirspyInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::AIRSPY;

    device = nullptr;
    dumpFile = nullptr;
    enaDumpToFile = false;
    signalLevelEmitCntr = 0;

    filterOutBuffer = new ( std::align_val_t(16) ) float[65536];
    filter = new AirspyDSFilter();

    connect(this, &AirspyInput::agcLevel, this, &AirspyInput::updateAgc, Qt::QueuedConnection);

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

    operator delete [] (filterOutBuffer, std::align_val_t(16));
    delete filter;
}

void AirspyInput::tune(uint32_t freq)
{
    //qDebug() << Q_FUNC_INFO << freq;
    frequency = freq;
    if (airspy_is_streaming(device) || (0 == freq))
    {   // airspy is running
        //      sequence in this case is:
        //      1) stop
        //      2) wait to finish
        //      3) start new RX on new frequency
        // ==> this way we can be sure that all buffers are reset and nothing left from previous channel
        stop();
    }
    else
    {   // airspy is not running
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
    gainMode = AirpyGainMode::Software;
    resetAgc();

    emit deviceReady();

    return true;
}

void AirspyInput::run()
{
    //qDebug() << Q_FUNC_INFO;

    // Reset buffer here - airspy is not running, DAB waits for new data
    inputBuffer.reset();

    filter->reset();

    if (frequency != 0)
    {   // Tune to new frequency

        int ret = airspy_set_freq(device, frequency*1000);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug("AIRSPY: Tune to %d kHz failed", frequency);
            emit error(InputDeviceErrorCode::DeviceDisconnected);
            return;
        }

        resetAgc();

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
    //qDebug() << Q_FUNC_INFO;

    if (AIRSPY_TRUE == airspy_is_streaming(device))
    {   // if devise is running, stop RX
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

void AirspyInput::setGainMode(const AirspyGainStr &gain)
{
    switch (gain.mode)
    {
    case AirpyGainMode::Hybrid:
        if (gainMode == gain.mode)
        {   // do nothing -> mode does not change
            break;
        }
        // mode changes
        gainMode = gain.mode;
        airspy_set_lna_agc(device, 1);
        airspy_set_mixer_agc(device, 1);
        resetAgc();
        break;
    case AirpyGainMode::Manual:
        gainMode = gain.mode;
        airspy_set_vga_gain(device, gain.ifGainIdx);
        if (gain.lnaAgcEna)
        {
            airspy_set_lna_agc(device, 1);
        }
        else
        {
            airspy_set_lna_agc(device, 0);
            airspy_set_lna_gain(device, gain.lnaGainIdx);
        }
        if (gain.mixerAgcEna)
        {
            airspy_set_mixer_agc(device, 1);
        }
        else
        {
            airspy_set_mixer_agc(device, 0);
            airspy_set_mixer_gain(device, gain.mixerGainIdx);
        }
        break;
    case AirpyGainMode::Software:
        if (gainMode == gain.mode)
        {   // do nothing -> mode does not change
            break;
        }
        gainMode = gain.mode;
        resetAgc();
        break;
    case AirpyGainMode::Sensitivity:
        gainMode = gain.mode;
        airspy_set_sensitivity_gain(device, gain.sensitivityGainIdx);
        break;
    }

    emit agcGain(INPUTDEVICE_AGC_GAIN_NA);
}

void AirspyInput::setGain(int gIdx)
{
    if (AirpyGainMode::Hybrid == gainMode)
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
            //qDebug() << "AIRSPY: Tuner VGA gain set to" << gainIdx;
            //emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
    if (AirpyGainMode::Software == gainMode)
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
            //qDebug() << "AIRSPY: Tuner Sensitivity gain set to" << gainIdx;
            //emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
}

void AirspyInput::resetAgc()
{
    filter->resetSignalLevel();
    signalLevelEmitCntr = 0;

    if (AirpyGainMode::Software == gainMode)
    {
        gainIdx = -1;
        setGain((AIRSPY_SW_AGC_MAX+1)/2); // set it to the middle
        return;
    }
    if (AirpyGainMode::Hybrid == gainMode)
    {
        gainIdx = -1;
        setGain(6);
    }
}

void AirspyInput::updateAgc(float level)
{
    //qDebug() << Q_FUNC_INFO << level;
    if (level > AIRSPY_LEVEL_THR_MAX)
    {
        //qDebug()  << Q_FUNC_INFO << level << "==> sensitivity down";
        setGain(gainIdx-1);
        return;
    }
    if (level < AIRSPY_LEVEL_THR_MIN)
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
        fileMutex.lock();
        enaDumpToFile = true;
        fileMutex.unlock();
#if AIRSPY_DUMP_INT16
        emit dumpingToFile(true, 2*sizeof(int16_t));
#else
        emit dumpingToFile(true, 2*sizeof(float));
#endif
    }
}

void AirspyInput::stopDumpToFile()
{
    enaDumpToFile = false;
    fileMutex.lock();
    fflush(dumpFile);
    fclose(dumpFile);
    dumpFile = nullptr;
    fileMutex.unlock();

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

void AirspyInput::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
#if AIRSPY_DUMP_INT16
        // dumping in int16
        float * floatBuf = (float *) buf;
        int16_t int16Buf[len/sizeof(float)];
        for (int n = 0; n < len/sizeof(float); ++n)
        {
            int16Buf[n] = *floatBuf++ * AIRSPY_DUMP_FLOAT2INT16;
        }
        ssize_t bytes = fwrite(int16Buf, 1, sizeof(int16Buf), dumpFile);

#else
        // dumping in float
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
#endif
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

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

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    pthread_mutex_unlock(&inputBuffer.countMutex);

    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    uint64_t bytesToWrite = transfer->sample_count*sizeof(float); //*2/2 (considering downsampling by 2)

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite) //*2/2 (considering downsampling by 2)
    {
        qDebug() << Q_FUNC_INFO << "dropping" << transfer->sample_count << "IQ samples...";
        return;
    }

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz
    float signalLevel = filter->process((float*) transfer->samples, filterOutBuffer, transfer->sample_count);

#if (AIRSPY_AGC_ENABLE > 0)
    if (0 == (++signalLevelEmitCntr & 0x07))
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

AirspyDSFilter::AirspyDSFilter()
{
    bufferI = new float[2*taps];
    bufferQ = new float[2*taps];
    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;

    reset();
}

AirspyDSFilter::~AirspyDSFilter()
{
    delete [] bufferI;
    delete [] bufferQ;
}

void AirspyDSFilter::reset()
{
    resetSignalLevel();

    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;
    for (int n = 0; n < 2*taps; ++n)
    {
        bufferI[n] = 0;
        bufferQ[n] = 0;
    }
}

float AirspyDSFilter::process(float *inDataIQ, float *outDataIQ, int numIQ)
{
#define LEV_CATT 0.01
#define LEV_CREL 0.00001

    float level = signalLevel;

    float * bufPtrI = bufferPtrI;
    float * bufPtrQ = bufferPtrQ;

    for (int n = 0; n<numIQ/2; ++n)
    {
        float * fwdI = bufPtrI;
        float * revI = bufPtrI + taps;
        float * fwdQ = bufPtrQ;
        float * revQ = bufPtrQ + taps;


        *fwdI++ = *inDataIQ;
        *revI = *inDataIQ++;
        *fwdQ++ = *inDataIQ;
        *revQ = *inDataIQ++;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (taps+1)/4; ++c)
        {
            accI += (*fwdI + *revI) * coef[c];
            fwdI += 2;  // every other coeff is zero
            revI -= 2;  // every other coeff is zero
            accQ += (*fwdQ + *revQ) * coef[c];
            fwdQ += 2;  // every other coeff is zero
            revQ -= 2;  // every other coeff is zero
        }

        accI += *(fwdI-1) * coef[(taps+1)/4];
        accQ += *(fwdQ-1) * coef[(taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

        bufPtrQ += 1;
        if (++bufPtrI == bufferI + taps)
        {
            bufPtrI = bufferI;
            bufPtrQ = bufferQ;
        }

        // insert new samples to delay line
#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = (*inDataIQ) * (*inDataIQ);  // I*I

        // insert to delay line
        *(bufPtrI + taps) = *inDataIQ;           // I
        *bufPtrI++ = *inDataIQ++;                // I

        abs2 += (*inDataIQ) * (*inDataIQ);       // Q*Q

        // insert to delay line
        *(bufPtrQ + taps) = *inDataIQ;           // Q
        *bufPtrQ++ = *inDataIQ++;                // Q

        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > level)
        {
            c = LEV_CATT;
        }
        level = c * abs2 + level - c * level;
#else
        // insert new samples to delay line
        *(bufPtrI + taps) = *inDataIQ;    // I
        *bufPtrI++ = *inDataIQ++;         // I
        *(bufPtrQ + taps) = *inDataIQ;    // Q
        *bufPtrQ++ = *inDataIQ++;         // Q
#endif
        if (bufPtrI == bufferI + taps)
        {
            bufPtrI = bufferI;
            bufPtrQ = bufferQ;
        }
    }

    bufferPtrI = bufPtrI;
    bufferPtrQ = bufPtrQ;

    return signalLevel = level;
}
