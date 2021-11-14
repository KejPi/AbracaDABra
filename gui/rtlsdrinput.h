#ifndef RTLSDRINPUT_H
#define RTLSDRINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <rtl-sdr.h>
#include "inputdevice.h"

#define RTLSDR_DOC_ENABLE  1   // enable DOC
#define RTLSDR_AGC_ENABLE  1   // enable AGC

#define RTLSDR_WDOG_ENABLE 1        // enable watchdog timer
#define RTLSDR_WDOG_TIMEOUT_SEC 1   // watchdow timeout in seconds

class RtlSdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlSdrWorker(struct rtlsdr_dev *d, QObject *parent = nullptr);
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
private:
    QObject *rtlSdrPtr;
    struct rtlsdr_dev *device;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;
    std::atomic<bool> wdogIsRunningFlag;

    // DOC memory
    float dcI = 0.0;
    float dcQ = 0.0;

    // AGC memory
    float agcLev = 0.0;

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);    
    void emitAgcLevel(float level, int maxVal) { emit agcLevel(level, maxVal); }

    friend void rtlsdrCb(unsigned char *buf, uint32_t len, void *ctx);
};

class RtlSdrInput : public InputDevice
{
    Q_OBJECT
public:
    explicit RtlSdrInput(QObject *parent = nullptr);
    ~RtlSdrInput();
    bool openDevice() override;

public slots:
    void tune(uint32_t freq) override;
    void stop() override;
    void setGainMode(GainMode mode, int gainIdx = 0);
    void setDAGC(bool ena);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

signals:
    void gainListAvailable(const QList<int> * pList);

private:
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    struct rtlsdr_dev *device;
    RtlSdrWorker * worker;
#if (RTLSDR_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    GainMode gainMode = GainMode::Hardware;
    int gainIdx;
    QList<int> * gainList;
    FILE * dumpFile;

    void run();           

    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by friend
    void updateAgc(float level, int maxVal);
private slots:
    void readThreadStopped();
#if (RTLSDR_WDOG_ENABLE)
    void watchDogTimeout();
#endif
};


#endif // RTLSDRINPUT_H
