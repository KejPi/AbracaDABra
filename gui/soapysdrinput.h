#ifndef SOAPYSDRINPUT_H
#define SOAPYSDRINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Formats.hpp>
#include "inputdevice.h"
#include "inputdevicesrc.h"

#define SOAPYSDR_WDOG_ENABLE 1   // enable watchdog timer

#define SOAPYSDR_DUMP_INT16  1   // dump raw stream in int16 insetad of float
#define SOAPYSDR_DUMP_FLOAT2INT16  (32768)   // conversion constant to int16

#define SOAPYSDR_INPUT_SAMPLES (16384)

#define SOAPYSDR_BANDWIDTH     (1700*1000)
#define SOAPYSDR_LEVEL_THR_MAX (0.5)
#define SOAPYSDR_LEVEL_THR_MIN (SOAPYSDR_LEVEL_THR_MAX/20.0)
#define SOAPYSDR_LEVEL_RESET   ((SOAPYSDR_LEVEL_THR_MAX-SOAPYSDR_LEVEL_THR_MIN)/2.0 + SOAPYSDR_LEVEL_THR_MIN)

class SoapySdrWorker : public QThread
{
    Q_OBJECT
public:
    explicit SoapySdrWorker(SoapySDR::Device *d, double sampleRate, int channel = 0, QObject *parent = nullptr);
    ~SoapySdrWorker();
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
    bool isRunning();
    void stop();
protected:
    void run() override;
signals:
    void agcLevel(float level);
    void dumpedBytes(ssize_t bytes);
private:
    QObject *soapySdrPtr;
    SoapySDR::Device *device;
    int rxChannel;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;
    std::atomic<bool> wdogIsRunningFlag;
    std::atomic<bool> doReadIQ;

    // SRC
    float * filterOutBuffer;
    InputDeviceSRC * src;

    // AGC memory
    float agcLev = 0.0;
    uint_fast8_t signalLevelEmitCntr;

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);    
    void processInputData(std::complex<float> buff[], size_t numSamples);
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
    void setRxChannel(int ch) { rxChannel = ch; }

    void setGainMode(SoapyGainMode mode, int gainIdx = 0);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBW(int bw);

    QList<float> getGainList() const { return * gainList; }

private:
    double sampleRate;
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    SoapySDR::Device *device;

    // settimgs
    QString devArgs;
    int rxChannel = 0;
    SoapySdrWorker * worker;
#if (SOAPYSDR_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    SoapyGainMode gainMode = SoapyGainMode::Manual;
    int gainIdx;
    QList<float> * gainList;
    FILE * dumpFile;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by worker
    void updateAgc(float level);

    void readThreadStopped();
#if (SOAPYSDR_WDOG_ENABLE)
    void watchDogTimeout();
#endif
};


#endif // SOAPYSDRINPUT_H
