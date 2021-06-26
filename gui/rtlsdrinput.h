#ifndef RTLSDRINPUT_H
#define RTLSDRINPUT_H

#include <QObject>
#include <QThread>
#include <rtl-sdr.h>
#include "inputdevice.h"


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
private:
    QObject *rtlSdrPtr;
    struct rtlsdr_dev *device;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);

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
    void setGainAutoMode(bool enable = true);
    void setGain(int gainVal);
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
    bool gainAutoMode;
    QList<int> * gainList;
    FILE * dumpFile;

    void run();           
private slots:
    void readThreadStopped();
};


#endif // RTLSDRINPUT_H
