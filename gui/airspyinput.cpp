#include <QDir>
#include <QDebug>
#include "airspyinput.h"

AirspyInput::AirspyInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::AIRSPY;

    device = nullptr;
    dumpFile = nullptr;
    enaDumpToFile = false;
    signalLevelEmitCntr = 0;
    src = nullptr;
    filterOutBuffer = nullptr;

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

    if (nullptr != filterOutBuffer)
    {
        operator delete [] (filterOutBuffer, std::align_val_t(16));
    }
    if (nullptr != src)
    {
        delete src;
    }
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

    // Set sample rate
    uint32_t sampleRate = UINT32_MAX;
#if AIRSPY_TRY_SR_4096
    // here we try SR=4096kHz that is not oficailly supported by AirSpy devices
    // however it works on Airspy Mini
    // it seems that this sample rate leads to lower CPU load
    ret = airspy_set_samplerate(device, 4096*1000);
    if (AIRSPY_SUCCESS == ret)
    {   // succesfull
        sampleRate = 4096*1000;
        qDebug() << "AIRSPY: Sample rate set to" << sampleRate << "Hz";
    }
    else
#endif
    {   // find lowest supported sample rated that is >= 2048kHz
        uint32_t srCount;
        airspy_get_samplerates(device, &srCount, 0);
        uint32_t * srArray = new uint32_t[srCount];
        airspy_get_samplerates(device, srArray, srCount);
        for (int s = 0; s < srCount; ++s)
        {
            if ((srArray[s] >= (2048*1000)) && (srArray[s] < sampleRate))
            {
                sampleRate = srArray[s];
            }
        }
        delete [] srArray;

        // Set sample rate
        ret = airspy_set_samplerate(device, sampleRate);
        if (AIRSPY_SUCCESS != ret)
        {
            qDebug() << "AIRSPY: Setting sample rate failed";
            return false;
        }
        else
        {
            qDebug() << "AIRSPY: Sample rate set to" << sampleRate << "Hz";
        }
    }

    // airspy provides 49152 samples in callback in packed modes
    // we cannot produce more samples in SRC => allocating for worst case
    filterOutBuffer = new ( std::align_val_t(16) ) float[49152*2];
    src = new InputDeviceSRC(sampleRate);

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

    src->reset();

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
    src->resetSignalLevel(AIRSPY_LEVEL_RESET);
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

void AirspyInput::setDataPacking(bool ena)
{
    int ret = airspy_set_packing(device, ena);
    if (ret != 0)
    {
        qDebug() << "AIRSPY: Failed to set data packing";
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

    // input samples are IQ = [float float] @ 4096kHz
    // going to transform them to [float float] @ 2048kHz
    int numIQ = src->process((float*) transfer->samples, transfer->sample_count, filterOutBuffer);
    uint64_t bytesToWrite = numIQ * 2 * sizeof(float);

    if ((INPUT_FIFO_SIZE - count) < bytesToWrite)
    {
        qDebug() << Q_FUNC_INFO << "dropping" << transfer->sample_count << "IQ samples...";
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
