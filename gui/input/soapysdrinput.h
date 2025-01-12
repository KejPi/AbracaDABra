/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef SOAPYSDRINPUT_H
#define SOAPYSDRINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Types.hpp>

#include "inputdevice.h"
#include "inputdevicesrc.h"

#define SOAPYSDR_RECORD_INT16 1              // record raw stream in int16 instead of float
#define SOAPYSDR_RECORD_FLOAT2INT16 (32768)  // conversion constant to int16

#define SOAPYSDR_INPUT_SAMPLES (16384)

#define SOAPYSDR_LEVEL_THR_MAX (0.5)
#define SOAPYSDR_LEVEL_THR_MIN (SOAPYSDR_LEVEL_THR_MAX / 20.0)
#define SOAPYSDR_LEVEL_RESET ((SOAPYSDR_LEVEL_THR_MAX - SOAPYSDR_LEVEL_THR_MIN) / 2.0 + SOAPYSDR_LEVEL_THR_MIN)

class SoapySdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit SoapySdrWorker(SoapySDR::Device *device, double sampleRate, int rxChannel = 0, QObject *parent = nullptr);
    ~SoapySdrWorker();
    void startStopRecording(bool ena);
    bool isRunning();
    void stop();

protected:
    void run() override;
signals:
    void agcLevel(float level);
    void recordBuffer(const uint8_t *buf, uint32_t len);

private:
    SoapySDR::Device *m_device;
    int m_rxChannel;
    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_watchdogFlag;
    std::atomic<bool> m_doReadIQ;

    // SRC
    float *m_filterOutBuffer;
    InputDeviceSRC *m_src;

    // AGC memory
    float m_agcLevel = 0.0;
    uint_fast8_t m_signalLevelEmitCntr;

    void doRecordBuffer(const float *buf, uint32_t len);
    void processInputData(std::complex<float> buff[], size_t numSamples);
};

class SoapySdrInput : public InputDevice
{
    Q_OBJECT
public:
    explicit SoapySdrInput(QObject *parent = nullptr);
    ~SoapySdrInput();
    bool openDevice() override;
    void tune(uint32_t frequency) override;
    void setDevArgs(const QString &devArgs) { m_devArgs = devArgs; }
    void setRxChannel(int rxChannel) { m_rxChannel = rxChannel; }
    void setAntenna(const QString &antenna) { m_antenna = antenna; }
    void setGainMode(SoapyGainMode gainMode, int gainIdx = 0);
    void startStopRecording(bool start) override;
    void setBW(uint32_t bw);
    void setPPM(int ppm);
    QList<float> getGainList() const { return *m_gainList; }

private:
    double m_sampleRate;
    uint32_t m_frequency;
    uint32_t m_bandwidth;
    int m_ppm;
    bool m_deviceUnpluggedFlag;
    bool m_deviceRunningFlag;
    SoapySDR::Device *m_device;

    // settimgs
    QString m_devArgs;
    QString m_antenna;
    int m_rxChannel = 0;
    SoapySdrWorker *m_worker;
    QTimer m_watchdogTimer;
    SoapyGainMode m_gainMode = SoapyGainMode::Manual;
    int m_gainIdx;
    QList<float> *m_gainList;

    void run();
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gainIdx);

    // used by worker
    void onAgcLevel(float agcLevel);

    void onReadThreadStopped();
    void onWatchdogTimeout();
    double findApplicableBw(uint32_t bw) const;
};

#endif  // SOAPYSDRINPUT_H
