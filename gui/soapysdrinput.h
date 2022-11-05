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
    explicit SoapySdrWorker(SoapySDR::Device *device, double sampleRate, int rxChannel = 0, QObject *parent = nullptr);
    ~SoapySdrWorker();
    void dumpToFileStart(FILE * dumpFile);
    void dumpToFileStop();
    bool isRunning();
    void stop();
protected:
    void run() override;
signals:
    void agcLevel(float level);
    void dumpedBytes(ssize_t bytes);
private:
    SoapySDR::Device * m_device;
    int m_rxChannel;
    std::atomic<bool> m_enaDumpToFile;
    FILE * m_dumpFile;
    QMutex m_dumpFileMutex;
    std::atomic<bool> m_watchdogFlag;
    std::atomic<bool> m_doReadIQ;

    // SRC
    float * m_filterOutBuffer;
    InputDeviceSRC * m_src;

    // AGC memory
    float m_agcLevel = 0.0;
    uint_fast8_t m_signalLevelEmitCntr;

    bool isDumpingIQ() const { return m_enaDumpToFile; }
    void dumpBuffer(const float *buf, uint32_t numIQ);
    void processInputData(std::complex<float> buff[], size_t numSamples);
};

class SoapySdrInput : public InputDevice
{
    Q_OBJECT
public:
    explicit SoapySdrInput(QObject *parent = nullptr);
    ~SoapySdrInput();
    bool openDevice() override;

    void tune(uint32_t frequency) override;
    void setDevArgs(const QString & devArgs) { m_devArgs = devArgs; }
    void setRxChannel(int rxChannel) { m_rxChannel = rxChannel; }
    void setAntenna(const QString & antenna) { m_antenna = antenna; }

    void setGainMode(SoapyGainMode gainMode, int gainIdx = 0);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBW(int bw);

    QList<float> getGainList() const { return * m_gainList; }

private:
    double m_sampleRate;
    uint32_t m_frequency;
    bool m_deviceUnpluggedFlag;
    bool m_deviceRunningFlag;
    SoapySDR::Device * m_device;

    // settimgs
    QString m_devArgs;
    QString m_antenna;
    int m_rxChannel = 0;
    SoapySdrWorker * m_worker;
    QTimer m_watchdogTimer;
    SoapyGainMode m_gainMode = SoapyGainMode::Manual;
    int m_gainIdx;
    QList<float> * m_gainList;
    FILE * m_dumpFile;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gainIdx);

    // used by worker
    void onAgcLevel(float agcLevel);

    void onReadThreadStopped();
    void onWatchdogTimeout();
};


#endif // SOAPYSDRINPUT_H
