#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QIODevice>
#include <QObject>
#include <QWaitCondition>
#include <QMutex>
#include <QTimer>

#include "audiofifo.h"

#define AUDIOOUTPUT_USE_PORTAUDIO

#ifdef AUDIOOUTPUT_USE_PORTAUDIO
#include "portaudio.h"
#else
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#endif

#ifdef QT_DEBUG
//#define AUDIOOUTPUT_DBG_TIMER 1
#define AUDIOOUTPUT_DBG_AVRG_SIZE 32
#endif
//#define AUDIOOUTPUT_RAW_FILE_OUT

#define AUDIOOUTPUT_FADE_TIME_MS 40

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
    void mute(bool on);

private:
    uint32_t m_sampleRate_kHz;
    uint8_t m_numChannels;
#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    FILE * rawOut;
#endif

#ifdef AUDIOOUTPUT_USE_PORTAUDIO
    PaStream * m_outStream = nullptr;
    audioFifo_t * m_inFifoPtr = nullptr;
    unsigned int m_bufferFrames;
    uint8_t m_bytesPerFrame;

    enum
    {
        StatePlaying = 0,
        StateMuted = 1,
        StateDoMute = 2,
        StateDoUnmute = 3,
    } m_playbackState;

    bool m_muteFlag = false;
    QMutex m_muteMutex;

    int portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames, PaStreamCallbackFlags statusFlags);

    friend int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);
#else
    QTimer * audioStartTimer;
    AudioIODevice * ioDevice;
    QAudioOutput * audioOutput;
#endif

#if AUDIOOUTPUT_DBG_TIMER
    int64_t m_minCount = INT64_MAX;
    int64_t m_maxCount = INT64_MIN;
    int64_t m_dbgBuf[AUDIOOUTPUT_DBG_AVRG_SIZE];
    int8_t m_cntr = 0;
    int64_t m_sum = 0;
    QTimer * m_dbgTimer;
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
