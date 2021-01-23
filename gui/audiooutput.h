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
//#define AUDIOOUTPUT_USE_RTAUDIO
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
    void start(quint32 sRate, quint8 numChannels);

private:
    quint32 sampleRate;
    quint8 numChannels;    
    QTimer * audioStartTimer;
#ifdef AUDIOOUTPUT_USE_PORTAUDIO
    PaStream * audioOutput = nullptr;
    audioFifo_t * inFifoPtr = nullptr;
    unsigned int bufferFrames;
    static int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *buffer);
    //    static void rtAudioErrorCb(RtAudioError::Type type, const std::string &errorText);
#else
#ifdef AUDIOOUTPUT_USE_RTAUDIO
    RtAudio * audioOutput = nullptr;
    audioFifo_t * inFifoPtr = nullptr;
    unsigned int bufferFrames;
    static int rtAudioCb(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                              double streamTime, RtAudioStreamStatus status, void * buffer);
    static void rtAudioErrorCb(RtAudioError::Type type, const std::string &errorText);
#else
    AudioIODevice * ioDevice;
    QAudioOutput * audioOutput;
#endif
#endif

#if AUDIOOUTPUT_DBG_TIMER
    qint64 minCount = INT64_MAX;
    qint64 maxCount = INT64_MIN;
    qint64 buf[AUDIOOUTPUT_DBG_AVRG_SIZE];
    qint8 cntr = 0;
    qint64 sum = 0;
    QTimer * dbgTimer;
    void bufferMonitor();
#endif
    qint64 bytesAvailable() const;

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

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;
private:
    audioFifo_t * inFifoPtr = nullptr;
};

#endif // AUDIOOUTPUT_H
