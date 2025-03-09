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

#ifndef AIRSPYINPUT_H
#define AIRSPYINPUT_H

#include <libairspy/airspy.h>
#include <libairspy/airspy_commands.h>

#include <QObject>
#include <QThread>
#include <QTimer>

#include "inputdevice.h"
#include "inputdevicesrc.h"

#define AIRSPY_AGC_ENABLE 1    // enable AGC
#define AIRSPY_RECORD_INT16 1  // record raw stream in int16 instead of float

#define AIRSPY_RECORD_FLOAT2INT16 (16384 * 2)  // conversion constant to int16

#define AIRSPY_SW_AGC_MIN 0
#define AIRSPY_SW_AGC_MAX 17
#define AIRSPY_HW_AGC_MIN 0
#define AIRSPY_HW_AGC_MAX 17
#define AIRSPY_LEVEL_THR_MIN (0.001)
#define AIRSPY_LEVEL_THR_MAX (0.1)
#define AIRSPY_LEVEL_RESET (0.008)

enum class AirpyGainMode
{
    Hybrid,
    Software,
    Sensitivity,
    Manual
};

struct AirspyGainStruct
{
    AirpyGainMode mode;
    int sensitivityGainIdx;
    int lnaGainIdx;
    int mixerGainIdx;
    int ifGainIdx;
    bool lnaAgcEna;
    bool mixerAgcEna;
};

class AirspyInput : public InputDevice
{
    Q_OBJECT
public:
    static InputDeviceList getDeviceList();

    explicit AirspyInput(bool try4096kHz, QObject *parent = nullptr);
    ~AirspyInput();
    bool openDevice(const QVariant &hwId = QVariant(), bool fallbackConnection = true) override;
    void tune(uint32_t frequency) override;
    InputDevice::Capabilities capabilities() const override { return LiveStream | Recording; }
    void setGainMode(const AirspyGainStruct &gain);
    void startStopRecording(bool start) override;
    void setBiasT(bool ena) override;
    void setDataPacking(bool ena);
    virtual QVariant hwId() const override;
    virtual InputDeviceDesc deviceDesc() const override;

signals:
    void agcLevel(float level);

private:
    uint32_t m_frequency;
    bool m_biasT;
    struct airspy_device *m_device;
    QTimer m_watchdogTimer;
    AirpyGainMode m_gainMode = AirpyGainMode::Hybrid;
    int m_gainIdx;
    std::atomic<bool> m_isRecording;
    bool m_try4096kHz;
    float *m_filterOutBuffer;
    InputDeviceSRC *m_src;
    uint_fast8_t m_signalLevelEmitCntr;

    void run();
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside using setGainMode() function
    void setGain(int gIdx);

    void onAgcLevel(float level);
    void onWatchdogTimeout();

    void doRecordBuffer(const float *buf, uint32_t len);
    void processInputData(airspy_transfer *transfer);
    static int callback(airspy_transfer *transfer);
};

#endif  // AIRSPYNPUT_H
