#ifndef INPUTDEVICE_H
#define INPUTDEVICE_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <pthread.h>

// this is chunk that is received from input device to be stored in input FIFO
#define INPUT_CHUNK_MS            (400)
#define INPUT_CHUNK_IQ_SAMPLES    (2048 * INPUT_CHUNK_MS)

// Input FIFO size in bytes - FIFO contains float _Complex samples => [float float]
// total capacity is 8 input chunks
#define INPUT_FIFO_SIZE           (INPUT_CHUNK_IQ_SAMPLES * (sizeof(float _Complex)) * 8)

#define INPUTDEVICE_WDOG_TIMEOUT_SEC 2   // watchdog timeout in seconds (if implemented and enabled)

#define INPUTDEVICE_AGC_GAIN_NA  0x0FFF  // this value signalizes that AGC gain is not available (e.g. hardware AGC)

struct ComplexFifo
{
    uint64_t count;
    uint64_t head;
    uint64_t tail;
    uint8_t buffer[INPUT_FIFO_SIZE];

    pthread_mutex_t countMutex;
    pthread_cond_t countCondition;

    void reset();
    void fillDummy();
};
typedef struct ComplexFifo fifo_t;

enum class InputDeviceId { UNDEFINED, RTLSDR, RTLTCP, RARTTCP, RAWFILE};

enum class GainMode
{
    Hardware,
    Software,
    Manual
};

enum class InputDeviceErrorCode
{
    Undefined = 0,
    EndOfFile = -1,                // Raw file input
    DeviceDisconnected = -2,       // USB disconnected or socket disconnected
    NoDataAvailable = -3,          // This can happen when TCP server is connected but stopped sending data for some reason
};

Q_DECLARE_METATYPE(InputDeviceErrorCode);

class InputDevice : public QObject
{
    Q_OBJECT    
public:
    InputDevice(QObject *parent = nullptr);
    ~InputDevice();
    virtual bool openDevice() = 0;
    const InputDeviceId getDeviceId() const { return id; }

public slots:
    virtual void tune(uint32_t freq) = 0;
    virtual void startDumpToFile(const QString & filename)  { /* do nothing by default */ };
    virtual void stopDumpToFile() { /* do nothing by default */ };

signals:
    void deviceReady();
    void tuned(uint32_t freq);
    void dumpingToFile(bool running, int bytesPerSample = 2);
    void dumpedBytes(ssize_t bytes);
    void agcGain(int gain10);
    void error(const InputDeviceErrorCode errCode = InputDeviceErrorCode::Undefined);

protected:
    InputDeviceId id = InputDeviceId::UNDEFINED;
};

extern fifo_t inputBuffer;
void getSamples(float _Complex buffer[], uint16_t len);
void skipSamples(float _Complex buffer[], uint16_t numSamples);

#endif // INPUTDEVICE_H
