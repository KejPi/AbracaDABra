#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QIODevice>
#include <QObject>
#include <QWaitCondition>
#include <QMutex>

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QTimer>

#include "audiofifo.h"

#define AUDIOOUTPUT_DBG_TIMER 1
#define AUDIOOUTPUT_DBG_AVRG_SIZE 32
#define AUDIOOUTPUT_USE_PORTAUDIO

#ifdef AUDIOOUTPUT_USE_RTAUDIO
#include "rtaudio/RtAudio.h"
#endif

#ifdef AUDIOOUTPUT_USE_PORTAUDIO
#include "portaudio.h"
#endif


class AudioIODevice;

class AudioOutput : public QObject
{
    Q_OBJECT
public:
    AudioOutput(audioFifo_t *buffer);
    ~AudioOutput();
    void stop();

public slots:
    void start(uint32_t sRate, uint8_t numChannels);

private:
    uint32_t sampleRate;
    uint8_t numChannels;    
    QTimer * audioStartTimer;
#ifdef AUDIOOUTPUT_USE_PORTAUDIO
    PaStream * audioOutput = nullptr;
    audioFifo_t * inFifoPtr = nullptr;
    unsigned int bufferFrames;
    static int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *buffer);
    //    static void rtAudioErrorCb(RtAudioError::Type type, const std::string &errorText);
#else
    AudioIODevice * ioDevice;
    QAudioOutput * audioOutput;
#endif

#if AUDIOOUTPUT_DBG_TIMER
    int64_t minCount = INT64_MAX;
    int64_t maxCount = INT64_MIN;
    int64_t buf[AUDIOOUTPUT_DBG_AVRG_SIZE];
    int8_t cntr = 0;
    int64_t sum = 0;
    QTimer * dbgTimer;
    void bufferMonitor();
#endif
    int64_t bytesAvailable() const;

private slots:
    void checkInputBuffer();
    void initTimer();
    void destroyTimer();
#if (!defined AUDIOOUTPUT_USE_RTAUDIO) && (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    void handleStateChanged(QAudio::State newState);
#endif
};

class AudioIODevice : public QIODevice
{
public:
    AudioIODevice(audioFifo_t *buffer, QObject *parent = nullptr);

    void start();
    void stop();

    int64_t readData(char *data, int64_t maxlen) override;
    int64_t writeData(const char *data, int64_t len) override;
    int64_t bytesAvailable() const override;
private:
    audioFifo_t * inFifoPtr = nullptr;
};

#endif // AUDIOOUTPUT_H
