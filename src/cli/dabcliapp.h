/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef DABCLIAPP_H
#define DABCLIAPP_H

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QThread>

#include "audiodecoder.h"
#include "audiostreamer.h"
#include "config.h"
#include "dldecoder.h"
#include "inputdevice.h"
#include "radiocontrol.h"
#include "slideshowapp.h"

#if HAVE_PORTAUDIO
#include "cliaudiooutput.h"
#endif

class AudioRecorder;
class WebServer;

enum class CliDeviceType
{
    RTLSDR,
    RTLTCP
};

struct CliConfig
{
    CliDeviceType deviceType = CliDeviceType::RTLSDR;
    QString deviceArg;  // RTLSDR: serial number or index (empty = first device found)

    QString rtlTcpHost = QStringLiteral("127.0.0.1");
    quint16 rtlTcpPort = 1234;
    bool rtlTcpNativeSocket = false;

    QString channel;           // e.g. "12C"
    uint32_t frequencyKHz = 0;  // used when channel is empty

    bool haveInitialService = false;
    uint32_t initialSid = 0;
    uint8_t initialScids = 0;
    QString initialServiceLabel;  // alternative to SId: match by (partial) label

    QHostAddress bindAddress = QHostAddress::AnyIPv4;
    quint16 webPort = 7979;
    // true if the user explicitly passed --bind and/or --port; otherwise the CLI shows the
    // interactive terminal UI (or, if stdout isn't a terminal, falls back to plain log output)
    bool webUiEnabled = false;

    // RtlSdrInput's software AGC (RtlGainMode::Software) is the GUI's default and adjusts gain
    // automatically based on RF level; RTL-SDR's own hardware AGC often performs worse.
    bool hardwareAgc = false;
    int manualGainIndex = -1;

    int verboseLevel = 0;
};

struct CliServiceInfo
{
    uint32_t sid = 0;
    uint8_t scids = 0;
    QString label;
    uint8_t subChId = 0;
    uint16_t subChSize = 0;
    uint16_t bitRate = 0;
    bool isAudio = false;
};

class DabCliApp : public QObject
{
    Q_OBJECT
public:
    explicit DabCliApp(const CliConfig &config, QObject *parent = nullptr);
    ~DabCliApp();

    // starts input device + radio control + web server; returns false on fatal error
    bool start();

    // thread-safe accessors used by WebServer (which lives in the same thread as this object)
    QJsonObject statusJson() const;
    static QJsonArray channelsJson();

    bool requestTuneChannel(const QString &channelLabel, QString *errorOut);
    bool requestTuneFrequency(uint32_t freqKHz, QString *errorOut);
    bool requestService(uint32_t sid, uint8_t scids, QString *errorOut);

    // percent in [0, 100]; no-op if local audio output (HAVE_PORTAUDIO / TUI mode) isn't active
    void setVolume(int percent);
    int volumePercent() const;

    // toggles volumePercent() between 0 and the level it had before muting; invokable so it can
    // be triggered by desktop media-key/MPRIS Play-Pause commands (see linuxSetupMediaRemoteCommands)
    Q_INVOKABLE void toggleMute();

    // Begins a graceful shutdown: tunes the DAB engine to idle (freq 0) so the input device and
    // dabsdr processing thread can stop cleanly, then emits readyToQuit(). Safe to call even if
    // never tuned (emits readyToQuit() immediately in that case).
    void requestShutdown();

    void currentAudioFormat(int *sampleRate, int *numChannels) const;

    // "Artist - Title" if DL Plus tags are available, else the raw DLS text; empty if none yet
    QString currentStreamTitle() const;

    // returns false if no slide (MOT/SLS image) has been received yet for the current service
    bool currentSlide(QByteArray *data, QString *contentType) const;

signals:
    // forwarded (queued) to RadioControl, which lives in its own thread
    void tuneServiceRequested(uint32_t freq, uint32_t SId, uint8_t SCIdS);

    // emitted once the graceful shutdown requested via requestShutdown() has completed
    void readyToQuit();

private slots:
    void onEnsembleInformation(const RadioControlEnsemble &ens);
    void onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &s);
    void onServiceListComplete(const RadioControlEnsemble &ens);
    void onSignalState(uint8_t sync, float snr);
    void onTuneDone(uint32_t freq);
    void onAudioServiceSelection(const RadioControlServiceComponent &s);
    void onAudioParametersInfo(const AudioParameters &params);
    void onInputDeviceReady();
    void onInputDeviceError(InputDevice::ErrorCode errCode);
    void onDlComplete(const QString &dl);
    void onDlPlusObject(const DLPlusObject &object);
    void onDlReset();
    void onCurrentSlide(const Slide &slide);
    void onFreqOffset(float offsetHz);
    void onDecodingStats(const RadioControlDecodingStats &stats);
    void onRfLevel(float level, float gain);

private:
    static quint64 key(uint32_t sid, uint8_t scids) { return (quint64(sid) << 8) | scids; }
    bool openInputDevice();
    void resolvePendingServiceLabel();

    // assumes m_mutex is already locked by the caller (used both by currentStreamTitle() and by
    // the MPRIS "now playing" subtitle updates triggered from the same DL Plus/DLS handlers)
    QString buildCurrentSubtitleLocked() const;

    CliConfig m_config;

    QThread *m_radioControlThread = nullptr;
    RadioControl *m_radioControl = nullptr;

    QThread *m_audioDecoderThread = nullptr;
    AudioDecoder *m_audioDecoder = nullptr;
    AudioRecorder *m_audioRecorder = nullptr;

    AudioStreamer *m_audioStreamer = nullptr;
#if HAVE_PORTAUDIO
    // Local audio playback for headless/TUI mode (no web UI client to stream audio to);
    // only created when m_config.webUiEnabled is false.
    CliAudioOutput *m_audioOutput = nullptr;
#endif
    InputDevice *m_inputDevice = nullptr;
    WebServer *m_webServer = nullptr;
    DLDecoder *m_dlDecoder = nullptr;
    SlideShowApp *m_slideShowApp = nullptr;

    mutable QMutex m_mutex;
    RadioControlEnsemble m_ensemble;
    QMap<quint64, CliServiceInfo> m_services;
    uint8_t m_syncLevel = 0;
    float m_snr = 0.0f;
    uint32_t m_currentFrequency = 0;
    uint32_t m_currentSid = 0;
    uint8_t m_currentScids = 0;
    QString m_currentServiceLabel;
    bool m_isPlaying = false;
    AudioParameters m_audioParams{};
    bool m_haveAudioParams = false;
    bool m_deviceReady = false;
    bool m_shutdownRequested = false;
    QString m_lastDeviceError;
    QString m_dlsText;
    QString m_dlPlusTitle;
    QString m_dlPlusArtist;
    // other DL Plus tags (weather, traffic, station name, ...), keyed by DLPlusContentType;
    // title/artist are kept separately above since currentStreamTitle() special-cases them
    QMap<int, QString> m_dlPlusTags;

    int m_volumePercent = 100;
    int m_volumeBeforeMute = 100;

    QByteArray m_slideData;
    QString m_slideContentType;
    int m_slideVersion = 0;

    float m_freqOffsetHz = 0.0f;
    bool m_haveRfLevel = false;
    float m_rfLevel = 0.0f;
    float m_gain = 0.0f;
    RadioControlDecodingStats m_decodingStats{};
    bool m_haveDecodingStats = false;
};

#endif  // DABCLIAPP_H
