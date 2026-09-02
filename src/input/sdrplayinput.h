/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

// Worker thread that owns the SW-AGC feedback loop (full Software AGC and the
// Manual+IF-AGC level feedback) so that blocking SoapySDR::Device::setGain()
// calls never run on the GUI thread. Manual gain (no IF-AGC) is still applied
// synchronously by SdrPlayInput on the GUI thread; this worker only takes
// over gain control while a feedback mode is active, and keeps emitting
// telemetry (rfLevel/agcGain/gainIdx/ifGain) at all times so the UI keeps
// getting periodic level updates in every gain mode.
class SdrPlayWorker : public SoapySdrWorker
{
    Q_OBJECT
public:
    enum class FeedbackMode
    {
        Off,          // plain manual gain, no feedback loop
        IfOnly,       // Manual mode + IF-AGC enabled: adjust IFGR only
        SoftwareFull  // Software AGC mode: full RFGR+IFGR state machine
    };

    explicit SdrPlayWorker(SoapySDR::Device *device, double sampleRate, int rxChannel, const QList<float> &rfGainList, QObject *parent = nullptr);

    // called from GUI thread only; lock-free, no device I/O (mirrors
    // m_captureStartCntr/m_isRecording convention used by SoapySdrWorker,
    // required because this worker's run() loop never calls QThread::exec())
    void setFeedbackMode(FeedbackMode mode);
    void requestAgcReset();
    // called by SdrPlayInput (GUI thread) after it applies a manual RFGR/IFGR
    // change directly, so the worker's telemetry (and IfOnly's fixed RFGR)
    // stays in sync with the channel not currently owned by the feedback loop
    void syncManualRfGain(int rfGR);
    void syncManualIfGain(int ifGR);

signals:
    void agcGainChanged(float gain);
    void rfLevelChanged(float level, float gain);
    void ifGainChanged(int ifGain);
    void gainIdxChanged(int idx);

protected:
    void onSignalLevel(float level) override;

private:
    enum SwAgcState
    {
        Idle = -1,
        Converging = 0,
        Running
    };
    SwAgcState m_agcState = Idle;

    const QList<float> m_rfGainList;
    // owned by this worker while its feedback mode is active for the given
    // channel (RFGR: SoftwareFull only; IFGR: SoftwareFull or IfOnly);
    // otherwise written by SdrPlayInput via syncManualRfGain()/syncManualIfGain()
    // so telemetry stays correct in every mode. atomic because writer differs
    // depending on mode (GUI thread vs this worker thread).
    std::atomic<int> m_rfGR{-1};
    std::atomic<int> m_ifGR{-1};
    const int m_ifGRmin = 20;
    const int m_ifGRmax = 59;
    int m_rfGRchangeCntr = 0;
    int m_levelEmitCntr = 0;

    std::atomic<FeedbackMode> m_feedbackMode{FeedbackMode::Off};
    std::atomic<bool> m_resetRequested{false};

    void doReset(FeedbackMode mode);
    void setRFGR(int gain);
    float getRFGain() const { return m_rfGainList.at(m_rfGainList.size() - 1 - m_rfGR.load()); }
    void setIFGR(int gain);
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
    void setAntenna(const QString &antenna) override;
    void setDevArgs(const QString &devArgs) = delete;
    virtual QVariant hwId() const override { return m_hwId; }
    QList<float> getRFGainList() const { return m_rfGainList; }
    InputDevice::Capabilities capabilities() const override { return LiveStream | Recording | RfLevel; }

signals:
    void ifGain(int ifGain);

private:
    QVariant m_hwId;
    float m_rfGR = -1;  // -1 forces first setRFGR() call to apply
    float m_ifGR = -1;  // -1 forces first setIFGR() call to apply
    const float m_ifGRmin = 20;
    const float m_ifGRmax = 59;
    bool m_biasT;

    QList<float> m_rfGainList;
    const QHash<QString, QList<float>> m_rfGainMap;

    SdrPlayGainMode m_gainMode = SdrPlayGainMode::Manual;
    bool m_ifAgcEna = false;

    void resetAgc() override;
    void setRFGR(int gain);
    float getRFGain() const { return m_rfGainList.at(m_rfGainList.size() - 1 - m_rfGR); }
    void setIFGR(int gain);

    SdrPlayWorker::FeedbackMode currentFeedbackMode() const;

    // worker factory / signal wiring hooks (SoapySdrInput)
    SoapySdrWorker *createWorker(SoapySDR::Device *device, double sampleRate, int rxChannel, QObject *parent) override;
    void connectWorkerSignals(SoapySdrWorker *worker) override;
};

#endif  // SDRPLAYINPUT_H
