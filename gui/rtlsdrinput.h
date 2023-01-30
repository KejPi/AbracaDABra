/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QObject>
#include <QThread>
#include <QTimer>
#include <rtl-sdr.h>
#include "inputdevice.h"

#define RTLSDR_DOC_ENABLE  1   // enable DOC
#define RTLSDR_AGC_ENABLE  1   // enable AGC

class RtlSdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlSdrWorker(struct rtlsdr_dev *device, QObject *parent = nullptr);
    void startStopRecording(bool ena);
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
    void recordBuffer(const uint8_t * buf, uint32_t len);
private:
    QObject * m_rtlSdrPtr;
    struct rtlsdr_dev * m_device;
    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_watchdogFlag;

    // DOC memory
    float m_dcI = 0.0;
    float m_dcQ = 0.0;

    // AGC memory
    float m_agcLevel = 0.0;

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
    void setBW(int bw);
    void setBiasT(bool ena);
    QList<float> getGainList() const;

private:
    uint32_t m_frequency;
    bool m_deviceUnpluggedFlag;
    bool m_deviceRunningFlag;
    struct rtlsdr_dev * m_device;
    RtlSdrWorker * m_worker;
    QTimer m_watchdogTimer;
    RtlGainMode m_gainMode = RtlGainMode::Hardware;
    int m_gainIdx;
    QList<int> * m_gainList;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by worker
    void onAgcLevel(float agcLevel, int maxVal);

    void onReadThreadStopped();
    void onWatchdogTimeout();
};


#endif // RTLSDRINPUT_H
