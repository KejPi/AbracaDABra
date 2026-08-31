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

#include "dabcliapp.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <algorithm>

#include "audiorecorder.h"
#include "config.h"
#include "dabtables.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#include "webserver.h"

#if HAVE_FAAD
#include "audiodecoderfaad.h"
#endif
#if HAVE_FDKAAC
#include "audiodecoderfdkaac.h"
#endif
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
#include "linux.h"
#endif

namespace
{
// English labels for the DL Plus content types worth surfacing outside of ITEM_TITLE/ITEM_ARTIST
// (handled separately); mirrors DLPlusModelItem::typeToLabel() in dlplusmodel.cpp. Empty return
// means "not shown" (DUMMY, RFU/reserved and receiver-only hint types).
QString dlPlusContentTypeLabel(DLPlusContentType type)
{
    switch (type)
    {
        case DLPlusContentType::ITEM_ALBUM:
            return QStringLiteral("Album");
        case DLPlusContentType::ITEM_TRACKNUMBER:
            return QStringLiteral("Track Number");
        case DLPlusContentType::ITEM_COMPOSITION:
            return QStringLiteral("Composition");
        case DLPlusContentType::ITEM_MOVEMENT:
            return QStringLiteral("Movement");
        case DLPlusContentType::ITEM_CONDUCTOR:
            return QStringLiteral("Conductor");
        case DLPlusContentType::ITEM_COMPOSER:
            return QStringLiteral("Composer");
        case DLPlusContentType::ITEM_BAND:
            return QStringLiteral("Band");
        case DLPlusContentType::ITEM_COMMENT:
            return QStringLiteral("Comment");
        case DLPlusContentType::ITEM_GENRE:
            return QStringLiteral("Genre");
        case DLPlusContentType::INFO_NEWS:
            return QStringLiteral("News");
        case DLPlusContentType::INFO_NEWS_LOCAL:
            return QStringLiteral("News (local)");
        case DLPlusContentType::INFO_STOCKMARKET:
            return QStringLiteral("Stock Market");
        case DLPlusContentType::INFO_SPORT:
            return QStringLiteral("Sport");
        case DLPlusContentType::INFO_LOTTERY:
            return QStringLiteral("Lottery");
        case DLPlusContentType::INFO_HOROSCOPE:
            return QStringLiteral("Horoscope");
        case DLPlusContentType::INFO_DAILY_DIVERSION:
            return QStringLiteral("Daily Diversion");
        case DLPlusContentType::INFO_HEALTH:
            return QStringLiteral("Health");
        case DLPlusContentType::INFO_EVENT:
            return QStringLiteral("Event");
        case DLPlusContentType::INFO_SCENE:
            return QStringLiteral("Scene");
        case DLPlusContentType::INFO_CINEMA:
            return QStringLiteral("Cinema");
        case DLPlusContentType::INFO_TV:
            return QStringLiteral("TV");
        case DLPlusContentType::INFO_WEATHER:
            return QStringLiteral("Weather");
        case DLPlusContentType::INFO_TRAFFIC:
            return QStringLiteral("Traffic");
        case DLPlusContentType::INFO_ALARM:
            return QStringLiteral("Alarm");
        case DLPlusContentType::INFO_ADVERTISEMENT:
            return QStringLiteral("Advertisment");
        case DLPlusContentType::INFO_URL:
            return QStringLiteral("URL");
        case DLPlusContentType::INFO_OTHER:
            return QStringLiteral("Other");
        case DLPlusContentType::STATIONNAME_SHORT:
            return QStringLiteral("Station (short)");
        case DLPlusContentType::STATIONNAME_LONG:
            return QStringLiteral("Station");
        case DLPlusContentType::PROGRAMME_NOW:
            return QStringLiteral("Now");
        case DLPlusContentType::PROGRAMME_NEXT:
            return QStringLiteral("Next");
        case DLPlusContentType::PROGRAMME_PART:
            return QStringLiteral("Programme Part");
        case DLPlusContentType::PROGRAMME_HOST:
            return QStringLiteral("Host");
        case DLPlusContentType::PROGRAMME_EDITORIAL_STAFF:
            return QStringLiteral("Editorial");
        case DLPlusContentType::PROGRAMME_HOMEPAGE:
            return QStringLiteral("Homepage");
        case DLPlusContentType::PHONE_HOTLINE:
            return QStringLiteral("Phone (Hotline)");
        case DLPlusContentType::PHONE_STUDIO:
            return QStringLiteral("Phone (Studio)");
        case DLPlusContentType::PHONE_OTHER:
            return QStringLiteral("Phone (Other)");
        case DLPlusContentType::SMS_STUDIO:
            return QStringLiteral("SMS (Studio)");
        case DLPlusContentType::SMS_OTHER:
            return QStringLiteral("SMS (Other)");
        case DLPlusContentType::EMAIL_HOTLINE:
            return QStringLiteral("E-mail (Hotline)");
        case DLPlusContentType::EMAIL_STUDIO:
            return QStringLiteral("E-mail (Studio)");
        case DLPlusContentType::EMAIL_OTHER:
            return QStringLiteral("E-mail (Other)");
        case DLPlusContentType::MMS_OTHER:
            return QStringLiteral("MMS");
        case DLPlusContentType::CHAT:
            return QStringLiteral("Chat Message");
        case DLPlusContentType::CHAT_CENTER:
            return QStringLiteral("Chat");
        case DLPlusContentType::VOTE_QUESTION:
            return QStringLiteral("Vote Question");
        case DLPlusContentType::VOTE_CENTRE:
            return QStringLiteral("Vote Here");
        default:
            return QString();
    }
}
}  // namespace

Q_LOGGING_CATEGORY(cliApp, "DabCli", QtInfoMsg)

DabCliApp::DabCliApp(const CliConfig &config, QObject *parent) : QObject(parent), m_config(config) {}

DabCliApp::~DabCliApp()
{
    // Stop worker threads first, before destroying the input device.
    if (nullptr != m_radioControlThread)
    {
        if (nullptr != m_radioControl)
        {
            m_radioControl->exit();
        }
        m_radioControlThread->quit();  // this deletes m_radioControl (and stops the dabsdr thread)
        m_radioControlThread->wait();
    }
    if (nullptr != m_audioDecoderThread)
    {
        m_audioDecoderThread->quit();
        m_audioDecoderThread->wait();
    }
    if (nullptr != m_inputDevice)
    {
        delete m_inputDevice;
    }

#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    linuxTeardownMediaRemoteCommands();
#endif
}

bool DabCliApp::start()
{
    // RadioControl runs in its own thread, just like in the GUI application
    m_radioControl = new RadioControl();
    m_radioControlThread = new QThread(this);
    m_radioControlThread->setObjectName("radioControlThr");
    m_radioControl->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_radioControl, &QObject::deleteLater);
    m_radioControlThread->start();

    if (!m_radioControl->init())
    {
        qCCritical(cliApp) << "RadioControl init failed";
        return false;
    }

    m_audioRecorder = new AudioRecorder();

#if HAVE_FDKAAC
    m_audioDecoder = new AudioDecoderFDKAAC(m_audioRecorder);
#elif HAVE_FAAD
    m_audioDecoder = new AudioDecoderFAAD(m_audioRecorder);
#else
#error "Either FAAD or FDK-AAC decoder must be enabled"
#endif
    m_audioDecoderThread = new QThread(this);
    m_audioDecoderThread->setObjectName("audioDecoderThr");
    m_audioDecoder->moveToThread(m_audioDecoderThread);
    m_audioRecorder->moveToThread(m_audioDecoderThread);
    connect(m_audioDecoderThread, &QThread::finished, m_audioDecoder, &QObject::deleteLater);
    connect(m_audioDecoderThread, &QThread::finished, m_audioRecorder, &QObject::deleteLater);
    m_audioDecoderThread->start();

    // RadioControl <-> AudioDecoder wiring (mirrors Application::Application())
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_audioDecoder, &AudioDecoder::start, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioData, m_audioDecoder, &AudioDecoder::decodeData, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::stopAudio, m_audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
    connect(m_audioDecoder, &AudioDecoder::audioParametersInfo, this, &DabCliApp::onAudioParametersInfo, Qt::QueuedConnection);

    if (m_config.webUiEnabled)
    {
        m_audioStreamer = new AudioStreamer(this);
        connect(m_audioDecoder, &AudioDecoder::startAudio, m_audioStreamer, &AudioStreamer::onStartAudio, Qt::QueuedConnection);
        connect(m_audioDecoder, &AudioDecoder::switchAudio, m_audioStreamer, &AudioStreamer::onSwitchAudio, Qt::QueuedConnection);
        connect(m_audioDecoder, &AudioDecoder::stopAudio, m_audioStreamer, &AudioStreamer::onStopAudio, Qt::QueuedConnection);
    }

#if HAVE_PORTAUDIO
    if (!m_config.webUiEnabled)
    {  // no web UI client to stream audio to -> play it locally instead
        m_audioOutput = new CliAudioOutput(this);
        connect(m_audioDecoder, &AudioDecoder::startAudio, m_audioOutput, &CliAudioOutput::onStartAudio, Qt::QueuedConnection);
        connect(m_audioDecoder, &AudioDecoder::switchAudio, m_audioOutput, &CliAudioOutput::onSwitchAudio, Qt::QueuedConnection);
        connect(m_audioDecoder, &AudioDecoder::stopAudio, m_audioOutput, &CliAudioOutput::onStopAudio, Qt::QueuedConnection);
    }
#endif

    connect(m_radioControl, &RadioControl::ensembleInformation, this, &DabCliApp::onEnsembleInformation, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleReconfiguration, this, &DabCliApp::onEnsembleInformation, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListEntry, this, &DabCliApp::onServiceListEntry, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, this, &DabCliApp::onServiceListComplete, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::signalState, this, &DabCliApp::onSignalState, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, this, &DabCliApp::onTuneDone, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, this, &DabCliApp::onAudioServiceSelection, Qt::QueuedConnection);

    // Dynamic label (DLS) + DL Plus (title/artist/...) decoding, mirrors Application::Application()
    m_dlDecoder = new DLDecoder(this);
    connect(m_radioControl, &RadioControl::dlDataGroup_Service, m_dlDecoder, &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_dlDecoder, &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_dlDecoder, &DLDecoder::dlComplete, this, &DabCliApp::onDlComplete, Qt::QueuedConnection);
    connect(m_dlDecoder, &DLDecoder::dlPlusObject, this, &DabCliApp::onDlPlusObject, Qt::QueuedConnection);
    connect(m_dlDecoder, &DLDecoder::resetTerminal, this, &DabCliApp::onDlReset, Qt::QueuedConnection);

    // MOT Slideshow (album art / station image), mirrors Application::Application(); lives in the
    // RadioControl thread, only the resulting Slide is delivered (queued) to the main thread.
    m_slideShowApp = new SlideShowApp();
    m_slideShowApp->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_slideShowApp, &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp, &SlideShowApp::start);
    connect(m_radioControl, &RadioControl::userAppData_Service, m_slideShowApp, &SlideShowApp::onUserAppData);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_slideShowApp, &UserApplication::setEnsId);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp, &UserApplication::setAudioServiceId);
    connect(m_slideShowApp, &SlideShowApp::currentSlide, this, &DabCliApp::onCurrentSlide, Qt::QueuedConnection);

    // advanced/technical info: frequency offset, RS/FIB/MSC decoding stats
    connect(m_radioControl, &RadioControl::freqOffset, this, &DabCliApp::onFreqOffset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::decodingStats, this, &DabCliApp::onDecodingStats, Qt::QueuedConnection);

    connect(this, &DabCliApp::tuneServiceRequested, m_radioControl, &RadioControl::tuneService, Qt::QueuedConnection);

    if (!openInputDevice())
    {
        return false;
    }

    if (m_config.webUiEnabled)
    {
        m_webServer = new WebServer(this, m_audioStreamer, this);
        if (!m_webServer->listen(m_config.bindAddress, m_config.webPort))
        {
            qCCritical(cliApp) << "Failed to start web server on port" << m_config.webPort;
            return false;
        }
        qCInfo(cliApp).noquote() << QString("Web UI listening on http://%1:%2/")
                                        .arg(m_config.bindAddress == QHostAddress::AnyIPv4 ? "0.0.0.0" : m_config.bindAddress.toString())
                                        .arg(m_config.webPort);
    }

    // initial tune, if requested on the command line
    uint32_t freq = m_config.frequencyKHz;
    if (!m_config.channel.isEmpty())
    {
        QString err;
        if (!requestTuneChannel(m_config.channel, &err))
        {
            qCCritical(cliApp).noquote() << err;
            return false;
        }
    }
    else if (freq != 0)
    {
        QString err;
        if (!requestTuneFrequency(freq, &err))
        {
            qCCritical(cliApp).noquote() << err;
            return false;
        }
    }
    else
    {
        qCInfo(cliApp) << "No channel specified, waiting for tune request";
    }

#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    // exposes "now playing" title/artist/state to desktop
    linuxSetupMediaRemoteCommands(this);
    linuxUpdateNowPlayingPlaybackState(m_volumePercent > 0);
#endif

    return true;
}

bool DabCliApp::openInputDevice()
{
    switch (m_config.deviceType)
    {
        case CliDeviceType::RTLSDR:
        {
            RtlSdrInput *dev = new RtlSdrInput();
            m_inputDevice = dev;

            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::deviceReady, this, &DabCliApp::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &DabCliApp::onInputDeviceError, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::rfLevel, this, &DabCliApp::onRfLevel, Qt::QueuedConnection);

            QVariant hwId = m_config.deviceArg.isEmpty() ? QVariant() : QVariant(m_config.deviceArg);
            if (!dev->openDevice(hwId, true))
            {
                qCCritical(cliApp) << "Could not open RTL-SDR device";
                return false;
            }

            if (m_config.manualGainIndex >= 0)
            {
                dev->setGainMode(RtlGainMode::Manual, m_config.manualGainIndex);
            }
            else if (m_config.hardwareAgc)
            {
                dev->setGainMode(RtlGainMode::Hardware);
            }
            else
            {
                dev->setGainMode(RtlGainMode::Software);
            }
            break;
        }
        case CliDeviceType::RTLTCP:
        {
            RtlTcpInput *dev = new RtlTcpInput(m_config.rtlTcpNativeSocket);
            m_inputDevice = dev;

            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::deviceReady, this, &DabCliApp::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &DabCliApp::onInputDeviceError, Qt::QueuedConnection);

            dev->setTcpIp(m_config.rtlTcpHost, m_config.rtlTcpPort, false);
            if (!dev->openDevice())
            {
                qCCritical(cliApp) << "Could not connect to rtl_tcp at" << m_config.rtlTcpHost << ":" << m_config.rtlTcpPort;
                return false;
            }
            break;
        }
    }
    return true;
}

void DabCliApp::onInputDeviceReady()
{
    QMutexLocker lock(&m_mutex);
    m_deviceReady = true;
    qCInfo(cliApp) << "Input device ready";
}

void DabCliApp::onInputDeviceError(InputDevice::ErrorCode errCode)
{
    QMutexLocker lock(&m_mutex);
    m_lastDeviceError = QString("Input device error code %1").arg(int(errCode));
    qCCritical(cliApp).noquote() << m_lastDeviceError;
}

bool DabCliApp::requestTuneChannel(const QString &channelLabel, QString *errorOut)
{
    const QString wanted = channelLabel.trimmed().toUpper();
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        if (it.value().toUpper() == wanted)
        {
            return requestTuneFrequency(it.key(), errorOut);
        }
    }
    if (nullptr != errorOut)
    {
        *errorOut = QString("Unknown DAB channel '%1'").arg(channelLabel);
    }
    return false;
}

bool DabCliApp::requestTuneFrequency(uint32_t freqKHz, QString *errorOut)
{
    if (0 == freqKHz)
    {
        if (nullptr != errorOut)
        {
            *errorOut = QStringLiteral("Invalid frequency");
        }
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_services.clear();
        m_currentFrequency = freqKHz;
        m_currentSid = m_config.haveInitialService ? m_config.initialSid : 0;
        m_currentScids = m_config.haveInitialService ? m_config.initialScids : 0;
        m_currentServiceLabel.clear();
        m_isPlaying = false;
        m_haveAudioParams = false;
    }

    emit tuneServiceRequested(freqKHz, m_config.haveInitialService ? m_config.initialSid : 0,
                               m_config.haveInitialService ? m_config.initialScids : 0);
    return true;
}

bool DabCliApp::requestService(uint32_t sid, uint8_t scids, QString *errorOut)
{
    uint32_t freq;
    {
        QMutexLocker lock(&m_mutex);
        freq = m_currentFrequency;
        if (0 == freq)
        {
            if (nullptr != errorOut)
            {
                *errorOut = QStringLiteral("Not tuned to any channel yet");
            }
            return false;
        }
        if (!m_services.contains(key(sid, scids)))
        {
            if (nullptr != errorOut)
            {
                *errorOut = QStringLiteral("Service not found in current ensemble");
            }
            return false;
        }
    }
    emit tuneServiceRequested(freq, sid, scids);
    return true;
}

void DabCliApp::setVolume(int percent)
{
    percent = std::clamp(percent, 0, 100);
    {
        QMutexLocker lock(&m_mutex);
        m_volumePercent = percent;
    }
#if HAVE_PORTAUDIO
    if (nullptr != m_audioOutput)
    {
        m_audioOutput->setVolume(percent / 100.0f);
    }
#endif
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    linuxUpdateNowPlayingPlaybackState(percent > 0);
#endif
}

int DabCliApp::volumePercent() const
{
    QMutexLocker lock(&m_mutex);
    return m_volumePercent;
}

void DabCliApp::toggleMute()
{
    int current;
    {
        QMutexLocker lock(&m_mutex);
        current = m_volumePercent;
    }
    if (current > 0)
    {
        m_volumeBeforeMute = current;
        setVolume(0);
    }
    else
    {
        setVolume(m_volumeBeforeMute);
    }
}

void DabCliApp::resolvePendingServiceLabel()
{
    if (m_config.initialServiceLabel.isEmpty())
    {
        return;
    }
    const QString wanted = m_config.initialServiceLabel.trimmed();
    uint32_t sid = 0;
    uint8_t scids = 0;
    uint32_t freq = 0;
    bool found = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_currentSid != 0)
        {
            // an explicit SId was already requested / selected - nothing to resolve
            return;
        }
        for (auto it = m_services.cbegin(); it != m_services.cend(); ++it)
        {
            if (it->isAudio && it->label.contains(wanted, Qt::CaseInsensitive))
            {
                sid = it->sid;
                scids = it->scids;
                freq = m_currentFrequency;
                found = true;
                break;
            }
        }
    }
    if (found)
    {
        qCInfo(cliApp).noquote() << QString("Selecting service '%1' matching '%2'").arg(sid, 0, 16).arg(wanted);
        emit tuneServiceRequested(freq, sid, scids);
    }
}

void DabCliApp::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    QMutexLocker lock(&m_mutex);
    m_ensemble = ens;
}

void DabCliApp::onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &s)
{
    QMutexLocker lock(&m_mutex);
    m_ensemble = ens;

    CliServiceInfo info;
    info.sid = s.SId.value();
    info.scids = s.SCIdS;
    info.label = s.label;
    info.subChId = s.SubChId;
    info.subChSize = s.SubChSize;
    info.isAudio = s.isAudioService();
    info.bitRate = info.isAudio ? s.streamAudioData.bitRate : 0;
    m_services.insert(key(info.sid, info.scids), info);
}

void DabCliApp::onServiceListComplete(const RadioControlEnsemble &ens)
{
    int numServices;
    {
        QMutexLocker lock(&m_mutex);
        m_ensemble = ens;
        numServices = m_services.size();
    }
    qCInfo(cliApp) << "Service list complete:" << numServices << "service component(s)";

    resolvePendingServiceLabel();
}

void DabCliApp::onSignalState(uint8_t sync, float snr)
{
    QMutexLocker lock(&m_mutex);
    m_syncLevel = sync;
    m_snr = snr;
}

void DabCliApp::onTuneDone(uint32_t freq)
{
    if (m_shutdownRequested && (0 == freq))
    {
        emit readyToQuit();
    }
}

void DabCliApp::requestShutdown()
{
    if (m_shutdownRequested)
    {
        return;
    }
    m_shutdownRequested = true;

    uint32_t freq;
    {
        QMutexLocker lock(&m_mutex);
        freq = m_currentFrequency;
    }

    if (0 == freq)
    {  // never tuned (or already idle) - nothing to wait for
        emit readyToQuit();
        return;
    }

    qCInfo(cliApp) << "Shutting down - stopping DAB processing...";
    emit tuneServiceRequested(0, 0, 0);
}

void DabCliApp::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    QMutexLocker lock(&m_mutex);
    m_currentSid = s.SId.value();
    m_currentScids = s.SCIdS;
    m_currentServiceLabel = s.label;
    m_isPlaying = true;
    m_haveAudioParams = false;
    m_dlsText.clear();
    m_dlPlusTitle.clear();
    m_dlPlusArtist.clear();
    m_dlPlusTags.clear();
    m_slideData.clear();
    m_slideContentType.clear();
    ++m_slideVersion;
    qCInfo(cliApp).noquote() << "Now playing:" << s.label;
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    linuxUpdateNowPlayingInfo(s.label);
    linuxUpdateNowPlayingSubtitle(QString());
#endif
}

void DabCliApp::onDlComplete(const QString &dl)
{
    QMutexLocker lock(&m_mutex);
    m_dlsText = dl;
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    linuxUpdateNowPlayingSubtitle(buildCurrentSubtitleLocked());
#endif
}

void DabCliApp::onDlPlusObject(const DLPlusObject &object)
{
    QMutexLocker lock(&m_mutex);
    if (DLPlusContentType::ITEM_TITLE == object.getType())
    {
        m_dlPlusTitle = object.isDelete() ? QString() : object.getTag();
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
        linuxUpdateNowPlayingSubtitle(buildCurrentSubtitleLocked());
#endif
        return;
    }
    if (DLPlusContentType::ITEM_ARTIST == object.getType())
    {
        m_dlPlusArtist = object.isDelete() ? QString() : object.getTag();
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
        linuxUpdateNowPlayingSubtitle(buildCurrentSubtitleLocked());
#endif
        return;
    }
    if (object.isDelete())
    {
        m_dlPlusTags.remove(int(object.getType()));
        return;
    }
    const QString label = dlPlusContentTypeLabel(object.getType());
    if (!label.isEmpty())
    {
        m_dlPlusTags[int(object.getType())] = object.getTag();
    }
}

void DabCliApp::onDlReset()
{
    QMutexLocker lock(&m_mutex);
    m_dlsText.clear();
    m_dlPlusTitle.clear();
    m_dlPlusArtist.clear();
    m_dlPlusTags.clear();
#if defined(Q_OS_LINUX) && HAVE_LINUX_DBUS
    linuxUpdateNowPlayingSubtitle(QString());
#endif
}

void DabCliApp::onCurrentSlide(const Slide &slide)
{
    QMutexLocker lock(&m_mutex);
    m_slideData = slide.getRawData();
    const QString &format = slide.getFormat();
    m_slideContentType = format.contains("PNG", Qt::CaseInsensitive) ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    ++m_slideVersion;
}

void DabCliApp::onFreqOffset(float offsetHz)
{
    QMutexLocker lock(&m_mutex);
    m_freqOffsetHz = offsetHz;
}

void DabCliApp::onDecodingStats(const RadioControlDecodingStats &stats)
{
    QMutexLocker lock(&m_mutex);
    m_decodingStats = stats;
    m_haveDecodingStats = true;
}

void DabCliApp::onRfLevel(float level, float gain)
{
    QMutexLocker lock(&m_mutex);
    m_rfLevel = level;
    m_gain = gain;
    m_haveRfLevel = true;
}

void DabCliApp::onAudioParametersInfo(const AudioParameters &params)
{
    QMutexLocker lock(&m_mutex);
    m_audioParams = params;
    m_haveAudioParams = true;
}

void DabCliApp::currentAudioFormat(int *sampleRate, int *numChannels) const
{
    QMutexLocker lock(&m_mutex);
    if (m_haveAudioParams)
    {
        *sampleRate = m_audioParams.sampleRateKHz * 1000;
        *numChannels = m_audioParams.stereo ? 2 : 1;
    }
    else
    {
        *sampleRate = 48000;
        *numChannels = 2;
    }
}

QString DabCliApp::currentStreamTitle() const
{
    QMutexLocker lock(&m_mutex);
    return buildCurrentSubtitleLocked();
}

QString DabCliApp::buildCurrentSubtitleLocked() const
{
    if (!m_dlPlusTitle.isEmpty())
    {
        return m_dlPlusArtist.isEmpty() ? m_dlPlusTitle : QString("%1 - %2").arg(m_dlPlusArtist, m_dlPlusTitle);
    }
    return m_dlsText;
}

bool DabCliApp::currentSlide(QByteArray *data, QString *contentType) const
{
    QMutexLocker lock(&m_mutex);
    if (m_slideData.isEmpty())
    {
        return false;
    }
    *data = m_slideData;
    *contentType = m_slideContentType;
    return true;
}

QJsonObject DabCliApp::statusJson() const
{
    QMutexLocker lock(&m_mutex);

    QJsonObject root;
    root["deviceReady"] = m_deviceReady;
    root["deviceError"] = m_lastDeviceError;
    root["frequencyKHz"] = double(m_currentFrequency);
    root["channel"] = m_currentFrequency ? DabTables::channelList.value(m_currentFrequency, QString()) : QString();
    root["syncLevel"] = m_syncLevel;
    root["snr"] = double(m_snr);
    root["volumePercent"] = m_volumePercent;

    QJsonObject ensembleObj;
    ensembleObj["label"] = m_ensemble.label;
    ensembleObj["labelShort"] = m_ensemble.labelShort;
    ensembleObj["eid"] = m_ensemble.eid();
    ensembleObj["ecc"] = m_ensemble.ecc();
    ensembleObj["valid"] = m_ensemble.isValid();
    root["ensemble"] = ensembleObj;

    QJsonObject currentObj;
    currentObj["sid"] = QString("0x%1").arg(m_currentSid, 0, 16);
    currentObj["scids"] = m_currentScids;
    currentObj["label"] = m_currentServiceLabel;
    currentObj["playing"] = m_isPlaying;
    currentObj["dls"] = m_dlsText;
    currentObj["dlPlusTitle"] = m_dlPlusTitle;
    currentObj["dlPlusArtist"] = m_dlPlusArtist;
    QJsonArray dlPlusTagsArr;
    for (auto it = m_dlPlusTags.constBegin(); it != m_dlPlusTags.constEnd(); ++it)
    {
        QJsonObject tagObj;
        tagObj["label"] = dlPlusContentTypeLabel(static_cast<DLPlusContentType>(it.key()));
        tagObj["text"] = it.value();
        dlPlusTagsArr.append(tagObj);
    }
    currentObj["dlPlusTags"] = dlPlusTagsArr;
    currentObj["slideVersion"] = m_slideData.isEmpty() ? 0 : m_slideVersion;
    if (m_haveAudioParams)
    {
        QJsonObject audioObj;
        audioObj["sampleRate"] = m_audioParams.sampleRateKHz * 1000;
        audioObj["stereo"] = m_audioParams.stereo;
        audioObj["parametricStereo"] = m_audioParams.parametricStereo;
        audioObj["sbr"] = m_audioParams.sbr;
        audioObj["coding"] = int(m_audioParams.coding);
        currentObj["audio"] = audioObj;
    }
    root["current"] = currentObj;

    QJsonObject advancedObj;
    advancedObj["freqOffsetHz"] = double(m_freqOffsetHz);
    if (m_haveRfLevel)
    {
        advancedObj["rfLevel"] = double(m_rfLevel);
        advancedObj["gain"] = double(m_gain);
    }
    if (m_haveDecodingStats)
    {
        QJsonObject statsObj;
        statsObj["fibCntr"] = m_decodingStats.fibCntr;
        statsObj["fibErrorCntr"] = m_decodingStats.fibErrorCntr;
        statsObj["mscCrcOkCntr"] = m_decodingStats.mscCrcOkCntr;
        statsObj["mscCrcErrorCntr"] = m_decodingStats.mscCrcErrorCntr;
        statsObj["audioServiceBytes"] = m_decodingStats.audioServiceBytes;
        statsObj["padBytes"] = m_decodingStats.padBytes;
        statsObj["rsBytes"] = m_decodingStats.rsBytes;
        statsObj["rsBitErrorCntr"] = m_decodingStats.rsBitErrorCntr;
        statsObj["rsUncorrectableCntr"] = m_decodingStats.rsUncorrectableCntr;
        advancedObj["decodingStats"] = statsObj;
    }
    if (m_services.contains(key(m_currentSid, m_currentScids)))
    {
        const CliServiceInfo &svc = m_services.value(key(m_currentSid, m_currentScids));
        advancedObj["subChId"] = svc.subChId;
        advancedObj["subChSize"] = svc.subChSize;
        advancedObj["bitRate"] = svc.bitRate;
    }
    root["advanced"] = advancedObj;

    QJsonArray servicesArr;
    for (auto it = m_services.cbegin(); it != m_services.cend(); ++it)
    {
        if (!it->isAudio)
        {
            continue;
        }
        QJsonObject o;
        o["sid"] = QString("0x%1").arg(it->sid, 0, 16);
        o["scids"] = it->scids;
        o["label"] = it->label;
        o["subChId"] = it->subChId;
        o["subChSize"] = it->subChSize;
        o["bitRate"] = it->bitRate;
        servicesArr.append(o);
    }
    root["services"] = servicesArr;

    return root;
}

QJsonArray DabCliApp::channelsJson()
{
    QJsonArray arr;
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        QJsonObject o;
        o["channel"] = it.value();
        o["frequencyKHz"] = double(it.key());
        arr.append(o);
    }
    return arr;
}
