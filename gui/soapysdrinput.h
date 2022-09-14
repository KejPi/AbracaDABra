#ifndef SOAPYSDRINPUT_H
#define SOAPYSDRINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Formats.hpp>
#include "inputdevice.h"

#define SOAPYSDR_DOC_ENABLE  1   // enable DOC
#define SOAPYSDR_AGC_ENABLE  1   // enable AGC
#define SOAPYSDR_WDOG_ENABLE 1   // enable watchdog timer

#define SOAPYSDR_INPUT_SAMPLES (16384)

class SoapySdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit SoapySdrWorker(SoapySDR::Device *d, SoapySDR::Stream *s, QObject *parent = nullptr);
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
    bool isRunning();
    void stop();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
    void dumpedBytes(ssize_t bytes);
private:
    QObject *soapySdrPtr;
    SoapySDR::Device *device;
    SoapySDR::Stream *stream;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;
    std::atomic<bool> wdogIsRunningFlag;
    std::atomic<bool> doReadIQ;

    // DOC memory
    float dcI = 0.0;
    float dcQ = 0.0;

    // AGC memory
    float agcLev = 0.0;

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);    
    void emitAgcLevel(float level, int maxVal) { emit agcLevel(level, maxVal); }
    void processInputData(std::complex<float> buff[], size_t numSamples);

    friend void soapysdrCb(unsigned char *buf, uint32_t len, void *ctx);
};

class SoapySdrInput : public InputDevice
{
    Q_OBJECT
public:
    explicit SoapySdrInput(QObject *parent = nullptr);
    ~SoapySdrInput();
    bool openDevice() override;

    void tune(uint32_t freq) override;
    void setDevArgs(const QString & args) { devArgs = args; }
    void setRxChannel(size_t ch) { rxChannel = ch; }

    void setGainMode(SoapyGainMode mode, int gainIdx = 0);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBW(int bw);

signals:
    void gainListAvailable(const QList<int> * pList);

private:
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    SoapySDR::Device *device;
    SoapySDR::Stream *stream;

    // settimgs
    QString devArgs;
    size_t rxChannel = 0;
    SoapySdrWorker * worker;
#if (SOAPYSDR_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    SoapyGainMode gainMode = SoapyGainMode::Hardware;
    int gainIdx;
    QList<int> * gainList;
    FILE * dumpFile;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by friend
    void updateAgc(float level, int maxVal);

    void readThreadStopped();
#if (SOAPYSDR_WDOG_ENABLE)
    void watchDogTimeout();
#endif
};


#endif // SOAPYSDRINPUT_H
