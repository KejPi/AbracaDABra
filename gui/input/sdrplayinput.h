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

#ifndef SDRPLAYINPUT_H
#define SDRPLAYINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>

#include "soapysdrinput.h"

// -30 dBFS = 0.31623
#define SDRPLAY_LEVEL_THR_MIN (0.01)
#define SDRPLAY_LEVEL_THR_MAX (0.06)
#define SDRPLAY_RFGR_UP_THR (59 - 2)
#define SDRPLAY_RFGR_DOWN_THR (SDRPLAY_RFGR_UP_THR - 12)

enum class SdrPlayGainMode
{
    Software,
    Manual
};

struct SdrPlayGainStruct
{
    SdrPlayGainMode mode;
    int rfGain;
    int ifGain;
    bool ifAgcEna;
};

class SdrPlayInput : public SoapySdrInput
{
    Q_OBJECT
public:
    static InputDeviceList getDeviceList();
    static int getNumRxChannels(const QVariant &hwId);
    static QStringList getRxAntennas(const QVariant &hwId, const int channel);
    explicit SdrPlayInput(QObject *parent = nullptr);
    bool openDevice(const QVariant &hwId = QVariant(), bool fallbackConnection = true) override;
    void setGainMode(const SdrPlayGainStruct &gain);
    void setBiasT(bool ena) override;
    void setDevArgs(const QString &devArgs) = delete;
    virtual QVariant hwId() const override { return m_hwId; }
    QList<float> getRFGainList() const { return m_rfGainList; }

signals:
    void rfGain(int rfGain);
    void ifGain(int ifGain);

private:
    enum SwAgcState
    {
        Idle = -1,
        Converging = 0,
        Running
    };
    SwAgcState m_agcState = Idle;

    QVariant m_hwId;
    float m_rfGR;
    float m_ifGR;
    const float m_ifGRmin = 20;
    const float m_ifGRmax = 59;
    int m_rfGRchangeCntr;
    int m_levelEmitCntr;
    bool m_biasT;

    QList<float> m_rfGainList;
    const QHash<QString, QList<float>> m_rfGainMap;

    SdrPlayGainMode m_gainMode = SdrPlayGainMode::Manual;
    bool m_ifAgcEna = false;

    void resetAgc() override;
    void setRFGR(int gain);
    float getRFGain() const { return m_rfGainList.at(m_rfGainList.size() - 1 - m_rfGR); }
    void setIFGR(int gain);

    // used by worker
    void onAgcLevel(float agcLevel) override;
};

#endif  // SDRPLAYINPUT_H
