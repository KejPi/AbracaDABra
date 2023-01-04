#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QIODevice>
#include <QObject>
#include <QWaitCondition>
#include <QMutex>
#include <QTimer>
#include "config.h"
#include "audiofifo.h"

#if HAVE_PORTAUDIO
#include "portaudio.h"
#else
#include <QAudioSink>
#include <QMediaDevices>
#endif

// muting
#define AUDIOOUTPUT_FADE_TIME_MS    60
// these 2 values must be aligned
#define AUDIOOUTPUT_FADE_MIN_DB    -80.0
#define AUDIOOUTPUT_FADE_MIN_LIN     0.0001

// debug switch
//#define AUDIOOUTPUT_RAW_FILE_OUT

enum class AudioOutputPlaybackState
{
    Muted = 0,
    Playing = 1,
};


#if HAVE_PORTAUDIO

#define AUDIOOUTPUT_PORTAUDIO_VOLUME_ROUND      1

// port audio allows to set number of samples in callback
// this number must be aligned between AUDIOOUTPUT_FADE_TIME_MS and AUDIO_FIFO_CHUNK_MS
#if (AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)
#error "(AUDIOOUTPUT_FADE_TIME_MS != AUDIO_FIFO_CHUNK_MS)"
#endif

class AudioOutput : public QObject
{
    Q_OBJECT

public:
    AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();
    void stop();

public slots:
    void start(audioFifo_t *buffer);
    void restart(audioFifo_t *buffer);
    void mute(bool on);
    void setVolume(int value);

private:
    enum Request
    {
        None = 0,          // no bit
        Mute = (1 << 0),   // bit #0
        Stop = (1 << 1),   // bit #1
        Restart = (1 << 2) // bit #2
    };
    std::atomic<unsigned int> m_cbRequest = Request::None;

    PaStream * m_outStream = nullptr;
    audioFifo_t * m_inFifoPtr = nullptr;
    audioFifo_t * m_restartFifoPtr = nullptr;
    uint8_t m_numChannels;
    uint32_t m_sampleRate_kHz;
    unsigned int m_bufferFrames;
    uint8_t m_bytesPerFrame;
    float m_muteFactor;
    std::atomic<float> m_linearVolume;

    AudioOutputPlaybackState m_playbackState;

    int portAudioCbPrivate(void *outputBuffer, unsigned long nBufferFrames);
    void portAudioStreamFinishedPrivateCb() { emit streamFinished(); }

    static int portAudioCb(const void *inputBuffer, void *outputBuffer, unsigned long nBufferFrames,
                           const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *ctx);
    static void portAudioStreamFinishedCb(void *ctx);

#ifdef AUDIOOUTPUT_RAW_FILE_OUT
    FILE * m_rawOut;
#endif

    void onStreamFinished();
signals:
    void streamFinished();     // this signal is emited from portAudioStreamFinishedCb
};

#else // HAVE_PORTAUDIO

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
    void setVolume(int value);

private:
    // Qt audio
    AudioIODevice * m_ioDevice;
    QMediaDevices * m_devices;
    QAudioSink * m_audioSink;
    float m_linearVolume;

    void handleStateChanged(QAudio::State newState);
    int64_t bytesAvailable();
    void doStop();
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
