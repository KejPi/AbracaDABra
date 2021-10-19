#ifndef RTLSDRINPUT_H
#define RTLSDRINPUT_H

#include <QObject>
#include <QThread>
#include <rtl-sdr.h>
#include "inputdevice.h"

#define RTLSDR_DOC_ENABLE 1    // enable DOC
#define RTLSDR_AGC_ENABLE 1    // enable AGC

class RtlSdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlSdrWorker(struct rtlsdr_dev *d, QObject *parent = nullptr);
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
protected:
    void run() override;
signals:
    void readExit();
    void agcLevel(float level, int maxVal);
private:
    QObject *rtlSdrPtr;
    struct rtlsdr_dev *device;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;

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
    bool isAvailable() override;

public slots:
    void tune(uint32_t freq) override;
    void stop() override;
    void openDevice();
    void setGainMode(GainMode mode, int gainIdx = 0);
    void setDAGC(bool ena);

    void dumpToFileStart(const QString & filename);
    void dumpToFileStop();

signals:
    void gainListAvailable(const QList<int> * pList);
    void dumpToFileState(bool ena);
private:
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    struct rtlsdr_dev *device;
    RtlSdrWorker * worker;
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

    friend class RtlSdrWorker;
};


#endif // RTLSDRINPUT_H
