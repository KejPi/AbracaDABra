#include <QDir>
#include <QDebug>
#include "airspyinput.h"

#if HAVE_ARM_NEON
#include <arm_neon.h>
#endif

extern uint8_t airspy_sensitivity_vga_gains[22];
extern uint8_t airspy_sensitivity_mixer_gains[22];
extern uint8_t airspy_sensitivity_lna_gains[22];

AirspyInput::AirspyInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::AIRSPY;

    device = nullptr;
    dumpFile = nullptr;
    enaDumpToFile = false;

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
    qDebug() << Q_FUNC_INFO << freq;
    frequency = freq;
    if (airspy_is_streaming(device) || (0 == freq))
    {   // airsoy is running
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
    qDebug() << Q_FUNC_INFO;

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
    qDebug() << Q_FUNC_INFO;

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
            qDebug() << "AIRSPY: Tuner VGA gain set to" << gainIdx;
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
            qDebug() << "AIRSPY: Tuner Sensitivity gain set to" << gainIdx;
            //emit agcGain(gainList->at(gainIdx));
        }
        return;
    }
}

void AirspyInput::resetAgc()
{
    signalLevel = 0.008;

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
    if (level > 0.1)
    {
        //qDebug()  << Q_FUNC_INFO << level << "==> sensitivity down";
        setGain(gainIdx-1);
        return;
    }
    if (level < 0.001)
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

        emit dumpingToFile(true, 2*sizeof(float));
    }
}

void AirspyInput::stopDumpToFile()
{
    enaDumpToFile = false;
    fileMutex.lock();
    dumpFile = nullptr;
    fileMutex.unlock();

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
    filter->process((float*) transfer->samples, filterOutBuffer, transfer->sample_count, signalLevel);

#if (AIRSPY_AGC_ENABLE > 0)
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

AirspyDSFilter::AirspyDSFilter()
{
#if AIRSPY_FILTER_IQ_INTERLEAVED
    buffer = new ( std::align_val_t(16) ) float[4*taps];  // I Q interleaved, double size for circular buffer
    coeFull = new ( std::align_val_t(16) ) float[2*taps + 2];

//    Q_ASSERT(((uint64_t)buffer & 0x0F) == 0);
//    Q_ASSERT(((uint64_t)coeFull & 0x0F) == 0);
//    if ((nullptr == buffer) || (((uint64_t)buffer & 0x0F) != 0))
//    {
//        qDebug() << Q_FUNC_INFO << "buffer address error";
//    }
//    if ((nullptr == coeFull) || (((uint64_t)coeFull & 0x0F) != 0))
//    {
//        qDebug() << Q_FUNC_INFO << "coeFull address error";
//    }

    float * coeFullPtr = coeFull;
    for (int n = 0; n < (AIRSPY_FILTER_ORDER+2)/4-1; ++n)
    {
        *coeFullPtr++ = coef[n];
        *coeFullPtr++ = coef[n];
        *coeFullPtr++ = 0;
        *coeFullPtr++ = 0;
    }
    *coeFullPtr++ = coef[(AIRSPY_FILTER_ORDER+2)/4-1];
    *coeFullPtr++ = coef[(AIRSPY_FILTER_ORDER+2)/4-1];
    *coeFullPtr++ = coef[(AIRSPY_FILTER_ORDER+2)/4];;
    *coeFullPtr++ = coef[(AIRSPY_FILTER_ORDER+2)/4];;

    for (int n = (AIRSPY_FILTER_ORDER+2)/4-1; n >=0; --n)
    {
        *coeFullPtr++ = coef[n];
        *coeFullPtr++ = coef[n];
        *coeFullPtr++ = 0;
        *coeFullPtr++ = 0;
    }
    *coeFullPtr++ = 0;
    *coeFullPtr++ = 0;

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
    operator delete [] (buffer, std::align_val_t(16));
    operator delete [] (coeFull, std::align_val_t(16));
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

void AirspyDSFilter::process(float *inDataIQ, float *outDataIQ, int numIQ, float & signalLevel)
{
#define LEV_CATT 0.01
#define LEV_CREL 0.00001

#if AIRSPY_FILTER_IQ_INTERLEAVED
#if HAVE_ARM_NEON
    // input data must be aligned for autovectorization to work

    // prolog
    std::memcpy(buffer+2*(taps-1), inDataIQ, 2*(taps+1)*sizeof(float));

    for (int n = 0; n<2*(taps+1); n+=4)
    {

        float * fwd = buffer + n;

        //float accI = 0;
        //float accQ = 0;
        float32x4_t	accReg = vdupq_n_f32(0.0);

        for (int c = 0; c < 2*(taps + 1); c+=4)
        {
            /*
            accI += *fwd++ * coeFull[c];
            accQ += *fwd++ * coeFull[c+1];
            accI += *fwd++ * coeFull[c+2];
            accQ += *fwd++ * coeFull[c+3];
            */
            float32x4_t	inReg = vld1q_f32(fwd);
            float32x4_t	coeReg = vld1q_f32(coeFull+c);
            accReg = vmlaq_f32(accReg, inReg, coeReg);   // acc = acc + in * coe
        }

        float32x2_t res1reg = vget_high_f32(accReg);
        float32x2_t	res2reg = vget_low_f32(accReg);
        float32x2_t resReg = vadd_f32(res1reg, res2reg);

        //*outDataIQ++ = accI;
        //*outDataIQ++ = accQ;
        vst1_f32(outDataIQ, resReg);

        float accI = *outDataIQ;
        float accQ = *(outDataIQ+1);
        outDataIQ += 2;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;

        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;
#endif
    }
    // main loop
    for (int n = 2*(taps+1); n<2*numIQ; n+=4)
    {
        float * fwd = inDataIQ + n - (tapsX2 - 2);

        //float accI = 0;
        //float accQ = 0;
        float32x4_t	accReg = vdupq_n_f32(0.0);

        for (int c = 0; c < 2*(taps + 1); c+=4)
        {
            /*
            accI += *fwd++ * coeFull[c];
            accQ += *fwd++ * coeFull[c+1];
            accI += *fwd++ * coeFull[c+2];
            accQ += *fwd++ * coeFull[c+3];
            */
            float32x4_t	inReg = vld1q_f32(fwd);
            float32x4_t	coeReg = vld1q_f32(coeFull+c);
            accReg = vmlaq_f32(accReg, inReg, coeReg);   // acc = acc + in * coe
        }

        float32x2_t res1reg = vget_high_f32(accReg);
        float32x2_t	res2reg = vget_low_f32(accReg);
        float32x2_t resReg = vadd_f32(res1reg, res2reg);

        //*outDataIQ++ = accI;
        //*outDataIQ++ = accQ;
        vst1_f32(outDataIQ, resReg);

        float accI = *outDataIQ;
        float accQ = *(outDataIQ+1);
        outDataIQ += 2;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;
        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;
#endif
    }

    // epilog
    std::memcpy(buffer, inDataIQ + (2*numIQ) - (tapsX2-2), (tapsX2-2)*sizeof(float));

#else

    // input data must be aligned for autovectorization to work
#if 0
    Q_ASSERT(numIQ <= 65536);
    Q_ASSERT((uint64_t(inDataIQ) & 0x0F) == 0);
    Q_ASSERT((uint64_t(outDataIQ) & 0x0F) == 0);

    float * outPtr = outDataIQ;
    for (int n = 0; n < numIQ; n+=4)
    {
        float accI = inDataIQ[n];
        float accQ = inDataIQ[n+1];

        float abs2 = accI*accI + accQ*accQ;
        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;

        *outPtr++ = accI;
        *outPtr++ = accQ;
    }
#else
    // prolog
    std::memcpy(buffer+2*(taps-1), inDataIQ, 2*(taps+1)*sizeof(float));

    for (int n = 0; n<2*(taps+1); n+=4)
    {

        float * fwd = buffer + n;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < 2*(taps + 1); c+=4)
        {
            accI += *fwd++ * coeFull[c];
            accQ += *fwd++ * coeFull[c+1];
            accI += *fwd++ * coeFull[c+2];
            accQ += *fwd++ * coeFull[c+3];
        }

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;

        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;
#endif
    }
    // main loop
    for (int n = 2*(taps+1); n<2*numIQ; n+=4)
    {
        float * fwd = inDataIQ + n - (tapsX2 - 2);

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < 2*(taps + 1); c+=4)
        {
            accI += *fwd++ * coeFull[c];
            accQ += *fwd++ * coeFull[c+1];
            accI += *fwd++ * coeFull[c+2];
            accQ += *fwd++ * coeFull[c+3];
        }

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

#if (AIRSPY_AGC_ENABLE > 0)
        float abs2 = accI*accI + accQ*accQ;
        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;
#endif
    }

    // epilog
    std::memcpy(buffer, inDataIQ + (2*numIQ) - (tapsX2-2), (tapsX2-2)*sizeof(float));
#endif
#endif

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

        // calculate signal level (rectifier, fast attack slow release)
        float c = LEV_CREL;
        if (abs2 > signalLevel)
        {
            c = LEV_CATT;
        }
        signalLevel = c * abs2 + signalLevel - c * signalLevel;
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
