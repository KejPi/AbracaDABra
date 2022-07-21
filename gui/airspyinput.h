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
#define AIRSPY_WDOG_ENABLE 0   // enable watchdog timer

#define AIRSPY_WORKER 0

#define AIRSPY_SAMPLES_FLOAT 0

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

#if AIRSPY_SAMPLES_FLOAT
    void doFilter(float _Complex inBuffer[], int numIQsamples);
#else
    void doFilter(int16_t inBuffer[], int numIQsamples);
#endif

    friend int airspyCb(airspy_transfer* transfer);
};


#endif // AIRSPYNPUT_H
