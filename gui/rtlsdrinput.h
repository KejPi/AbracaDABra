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
protected:
    void run() override;
signals:
    void readExit();
private:
    QObject *rtlSdrPtr;
    struct rtlsdr_dev *device;
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

signals:
    void gainListAvailable(const QList<int> * pList);

private:
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    struct rtlsdr_dev *device;
    RtlSdrWorker * worker;
    bool gainAutoMode;
    QList<int> * gainList;

    void run();

    friend void rtlsdrCb(unsigned char *buf, uint32_t len, void *ctx);
private slots:
    void readThreadStopped();
};


#endif // RTLSDRINPUT_H
