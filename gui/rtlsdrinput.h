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
    void dumpToFileStart(FILE * dumpFile);
    void dumpToFileStop();
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
    void dumpedBytes(ssize_t bytes);
private:
    QObject * m_rtlSdrPtr;
    struct rtlsdr_dev * m_device;
    std::atomic<bool> m_enaDumpToFile;
    FILE * m_dumpFile;
    QMutex m_dumpFileMutex;
    std::atomic<bool> m_watchdogFlag;

    // DOC memory
    float m_dcI = 0.0;
    float m_dcQ = 0.0;

    // AGC memory
    float m_agcLevel = 0.0;

    bool isDumpingIQ() const { return m_enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);    
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

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

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
    FILE * m_dumpFile;

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
