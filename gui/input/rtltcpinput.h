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

#ifndef RTLTCPINPUT_H
#define RTLTCPINPUT_H

#include <rtl-sdr.h>

#include <QObject>
#include <QQueue>
#include <QSemaphore>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

#include "inputdevice.h"

#define RTLTCP_CHUNK_SIZE (INPUT_CHUNK_IQ_SAMPLES * 2)  // (16384 * 100)

#define RTLTCP_DOC_ENABLE 1          // enable DOC
#define RTLTCP_AGC_ENABLE 1          // enable AGC
#define RTLTCP_START_COUNTER_INIT 2  // init value of the counter used to reset buffer after tune

#define RTLTCP_AGC_LEVEL_MAX_DEFAULT 105

enum class RtlTcpCommand
{
    SET_FREQ = 0x01,
    SET_SAMPLE_RATE = 0x02,
    SET_GAIN_MODE = 0x03,
    SET_GAIN = 0x04,
    SET_FREQ_CORR = 0x05,
    SET_IF_GAIN = 0x06,
    SET_TEST_MODE = 0x07,
    SET_AGC_MODE = 0x08,
    SET_DIRECT_SAMPLING = 0x09,
    SET_OFFSET_TUNING = 0x0A,
    SET_RTL_XTAL_FREQ = 0x0B,
    SET_TUNER_XTAL_FREQ = 0x0C,
    SET_GAIN_IDX = 0x0D,
    SET_BIAS_TEE = 0x0E
};

class RtlTcpWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlTcpWorker(QTcpSocket *sock, QObject *parent = nullptr);
    ~RtlTcpWorker();
    void captureIQ(bool ena);
    void startStopRecording(bool ena);
    bool isRunning();
    void sendCommand(const RtlTcpCommand &cmd, uint32_t param);
    void quit() { m_quitFlag = true; }  // quit worker

protected:
    void run() override;
signals:
    void agcLevel(float level);
    void recordBuffer(const uint8_t *buf, uint32_t len);
    void dataReady();

private:
    QTcpSocket *socket;

    QMutex m_mutex;
    QQueue<QByteArray *> m_cmdQueue;
    std::atomic<bool> m_quitFlag;
    std::atomic<bool> m_commandFlag;
    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_enaCaptureIQ;
    std::atomic<bool> m_watchdogFlag;
    std::atomic<int8_t> m_captureStartCntr;

    // DOC memory
    float m_dcI = 0.0;
    float m_dcQ = 0.0;
#if (RTLTCP_DOC_ENABLE > 0)
    constexpr static const float m_doc_c = 0.05;
#endif

    // AGC memory
    int m_agcLevelCntr = 0;
    float m_agcLevel = 0.0;
#if (RTLTCP_AGC_ENABLE > 0)
    constexpr static const float m_agcLevel_catt = 0.1;
    constexpr static const float m_agcLevel_crel = 0.00005;
#endif

    // input buffer
    uint8_t m_bufferIQ[RTLTCP_CHUNK_SIZE];

    void processInputData(unsigned char *buf, uint32_t len);
};

class RtlTcpInput : public InputDevice
{
    Q_OBJECT

    /* taken from rtlsdr_get_tuner_gains() implementation */
    /* all gain values are expressed in tenths of a dB */
    static const int e4k_gains[];
    static const int e4k_gains_olddab[];
    static const int fc0012_gains[];
    static const int fc0013_gains[];
    static const int fc2580_gains[];
    static const int r82xx_gains[];
    static const int r82xx_gains_olddab[];
    static const int unknown_gains[];

public:
    explicit RtlTcpInput(QObject *parent = nullptr);
    ~RtlTcpInput();
    bool openDevice(const QVariant &hwId = QVariant()) override;
    void tune(uint32_t frequency) override;
    InputDevice::Capabilities capabilities() const override { return LiveStream | Recording; }
    void setTcpIp(const QString &address, int port, bool controlSockEna);
    void setGainMode(RtlGainMode gainMode, int gainIdx = 0);
    void setAgcLevelMax(float agcLevelMax);
    void setPPM(int ppm) override;
    void setDAGC(bool ena);
    void startStopRecording(bool start) override;
    QList<float> getGainList() const;

private:
    uint32_t m_frequency;
    QTcpSocket *m_streamSocket;
    QTcpSocket *m_controlSocket;
    bool m_controlSocketEna;
    bool m_haveControlSocket;
    QString m_address;
    int m_port;

    RtlTcpWorker *m_worker;
    QTimer m_watchdogTimer;
    RtlGainMode m_gainMode = RtlGainMode::Undefined;
    int m_gainIdx;
    QList<int> *m_gainList;
    float m_agcLevelMax;
    float m_agcLevelMin;
    QList<float> *m_agcLevelMinFactorList;
    int m_levelCalcCntr;
    int m_ppm;
    float m_deviceGain = 0.0;

    void resetAgc();

    // private function
    // gain is set from outside using setGainMode() function
    void setGain(int gainIdx);

    // used by worker
    void onAgcLevel(float agcLevel);

    void onReadThreadStopped();
    void onWatchdogTimeout();

    void initControlSocket();
    void readControlSocketData();
};

#endif  // RTLTCPINPUT_H
