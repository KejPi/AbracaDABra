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
#include <QAudioSink>
#include <QMediaDevices>
#endif


//#define AUDIOOUTPUT_RAW_FILE_OUT

#define AUDIOOUTPUT_FADE_TIME_MS    60

// these 2 values must be aligned
#define AUDIOOUTPUT_FADE_MIN_DB    -60.0
#define AUDIOOUTPUT_FADE_MIN_LIN     0.001

#ifdef AUDIOOUTPUT_USE_PORTAUDIO
// port audio allows to set number of samples in callback
// this number must be aligned between AUDIOOUTPUT_FADE_TIME_MS and AUDIO_FIFO_CHUNK_MS
#if (AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)
#error "(AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)"
#endif
#endif

enum class AudioOutputPlaybackState
{
    Playing = 0,
    Muted = 1,
};


class AudioIODevice;

class AudioOutput : public QObject
{
    Q_OBJECT

public:
    AudioOutput(audioFifo_t *buffer, QObject *parent = nullptr);
    ~AudioOutput();
    void stop();

public slots:
    void start(uint32_t sRate, uint8_t numChannels);
    void mute(bool on);

private:
#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    FILE * rawOut;
#endif

#ifdef AUDIOOUTPUT_USE_PORTAUDIO
    std::atomic<bool> m_muteFlag  = false;
    std::atomic<bool> m_stopFlag  = false;
    PaStream * m_outStream = nullptr;
    audioFifo_t * m_inFifoPtr = nullptr;
    uint8_t m_numChannels;
    uint32_t m_sampleRate_kHz;
    unsigned int m_bufferFrames;
    uint8_t m_bytesPerFrame;
    float m_muteFactor;

    AudioOutputPlaybackState m_playbackState;

    int portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames);

    friend int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                     const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);
#else

    // Qt audio
    AudioIODevice * ioDevice;
    QMediaDevices * devices;
    QAudioSink * audioSink;
    void handleStateChanged(QAudio::State newState);
    int64_t bytesAvailable();
    void doStop();
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

    void setFormat(const QAudioFormat & format);
    void mute(bool on);
    bool isMuted() const { return AudioOutputPlaybackState::Muted == m_playbackState; }

private:
    audioFifo_t * m_inFifoPtr = nullptr;
    AudioOutputPlaybackState m_playbackState;
    uint8_t m_bytesPerFrame;
    uint32_t m_sampleRate_kHz;
    uint8_t m_numChannels;
    float m_muteFactor;

    std::atomic<bool> m_muteFlag  = false;
    std::atomic<bool> m_stopFlag  = false;
};
#endif

#endif // AUDIOOUTPUT_H
