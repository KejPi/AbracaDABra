#ifndef AIRSPYINPUT_H
#define AIRSPYINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <libairspy/airspy.h>
#include <libairspy/airspy_commands.h>
#include "inputdevice.h"

#define AIRSPY_AGC_ENABLE  1   // enable AGC
#define AIRSPY_WDOG_ENABLE 1   // enable watchdog timer
#define AIRSPY_DUMP_INT16  1   // dump raw stream in int16 insetad of float

#define AIRSPY_DUMP_FLOAT2INT16  (16384*2)   // conversion constant to int16

#define AIRSPY_SW_AGC_MIN  0
#define AIRSPY_SW_AGC_MAX 17
#define AIRSPY_HW_AGC_MIN  0
#define AIRSPY_HW_AGC_MAX 17
#define AIRSPY_LEVEL_THR_MIN (0.001)
#define AIRSPY_LEVEL_THR_MAX (0.1)
#define AIRSPY_LEVEL_RESET   (0.008)

#define AIRSPY_FILTER_ORDER (42)

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

class AirspyDSFilter
{    
public:
    AirspyDSFilter();
    ~AirspyDSFilter();
    void reset();
    void resetSignalLevel() { signalLevel = AIRSPY_LEVEL_RESET; }

    // processing - returns signal level
    float process(float * inDataIQ, float *outDataIQ, int numIQ);
private:
#if (AIRSPY_AGC_ENABLE > 0)
    float signalLevel;
#endif
#if AIRSPY_FILTER_IQ_INTERLEAVED
    float * bufferPtr;
    float * buffer;
    float * coeFull;
#else
    float * bufferPtrI;
    float * bufferPtrQ;
    float * bufferI;
    float * bufferQ;
#endif    

    // Halfband FIR, fixed coeffs, designed for downsampling 4096kHz -> 2048kHz
    static const int_fast8_t taps = AIRSPY_FILTER_ORDER + 1;
    constexpr static const float coef[(AIRSPY_FILTER_ORDER+2)/4 + 1] =
    {
        0.000223158782894952853123604619156594708,
        -0.00070774549637065342286290636764078954,
        0.001735782601167994458266075064045708132,
        -0.003619832275410410967614316390950079949,
        0.006788741778432844271862212082169207861,
        -0.01183550169261274320753329902800032869,
        0.019680477383812611941182879604639310855,
        -0.032073581325677551212560700832909788005,
        0.053382280107447499517547839786857366562,
        -0.099631117404426483563639749263529665768,
        0.316099577146216947909351802081800997257,
        0.5
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
    void setGainMode(const AirspyGainStr & gain);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

    void setBiasT(bool ena);

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
    float * filterOutBuffer;
    AirspyDSFilter * filter;
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
