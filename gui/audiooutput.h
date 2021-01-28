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

#define AUDIOOUTPUT_USE_PORTAUDIO

#define AUDIOOUTPUT_DBG_TIMER 1
#define AUDIOOUTPUT_DBG_AVRG_SIZE 32

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
#ifdef AUDIOOUTPUT_USE_PORTAUDIO
    PaStream * outStream = nullptr;
    audioFifo_t * inFifoPtr = nullptr;
    unsigned int bufferFrames;
    uint8_t bytesPerFrame;
    bool isMuted = true;

    int portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames, PaStreamCallbackFlags statusFlags);

    friend int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);
#else
    QTimer * audioStartTimer;
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
#if (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    void checkInputBuffer();
    void initTimer();
    void destroyTimer();
    void handleStateChanged(QAudio::State newState);
#endif
};

#if (!defined AUDIOOUTPUT_USE_PORTAUDIO)
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
#endif

#endif // AUDIOOUTPUT_H
