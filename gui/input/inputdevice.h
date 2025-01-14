/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef INPUTDEVICE_H
#define INPUTDEVICE_H

#include <pthread.h>

#include <QMutex>
#include <QObject>
#include <QVariant>
#include <QWaitCondition>

// this is chunk that is received from input device to be stored in input FIFO
#define INPUT_CHUNK_MS (200)
#define INPUT_CHUNK_IQ_SAMPLES (2048 * INPUT_CHUNK_MS)

// Input FIFO size in bytes - FIFO contains float _Complex samples => [float float]
// total capacity is 8 input chunks
#define INPUT_FIFO_SIZE (INPUT_CHUNK_IQ_SAMPLES * (2 * sizeof(float)) * 8)

#define INPUTDEVICE_WDOG_TIMEOUT_SEC 3  // watchdog timeout in seconds (if implemented and enabled)

#define INPUTDEVICE_BANDWIDTH (1530 * 1000)

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

enum class RtlGainMode
{
    Undefined = -1,
    Hardware = 0,
    Software,
    Manual
};

enum class SoapyGainMode
{
    Hardware,
    Manual
};

struct InputDeviceDesc
{
    QString diplayName;
    QVariant id;
};
typedef QList<struct InputDeviceDesc> InputDeviceList;

class InputDevice : public QObject
{
    Q_OBJECT
public:
    enum class Id
    {
        UNDEFINED = 0,
        RTLSDR,
        RTLTCP,
        RAWFILE,
        AIRSPY,
        SOAPYSDR,
        RARTTCP
    };

    struct Description
    {
        InputDevice::Id id = InputDevice::Id::UNDEFINED;
        struct
        {
            QString name;
            QString model;
        } device;
        struct
        {
            int sampleRate;
            int channelBits;           // I or Q
            int containerBits;         // I or Q
            QString channelContainer;  // I or Q
        } sample;
        struct
        {
            bool hasXmlHeader;
            QString recorder;
            QString time;
            uint32_t frequency_kHz;
            uint64_t numSamples;
        } rawFile;
    };

    enum class ErrorCode
    {
        Undefined = 0,
        EndOfFile = -1,           // Raw file input
        DeviceDisconnected = -2,  // USB disconnected or socket disconnected
        NoDataAvailable = -3,     // This can happen when TCP server is connected but stopped sending data for some reason
    };

    enum Capability
    {
        LiveStream = (1 << 0),
        Recording = (1 << 1),
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    InputDevice(QObject *parent = nullptr);
    ~InputDevice();
    virtual bool openDevice(const QVariant &hwId = QVariant()) = 0;
    const InputDevice::Description &deviceDescription() const { return m_deviceDescription; }
    virtual void tune(uint32_t freq) = 0;
    virtual void startStopRecording(bool start) = 0;
    virtual InputDevice::Capabilities capabilities() const = 0;
    virtual void setBW(uint32_t bw) { Q_UNUSED(bw) }
    virtual void setBiasT(bool ena) { Q_UNUSED(ena) }
    virtual void setPPM(int ppm) { Q_UNUSED(ppm) }
    virtual QVariant hwId() { return QVariant(); }

signals:
    void deviceReady();
    void tuned(uint32_t freq);
    void agcGain(float gain);
    void rfLevel(float level, float gain);
    void recordBuffer(const uint8_t *buf, uint32_t len);
    void error(const InputDevice::ErrorCode errCode = InputDevice::ErrorCode::Undefined);

protected:
    Description m_deviceDescription;
};

Q_DECLARE_METATYPE(InputDevice::ErrorCode);
Q_DECLARE_OPERATORS_FOR_FLAGS(InputDevice::Capabilities)

extern fifo_t inputBuffer;
void getSamples(float buffer[], uint16_t len);
void skipSamples(float buffer[], uint16_t numSamples);

#endif  // INPUTDEVICE_H
