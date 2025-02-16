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
    Driver,
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
        SDRPLAY,
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
            QString tuner;
            QString sn;
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
    virtual bool openDevice(const QVariant &hwId = QVariant(), bool fallbackConnection = true) = 0;
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
    constexpr static const float m_20log10[128] = {
        // precalculated 20*log10(0..127)
        NAN,       0.000000,  6.020600,  9.542425,  12.041200, 13.979400, 15.563025, 16.901961, 18.061800, 19.084850, 20.000000, 20.827854, 21.583625,
        22.278867, 22.922561, 23.521825, 24.082400, 24.608978, 25.105450, 25.575072, 26.020600, 26.444386, 26.848454, 27.234557, 27.604225, 27.958800,
        28.299467, 28.627275, 28.943161, 29.247960, 29.542425, 29.827234, 30.103000, 30.370279, 30.629578, 30.881361, 31.126050, 31.364034, 31.595672,
        31.821292, 32.041200, 32.255677, 32.464986, 32.669369, 32.869054, 33.064250, 33.255157, 33.441957, 33.624825, 33.803922, 33.979400, 34.151404,
        34.320067, 34.485517, 34.647875, 34.807254, 34.963761, 35.117497, 35.268560, 35.417040, 35.563025, 35.706597, 35.847834, 35.986811, 36.123599,
        36.258267, 36.390879, 36.521496, 36.650178, 36.776982, 36.901961, 37.025167, 37.146650, 37.266457, 37.384634, 37.501225, 37.616272, 37.729815,
        37.841892, 37.952542, 38.061800, 38.169700, 38.276277, 38.381562, 38.485586, 38.588379, 38.689969, 38.790385, 38.889653, 38.987800, 39.084850,
        39.180828, 39.275757, 39.369659, 39.462557, 39.554472, 39.645425, 39.735435, 39.824522, 39.912704, 40.000000, 40.086427, 40.172003, 40.256744,
        40.340667, 40.423786, 40.506117, 40.587676, 40.668475, 40.748530, 40.827854, 40.906460, 40.984360, 41.061569, 41.138097, 41.213957, 41.289160,
        41.363717, 41.437640, 41.510939, 41.583625, 41.655707, 41.727197, 41.798102, 41.868434, 41.938200, 42.007411, 42.076074};
};

Q_DECLARE_METATYPE(InputDevice::ErrorCode);
Q_DECLARE_OPERATORS_FOR_FLAGS(InputDevice::Capabilities)

extern fifo_t inputBuffer;
void getSamples(float buffer[], uint16_t len);
void skipSamples(float buffer[], uint16_t numSamples);

#endif  // INPUTDEVICE_H
