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

#ifndef RTLSDRINPUT_H
#define RTLSDRINPUT_H

#include <rtl-sdr.h>

#include <QObject>
#include <QThread>
#include <QTimer>

#include "inputdevice.h"

#define RTLSDR_DOC_ENABLE 1  // enable DOC
#define RTLSDR_AGC_ENABLE 1  // enable AGC

// init value of the counter used to reset buffer after tune
// RTLSDR_START_COUNTER_INIT-1 buffers will be discarded
#define RTLSDR_RESTART_COUNTER 2

#define RTLSDR_AGC_LEVEL_MAX_DEFAULT 105

class RtlSdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlSdrWorker(struct rtlsdr_dev *device, QObject *parent = nullptr);
    void startStopRecording(bool ena);
    bool isRunning();
    void restart();

protected:
    void run() override;
signals:
    void agcLevel(float level);
    void recordBuffer(const uint8_t *buf, uint32_t len);
    void dataReady();

private:
    QObject *m_rtlSdrPtr;
    struct rtlsdr_dev *m_device;
    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_watchdogFlag;
    std::atomic<int8_t> m_captureStartCntr;

    // DOC memory
    float m_dcI = 0.0;
    float m_dcQ = 0.0;
#if (RTLSDR_DOC_ENABLE > 0)
    constexpr static const float m_doc_c = 0.05;
#endif

    // AGC memory
    float m_agcLevel = 0.0;
#if (RTLSDR_AGC_ENABLE > 0)
    constexpr static const float m_agcLevel_catt = 0.1;
    constexpr static const float m_agcLevel_crel = 0.00005;
#endif

    void processInputData(unsigned char *buf, uint32_t len);
    static void callback(unsigned char *buf, uint32_t len, void *ctx);
};

class RtlSdrInput : public InputDevice
{
    Q_OBJECT
public:
    explicit RtlSdrInput(QObject *parent = nullptr);
    ~RtlSdrInput();
    bool openDevice() override;
    void tune(uint32_t frequency) override;
    void setGainMode(RtlGainMode gainMode, int gainIdx = 0);
    void startStopRecording(bool start) override;
    void setBW(uint32_t bw);
    void setBiasT(bool ena);
    void setPPM(int ppm);
    void setAgcLevelMax(float agcMaxValue);
    QList<float> getGainList() const;

private:
    uint32_t m_frequency;
    uint32_t m_bandwidth;
    bool m_biasT;
    int m_ppm;
    struct rtlsdr_dev *m_device;
    RtlSdrWorker *m_worker;
    QTimer m_watchdogTimer;
    RtlGainMode m_gainMode = RtlGainMode::Hardware;
    int m_gainIdx;
    QList<int> *m_gainList;
    float m_agcLevelMax;
    float m_agcLevelMin;
    QList<float> *m_agcLevelMinFactorList;
    int m_levelReadCntr;
    constexpr static const float m_20log10[128] = {
        // precalculated 20*log10(0..127)
        NAN,       0.000000,  6.020600,  9.542425,  12.041200, 13.979400, 15.563025, 16.901961, 18.061800, 19.084850, 20.000000, 20.827854, 21.583625,
        22.278867, 22.922561, 23.521825, 24.082400, 24.608978, 25.105450, 25.575072, 26.020600, 26.444386, 26.848454, 27.234557, 27.604225, 27.958800,
        28.299467, 28.627275, 28.943161, 29.247960, 29.542425, 29.827234, 30.103000, 30.370279, 30.629578, 30.881361, 31.126050, 31.364034, 31.595672,
        31.821292, 32.041200, 32.255677, 32.464986, 32.669369, 32.869054, 33.064250, 33.255157, 33.441957, 33.624825, 33.803922, 33.979400, 34.151404,
        34.320067, 34.485517, 34.647875, 34.807254, 34.963761, 35.117497, 35.268560, 35.417040, 35.563025, 35.706597, 35.847834, 35.986811, 36.123599,
        36.258267, 36.390879, 36.521496, 36.650178, 36.776982, 36.901961, 37.025167, 37.146650, 37.266457, 37.384634, 37.501225, 37.616272, 37.729815,
        37.841892, 37.952542, 38.061800, 38.169700, 38.276277, 38.381562, 38.485586, 38.588379, 38.689969, 38.790385, 38.889653, 38.987800, 39.084850,
        39.180828, 39.275757, 39.369659, 39.462557, 39.554472, 39.645425, 39.735435, 39.824522, 39.912704, 40.000000, 40.086427, 40.172003, 40.256744,
        40.340667, 40.423786, 40.506117, 40.587676, 40.668475, 40.748530, 40.827854, 40.906460, 40.984360, 41.061569, 41.138097, 41.213957, 41.289160,
        41.363717, 41.437640, 41.510939, 41.583625, 41.655707, 41.727197, 41.798102, 41.868434, 41.938200, 42.007411, 42.076074};

    void run();
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by worker
    void onAgcLevel(float agcLevel);

    void onReadThreadStopped();
    void onWatchdogTimeout();
};

#endif  // RTLSDRINPUT_H
