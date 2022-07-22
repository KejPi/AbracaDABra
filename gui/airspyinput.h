#ifndef AIRSPYINPUT_H
#define AIRSPYINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <libairspy/airspy.h>
#include <libairspy/airspy_commands.h>
#include "inputdevice.h"

#define AIRSPY_DOC_ENABLE  0   // enable DOC
#define AIRSPY_AGC_ENABLE  0   // enable AGC
#define AIRSPY_WDOG_ENABLE 1   // enable watchdog timer

#define AIRSPY_WORKER 0

#define AIRSPY_FILTER_ORDER (42)

#if AIRSPY_WORKER
class AirspyWorker : public QThread
{
    Q_OBJECT
public:
    explicit AirspyWorker(struct airspy_device *d, QObject *parent = nullptr);
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
    void dumpedBytes(ssize_t bytes);
private:
    QObject *airspyPtr;
    struct airspy_device *device;
    std::atomic<bool> enaDumpToFile;
    FILE * dumpFile;
    QMutex fileMutex;
    std::atomic<bool> wdogIsRunningFlag;

#if (AIRSPY_DOC_ENABLE > 0)
    // DOC memory
    float dcI = 0.0;
    float dcQ = 0.0;
#endif

    // AGC memory
    float agcLev = 0.0;

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);    
    void emitAgcLevel(float level, int maxVal) { emit agcLevel(level, maxVal); }

    friend int airspyCb(airspy_transfer* transfer);
};
#endif

#define AIRSPY_FILTER_ORDER (42)
class AirspyDSFilter
{    
public:
    AirspyDSFilter();
    ~AirspyDSFilter();
    void reset();
    void process(float * inDataIQ, float *outDataIQ, int numIQ);
private:
    float * bufferPtr;
    float * buffer;

    // Halfband FIR, fixed coeffs, designed for downsampling 4096kHz -> 2048kHz
    static const int_fast8_t taps = AIRSPY_FILTER_ORDER + 1;
    static const int_fast8_t tapsX2 = 2*(AIRSPY_FILTER_ORDER + 1);
    constexpr static const float coef[(AIRSPY_FILTER_ORDER+2)/4 + 1] =
    {
        0.000112380767633697036585876949388307366,
        -0.000438873618326434972863880901172706217,
        0.001218129090837656493956364656128243951,
        -0.002787554831905981848894082730794252711,
        0.005622266695424369027656030795014885371,
        -0.010383084851125472941602012610928795766,
        0.018065458684386147963918389791615481954,
        -0.030481819420066884329667544761832687072,
        0.052030553411055557866404797096038237214,
        -0.098722780213991889741720342499320395291,
        0.315779321846157479125594136348809115589,
        0.5                                      ,
    };
};

class AirspyInput : public InputDevice
{
    Q_OBJECT
public:
    explicit AirspyInput(QObject *parent = nullptr);
    ~AirspyInput();
    bool openDevice() override;

    void tune(uint32_t freq) override;
    void setGainMode(GainMode mode, int gainIdx = 0);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBW(int bw);
    void setBiasT(bool ena);

signals:
    void gainListAvailable(const QList<int> * pList);

private:
    uint32_t frequency;
    bool deviceUnplugged;
    bool deviceRunning;
    struct airspy_device *device;
#if (AIRSPY_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    GainMode gainMode = GainMode::Hardware;
    int gainIdx;
    QList<int> * gainList;
    FILE * dumpFile;
    std::atomic<bool> enaDumpToFile;
    QMutex fileMutex;
    float * filterOutBuffer;

    AirspyDSFilter * filter;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    // used by friend
    void updateAgc(float level, int maxVal);

    void readThreadStopped();
#if (AIRSPY_WDOG_ENABLE)
    void watchDogTimeout();
#endif
    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);

    void processInputData(airspy_transfer* transfer);
    static int callback(airspy_transfer* transfer);
};


#endif // AIRSPYNPUT_H
