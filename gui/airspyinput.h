#ifndef AIRSPYINPUT_H
#define AIRSPYINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <libairspy/airspy.h>
#include <libairspy/airspy_commands.h>
#include "inputdevice.h"
#include "inputdevicesrc.h"

#define AIRSPY_AGC_ENABLE  1   // enable AGC
#define AIRSPY_WDOG_ENABLE 1   // enable watchdog timer
#define AIRSPY_DUMP_INT16  1   // dump raw stream in int16 insetad of float

#define AIRSPY_DUMP_FLOAT2INT16  (16384*2)   // conversion constant to int16

#define AIRSPY_SW_AGC_MIN      0
#define AIRSPY_SW_AGC_MAX     17
#define AIRSPY_HW_AGC_MIN      0
#define AIRSPY_HW_AGC_MAX     17
#define AIRSPY_LEVEL_THR_MIN (0.001)
#define AIRSPY_LEVEL_THR_MAX (0.1)
#define AIRSPY_LEVEL_RESET   (0.008)

enum class AirpyGainMode
{
    Hybrid,
    Software,
    Sensitivity,
    Manual
};

struct AirspyGainStr
{
    AirpyGainMode mode;
    int sensitivityGainIdx;
    int lnaGainIdx;
    int mixerGainIdx;
    int ifGainIdx;
    bool lnaAgcEna;
    bool mixerAgcEna;
};

class AirspyInput : public InputDevice
{
    Q_OBJECT
public:
    explicit AirspyInput(bool try4096kHz, QObject *parent = nullptr);
    ~AirspyInput();
    bool openDevice() override;

    void tune(uint32_t freq) override;
    void setGainMode(const AirspyGainStr & gain);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBiasT(bool ena);
    void setDataPacking(bool ena);

signals:
    void gainListAvailable(const QList<int> * pList);
    void agcLevel(float level);

private:
    uint32_t frequency;
    struct airspy_device *device;
#if (AIRSPY_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    AirpyGainMode gainMode = AirpyGainMode::Hybrid;
    int gainIdx;
    FILE * dumpFile;
    std::atomic<bool> enaDumpToFile;
    QMutex fileMutex;
    bool try4096kHzSR;
    float * filterOutBuffer;
    InputDeviceSRC * src;    
    int_fast8_t signalLevelEmitCntr;

    void run();           
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside usin setGainMode() function
    void setGain(int gIdx);

    void updateAgc(float level);

#if (AIRSPY_WDOG_ENABLE)
    void watchDogTimeout();
#endif

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);
    void processInputData(airspy_transfer* transfer);
    static int callback(airspy_transfer* transfer);
};

#endif // AIRSPYNPUT_H
