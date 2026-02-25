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

#include "application.h"

#include <QActionGroup>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMap>
#include <QNetworkProxy>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTextDocument>
#include <QUrl>
#include <QtGlobal>
#include <cstdlib>
#include <iostream>
#ifdef Q_OS_ANDROID
#include <android/log.h>
#include <unistd.h>

#include <QJniObject>
#include <cstring>
#endif

#include "appversion.h"
#include "audiooutputqt.h"
#include "dabtables.h"
#include "epgbackend.h"
#include "epgtime.h"
#include "navigationmodel.h"
#include "radiocontrol.h"
#include "scannerbackend.h"
#include "settingsbackend.h"
#include "tiibackend.h"
#include "updatechecker.h"
#if HAVE_PORTAUDIO
#include "audiooutputpa.h"
#endif
#include "metadatamanager.h"
#if HAVE_FMLIST_INTERFACE
#include "fmlistinterface.h"
#include "txdataloader.h"
#endif
#include "aboutui.h"
#include "bandscanbackend.h"
#include "catslsbackend.h"
#include "dlplusmodel.h"
#include "ensembleinfobackend.h"
#include "logbackend.h"
#include "signalbackend.h"
#include "slsbackend.h"

#if HAVE_QTWIDGETS
#ifdef Q_OS_MAC
#include <QMenu>
#endif
#include <QSystemTrayIcon>
#endif

// Input devices
#include "rawfileinput.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#if HAVE_AIRSPY
#include "airspyinput.h"
#endif
#if HAVE_SOAPYSDR
#include "sdrplayinput.h"
#include "soapysdrinput.h"
#endif
#if HAVE_RARTTCP
#include "rarttcpinput.h"
#endif

#if HAVE_FDKAAC
#include "audiodecoderfdkaac.h"
#endif
#if HAVE_FAAD
#include "audiodecoderfaad.h"
#endif

#ifdef Q_OS_MACOS
#include "mac.h"
#endif

Q_LOGGING_CATEGORY(application, "Application", QtInfoMsg)

const QString Application::appName("AbracaDABra");
const char *Application::syncLevelLabels[] = {QT_TR_NOOP("No signal"), QT_TR_NOOP("Signal found"), QT_TR_NOOP("Sync")};
const char *Application::syncLevelTooltip[] = {QT_TR_NOOP("DAB signal not detected<br>Looking for signal..."),
                                               QT_TR_NOOP("Found DAB signal,<br>trying to synchronize..."), QT_TR_NOOP("Synchronized to DAB signal")};
const QStringList Application::snrProgressStylesheet = {
    QString::fromUtf8("QProgressBar::chunk {background-color: #ff4b4b; }"),  // red
    QString::fromUtf8("QProgressBar::chunk {background-color: #ffb527; }"),  // yellow
    QString::fromUtf8("QProgressBar::chunk {background-color: #5bc214; }")   // green
};
const QString Application::slsDumpPatern("SLS/{serviceId}/{contentNameWithExt}");
const QString Application::spiDumpPatern("SPI/{ensId}/{scId}_{directoryId}/{contentName}");

int const Application::EXIT_CODE_RESTART = -123456789;

static LogModel *logModel;

// this is default log handler printing the sam format to log windows and to stderr
void logToModelHandlerDefault(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timeStamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    QString txt;
    switch (type)
    {
        case QtDebugMsg:
            txt = QString("%1 [D] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtInfoMsg:
            txt = QString("%1 [I] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtWarningMsg:
            txt = QString("%1 [W] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtCriticalMsg:
            txt = QString("%1 [C] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtFatalMsg:
            txt = QString("%1 [F] %2: %3").arg(timeStamp, context.category, msg);
            break;
    }

    QMetaObject::invokeMethod(logModel, "appendRow", Qt::QueuedConnection, Q_ARG(QString, txt), Q_ARG(int, type));

    std::cerr << txt.toStdString() << std::endl;
#ifdef Q_OS_ANDROID
    // logToAndroidLogger(txt);
    QByteArray localMsg = txt.toLocal8Bit();
    __android_log_print(ANDROID_LOG_INFO, "AbracaDABra", "%s", localMsg.constData());
#endif
}

// this is custom log handler printing default format to log window and custom format to stderr
void logToModelHandlerCustom(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timeStamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    QString txt;

    std::cerr << qFormatLogMessage(type, context, msg).toStdString() << std::endl;

    switch (type)
    {
        case QtDebugMsg:
            txt = QString("%1 [D] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtInfoMsg:
            txt = QString("%1 [I] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtWarningMsg:
            txt = QString("%1 [W] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtCriticalMsg:
            txt = QString("%1 [C] %2: %3").arg(timeStamp, context.category, msg);
            break;
        case QtFatalMsg:
            txt = QString("%1 [F] %2: %3").arg(timeStamp, context.category, msg);
            break;
    }

    QMetaObject::invokeMethod(logModel, "appendRow", Qt::QueuedConnection, Q_ARG(QString, txt), Q_ARG(int, type));
}

void setLogToModel(QAbstractItemModel *model)
{
    logModel = dynamic_cast<LogModel *>(model);

    if (qEnvironmentVariable("QT_MESSAGE_PATTERN", "").isEmpty())
    {
        qInstallMessageHandler(logToModelHandlerDefault);
    }
    else
    {
        qInstallMessageHandler(logToModelHandlerCustom);
    }
}

Application::Application(const QString &iniFilename, const QString &iniSlFilename, QObject *parent)
    : QObject(parent),
      m_ui(new ApplicationUI(this)),
      m_iniFilename(iniFilename),
      m_serviceListFilename(iniSlFilename),
      m_qmlEngine(new QQmlApplicationEngine),
      m_navigationModel(new NavigationModel(this)),
      m_messageBoxBackend(new MessageBoxBackend(this))
{
    m_settings = new Settings();

#if HAVE_FMLIST_INTERFACE
    m_fmlistInterface = nullptr;
#endif
    m_dlDecoder[Instance::Service] = new DLDecoder();
    m_dlDecoder[Instance::Announcement] = new DLDecoder();

    m_dlPlusModel[Instance::Service] = new DLPlusModel(this);
    m_dlPlusModel[Instance::Announcement] = new DLPlusModel(this);

    m_slsBackend[Instance::Service] = new SLSBackend(m_settings, this);
    m_slsBackend[Instance::Announcement] = new SLSBackend(m_settings, this);

    // creating log windows as soon as possible
    m_logBackend = new LogBackend(m_settings, this);
    connect(m_logBackend, &QObject::destroyed, this, []() { logModel = nullptr; });
    setLogToModel(m_logBackend->logModel());

    qCInfo(application, "Version: %s", PROJECT_VER);

#ifdef Q_OS_ANDROID
    // Initialize Android audio service helper for background playback
    QJniObject::callStaticMethod<void>("org/qtproject/abracadabra/AudioServiceHelper", "initializeContext", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());
#endif

    connect(m_ui, &ApplicationUI::isSystemDarkModeChanged, this, &Application::setColorTheme);

    connect(m_navigationModel, &NavigationModel::isUndockedChanged, this, &Application::pageUndocked);
    connect(m_navigationModel, &NavigationModel::isActiveChanged, this, &Application::pageActive);

    m_inputDeviceRecorder = new InputDeviceRecorder(m_settings);

    m_settingsBackend = new SettingsBackend(m_qmlEngine, this);
    connect(m_settingsBackend, &SettingsBackend::inputDeviceChanged, this, &Application::changeInputDevice);
    m_settingsBackend->setSlsDumpPaternDefault(slsDumpPatern);
    m_settingsBackend->setSpiDumpPaternDefault(spiDumpPatern);
    connect(m_settingsBackend, &SettingsBackend::showSystemTimeChanged, this, &Application::onShowSystemTimeChanged);
    connect(m_settingsBackend, &SettingsBackend::showEnsembleCountryFlagChanged, this, &Application::onShowCountryFlagChanged);
    connect(m_settingsBackend, &SettingsBackend::showServiceCountryFlagChanged, this, &Application::onShowCountryFlagChanged);
    connect(m_settingsBackend, &SettingsBackend::newAnnouncementSettings, this, &Application::onNewAnnouncementSettings);
    connect(m_settingsBackend, &SettingsBackend::xmlHeaderToggled, m_inputDeviceRecorder, &InputDeviceRecorder::setXmlHeaderEnabled);
    connect(m_settingsBackend, &SettingsBackend::proxySettingsChanged, this, &Application::setProxy);
    connect(m_settingsBackend, &SettingsBackend::spiIconSettingsChanged, this, &Application::onSpiProgressSettingsChanged);
    connect(m_settingsBackend, &SettingsBackend::applicationStyleChanged, this, &Application::setColorTheme);
    connect(m_settingsBackend, &SettingsBackend::compactUiChanged, this, [this]() { m_ui->isCompact(m_settings->compactUi); });
    connect(m_settingsBackend, &SettingsBackend::restartRequested, this,
            [this]()
            {
                m_exitCode = Application::EXIT_CODE_RESTART;
                close();
            });

    m_ensembleInfoBackend = new EnsembleInfoBackend(this);
    connect(m_ensembleInfoBackend, &EnsembleInfoBackend::recordingStart, m_inputDeviceRecorder, &InputDeviceRecorder::start);
    connect(m_ensembleInfoBackend, &EnsembleInfoBackend::recordingStop, m_inputDeviceRecorder, &InputDeviceRecorder::stop);

    connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_ensembleInfoBackend, &EnsembleInfoBackend::onRecording);
    connect(m_inputDeviceRecorder, &InputDeviceRecorder::bytesRecorded, m_ensembleInfoBackend, &EnsembleInfoBackend::updateRecordingStatus,
            Qt::QueuedConnection);

#if HAVE_FMLIST_INTERFACE
    m_fmlistInterface = new FMListInterface(PROJECT_VER, TxDataLoader::dbfile());
    connect(m_settingsBackend, &SettingsBackend::updateTxDb, this,
            [this]()
            {
                qCInfo(application, "Updating TX database (library version %s)", m_fmlistInterface->version().toUtf8().data());
                m_fmlistInterface->updateTiiData();
            });
    connect(m_fmlistInterface, &FMListInterface::updateTiiDataFinished, m_settingsBackend, &SettingsBackend::onTiiUpdateFinished);
    connect(m_fmlistInterface, &FMListInterface::updateTiiDataFinished, this,
            [this](QNetworkReply::NetworkError err)
            {
                if (QNetworkReply::NoError == err)
                {
                    qCInfo(application) << "TII database updated";
                    if (m_tiiBackend != nullptr)
                    {
                        m_tiiBackend->updateTxTable();
                    }
                    if (m_scannerBackend != nullptr)
                    {
                        m_scannerBackend->updateTxTable();
                    }
                }
                else
                {
                    qCWarning(application) << "Failed to update TII database";
                }
            });

    connect(m_fmlistInterface, &FMListInterface::ensembleCsvUploaded, this,
            [this](QNetworkReply::NetworkError err)
            {
                if (err == QNetworkReply::NoError)
                {
                    qCInfo(application) << "Ensemble information uploaded, thank you!";
                    m_ensembleInfoBackend->setEnsembleInfoUploaded(true);
                }
                else
                {
                    qCWarning(application) << "Ensemble information upload failed, error code:" << err;
                    m_ensembleInfoBackend->setEnsembleInfoUploaded(false);
                }
            });
#endif

    m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);

    // load audio setting from ini file
    Settings::AudioFramework audioFramework;
    Settings::AudioDecoder audioDecoder;
    getAudioSettings(audioFramework, audioDecoder);

#ifdef Q_OS_LINUX
    if (Settings::AudioFramework::Qt == audioFramework)
#endif
    {
        m_audioOutputMenuModel = new ContextMenuModel(this);
        connect(m_audioOutputMenuModel, &ContextMenuModel::actionTriggered, this, &Application::handleAudioOutputSelection);
    }

    onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);

    connect(m_ui, &ApplicationUI::audioVolumeChanged, this, &Application::onAudioVolumeChanged);

#if HAVE_QTWIDGETS && defined(Q_OS_MAC)
    QMenu *dockMenu = new QMenu();
    QAction *muteAction = new QAction(tr("Mute"), dockMenu);
    dockMenu->addAction(muteAction);
    connect(m_ui, &ApplicationUI::isMutedChanged, this, [muteAction, this]() { muteAction->setText(m_ui->isMuted() ? tr("Unmute") : tr("Mute")); });
    connect(muteAction, &QAction::triggered, this, [this]() { onMuteButtonToggled(!m_ui->isMuted()); });
    connect(this, &QObject::destroyed, dockMenu, &QMenu::deleteLater);
    dockMenu->setAsDockMenu();
#endif

    // service list
    m_serviceList = new ServiceList;

    // metadata
    m_metadataManager = new MetadataManager(m_serviceList, this);
    connect(m_metadataManager, &MetadataManager::dataUpdated, this, &Application::onMetadataUpdated);
    connect(m_serviceList, &ServiceList::serviceAddedToEnsemble, m_metadataManager, &MetadataManager::addServiceEpg);
    connect(m_serviceList, &ServiceList::serviceRemoved, m_metadataManager, &MetadataManager::removeServiceEpg);
    connect(m_serviceList, &ServiceList::empty, m_metadataManager, &MetadataManager::clearEpg);
    connect(EPGTime::getInstance(), &EPGTime::haveValidTime, m_metadataManager, &MetadataManager::getEpgData);

    m_slModel = new SLModel(m_serviceList, m_metadataManager, this);
    m_slSelectionModel = new QItemSelectionModel(m_slModel, this);

    connect(m_serviceList, &ServiceList::serviceAdded, m_slModel, &SLModel::addService);
    connect(m_serviceList, &ServiceList::serviceUpdated, m_slModel, &SLModel::updateService);
    connect(m_serviceList, &ServiceList::serviceRemoved, m_slModel, &SLModel::removeService);
    connect(m_serviceList, &ServiceList::empty, m_slModel, &SLModel::clear);
    connect(m_slSelectionModel, &QItemSelectionModel::selectionChanged, this, &Application::onServiceListSelection);

    m_slTreeModel = new SLTreeModel(m_serviceList, m_metadataManager, this);
    m_slTreeSelectionModel = new QItemSelectionModel(m_slTreeModel, this);

    connect(m_serviceList, &ServiceList::serviceAddedToEnsemble, m_slTreeModel, &SLTreeModel::addEnsembleService);
    connect(m_serviceList, &ServiceList::serviceUpdatedInEnsemble, m_slTreeModel, &SLTreeModel::updateEnsembleService);
    connect(m_serviceList, &ServiceList::serviceRemovedFromEnsemble, m_slTreeModel, &SLTreeModel::removeEnsembleService);
    connect(m_serviceList, &ServiceList::ensembleRemoved, m_slTreeModel, &SLTreeModel::removeEnsemble);

    connect(m_slTreeSelectionModel, &QItemSelectionModel::selectionChanged, this, &Application::onServiceListTreeSelection);
    connect(m_serviceList, &ServiceList::empty, m_slTreeModel, &SLTreeModel::clear);

    // EPG
    connect(m_metadataManager, &MetadataManager::epgAvailable, this, [this]() { m_navigationModel->setEnabled(NavigationModel::Epg, true); });
    connect(m_metadataManager, &MetadataManager::epgEmpty, this, &Application::onEpgEmpty);

    // fill channel list
    m_channelListModel = new DABChannelListFilteredModel(this);
    m_channelListModel->setSourceModel(new DABChannelListModel(this));
    setChannelIndex(-1);
    m_ui->tuneEnabled(false);
    connect(m_settingsBackend, &SettingsBackend::cableChannelsEnaChanged, this,
            [this]() { m_channelListModel->setCableChannelFilter(m_settings->cableChannelsEna); });

    // disable service list - it is enabled when some valid device is selected1
    m_ui->serviceSelectionEnabled(false);
    m_ui->isServiceSelected(false);

    clearEnsembleInformationLabels();
    clearServiceInformationLabels();

    m_ui->serviceInstance(Instance::Service);
    m_ui->audioEncodingLabelToolTip(tr("Audio coding"));

    m_slsBackend[Instance::Service]->reset();
    m_slsBackend[Instance::Announcement]->reset();

    m_ui->announcementButtonTooltip(tr("Ongoing announcement"));
    m_ui->isAnnouncementOngoing(false);
    m_ui->isAnnouncementActive(false);
    m_navigationModel->setEnabled(NavigationModel::CatSls, false);
    m_navigationModel->setEnabled(NavigationModel::Epg, false);

    m_ui->isSpiProgressEnsVisible(false);

    m_serviceSourcesMenuModel = new ContextMenuModel(this);
    connect(m_serviceSourcesMenuModel, &ContextMenuModel::checkStateChanged, this, &Application::handleServiceSourceSelection, Qt::QueuedConnection);

    // threads
    m_radioControl = new RadioControl();
    m_radioControlThread = new QThread(this);
    m_radioControlThread->setObjectName("radioControlThr");
    m_radioControl->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_radioControl, &QObject::deleteLater);
    m_radioControlThread->start();

    // initialize radio control
    if (!m_radioControl->init())
    {
        qCFatal(application) << "RadioControl() init failed";
        ::exit(1);
    }

    AudioRecorder *audioRecorder = new AudioRecorder();

#if (!HAVE_FAAD)
    m_audioDecoder = new AudioDecoderFDKAAC(audioRecorder);
#elif (!HAVE_FDKAAC)
    m_audioDecoder = new AudioDecoderFAAD(audioRecorder);
#else
    if (Settings::AudioDecoder::FDKAAC == audioDecoder)
    {
        m_audioDecoder = new AudioDecoderFDKAAC(audioRecorder);
        qCInfo(application) << "AAC audio decoder: FDK-AAC";
    }
    else
    {
        m_audioDecoder = new AudioDecoderFAAD(audioRecorder);
        qCInfo(application) << "AAC audio decoder: FAAD";
    }
#endif
    m_audioDecoderThread = new QThread(this);
    m_audioDecoderThread->setObjectName("audioDecoderThr");
    m_audioDecoder->moveToThread(m_audioDecoderThread);
    audioRecorder->moveToThread(m_audioDecoderThread);
    connect(m_audioDecoderThread, &QThread::finished, m_audioDecoder, &QObject::deleteLater);
    connect(m_audioDecoderThread, &QThread::finished, audioRecorder, &QObject::deleteLater);
    m_audioDecoderThread->start();

    m_audioRecScheduleModel = new AudioRecScheduleModel(this);
    m_audioRecManager = new AudioRecManager(m_audioRecScheduleModel, m_slModel, audioRecorder, m_settings, this);

    connect(m_audioRecManager, &AudioRecManager::audioRecordingStarted, this, &Application::onAudioRecordingStarted);
    connect(m_audioRecManager, &AudioRecManager::audioRecordingStopped, this, &Application::onAudioRecordingStopped);
    connect(m_audioRecManager, &AudioRecManager::audioRecordingProgress, this, &Application::onAudioRecordingProgress);
    connect(m_audioRecManager, &AudioRecManager::audioRecordingCountdown, this, &Application::onAudioRecordingCountdown, Qt::QueuedConnection);
    connect(m_audioRecManager, &AudioRecManager::requestServiceSelection, this, &Application::selectService);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_audioRecManager, &AudioRecManager::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::noiseConcealmentLevelChanged, m_audioDecoder, &AudioDecoder::setNoiseConcealment,
            Qt::QueuedConnection);
    connect(this, &Application::audioStop, m_audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::audioRecordingSettings, audioRecorder, &AudioRecorder::setup, Qt::QueuedConnection);

    onAudioRecordingStopped();

#if (HAVE_PORTAUDIO)
    if (Settings::AudioFramework::Pa == audioFramework)
    {
        m_audioOutput = new AudioOutputPa();
#ifndef Q_OS_LINUX
        connect(m_audioOutput, &AudioOutput::audioDevicesList, this, &Application::onAudioDevicesList);
        connect(m_audioOutput, &AudioOutput::audioDeviceChanged, this, &Application::onAudioDeviceChanged);
        connect(this, &Application::audioOutput, m_audioOutput, &AudioOutput::setAudioDevice);
        onAudioDevicesList(m_audioOutput->getAudioDevices());

        qCInfo(application) << "Audio output: PortAudio";
#endif
    }
    else
#endif
    {
        m_audioOutput = new AudioOutputQt();
        connect(m_audioOutput, &AudioOutput::audioDevicesList, this, &Application::onAudioDevicesList);
        connect(m_audioOutput, &AudioOutput::audioDeviceChanged, this, &Application::onAudioDeviceChanged);
        connect(static_cast<AudioOutputQt *>(m_audioOutput), &AudioOutputQt::audioOutputError, this, &Application::onAudioOutputError,
                Qt::QueuedConnection);
        connect(this, &Application::audioOutput, m_audioOutput, &AudioOutput::setAudioDevice, Qt::QueuedConnection);
        onAudioDevicesList(m_audioOutput->getAudioDevices());
        m_audioOutputThread = new QThread(this);
        m_audioOutputThread->setObjectName("audioOutThr");
        m_audioOutput->moveToThread(m_audioOutputThread);
        connect(m_audioOutputThread, &QThread::finished, m_audioOutput, &QObject::deleteLater);
        m_audioOutputThread->start();

        qCInfo(application) << "Audio output: Qt";
    }
    connect(this, &Application::audioVolume, m_audioOutput, &AudioOutput::setVolume, Qt::QueuedConnection);
    connect(this, &Application::audioMute, m_audioOutput, &AudioOutput::mute, Qt::QueuedConnection);

    // Connect signals
    connect(m_radioControl, &RadioControl::ensembleInformation, this, &Application::onEnsembleInfo, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleReconfiguration, this, &Application::onEnsembleReconfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, this, &Application::onServiceListComplete, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::signalState, this, &Application::onSignalState, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dabTime, this, &Application::onDabTime, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListEntry, this, &Application::onServiceListEntry, Qt::BlockingQueuedConnection);
    connect(m_radioControl, &RadioControl::announcement, this, &Application::onAnnouncement, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::programmeTypeChanged, this, &Application::onProgrammeTypeChanged, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleCSV_FMLIST, this, &Application::uploadEnsembleCSV, Qt::QueuedConnection);
    connect(this, &Application::announcementMask, m_radioControl, &RadioControl::setupAnnouncements, Qt::QueuedConnection);
    connect(m_audioOutput, &AudioOutput::audioOutputRestart, m_radioControl, &RadioControl::onAudioOutputRestart, Qt::QueuedConnection);
    connect(this, &Application::toggleAnnouncement, m_radioControl, &RadioControl::suspendResumeAnnouncement, Qt::QueuedConnection);
    connect(this, &Application::getEnsembleInfo, m_radioControl, &RadioControl::getEnsembleInformation, Qt::QueuedConnection);
    connect(m_ensembleInfoBackend, &EnsembleInfoBackend::requestEnsembleConfiguration, m_radioControl, &RadioControl::getEnsembleConfiguration,
            Qt::QueuedConnection);
    connect(m_ensembleInfoBackend, &EnsembleInfoBackend::requestEnsembleCSV, m_radioControl, &RadioControl::getEnsembleCSV, Qt::QueuedConnection);
    connect(m_ensembleInfoBackend, &EnsembleInfoBackend::requestUploadCVS, m_radioControl, &RadioControl::getEnsembleCSV_FMLIST,
            Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::signalState, m_ensembleInfoBackend, &EnsembleInfoBackend::updateSnr, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::freqOffset, m_ensembleInfoBackend, &EnsembleInfoBackend::updateFreqOffset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleConfiguration, m_ensembleInfoBackend, &EnsembleInfoBackend::refreshEnsembleConfiguration,
            Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceComponentsList, m_ensembleInfoBackend, &EnsembleInfoBackend::onServiceComponentsList,
            Qt::QueuedConnection);
    // connect(m_radioControl, &RadioControl::ensembleCSV, m_ensembleInfoDialog, &EnsembleInfoDialog::onEnsembleCSV, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleCSV, m_ensembleInfoBackend, &EnsembleInfoBackend::onEnsembleCSV, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, m_ensembleInfoBackend, &EnsembleInfoBackend::newFrequency, Qt::QueuedConnection);

    connect(m_radioControl, &RadioControl::tuneDone, m_inputDeviceRecorder, &InputDeviceRecorder::setCurrentFrequency, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::decodingStats, m_ensembleInfoBackend, &EnsembleInfoBackend::updatedDecodingStats, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_ensembleInfoBackend, &EnsembleInfoBackend::serviceChanged, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_ensembleInfoBackend, &EnsembleInfoBackend::onEnsembleInformation,
            Qt::QueuedConnection);

    connect(m_radioControl, &RadioControl::dlDataGroup_Service, m_dlDecoder[Instance::Service], &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dlDataGroup_Announcement, m_dlDecoder[Instance::Announcement], &DLDecoder::newDataGroup,
            Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, this, &Application::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_dlDecoder[Instance::Service], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_dlDecoder[Instance::Announcement], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioData, m_audioDecoder, &AudioDecoder::decodeData, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::tiiModeChanged, m_radioControl, &RadioControl::setTii, Qt::QueuedConnection);

    // service stopped
    connect(m_radioControl, &RadioControl::ensembleRemoved, this, &Application::onEnsembleRemoved, Qt::QueuedConnection);

    // reconfiguration
    connect(m_radioControl, &RadioControl::audioServiceReconfiguration, this, &Application::onAudioServiceReconfiguration, Qt::QueuedConnection);
    connect(this, &Application::getAudioInfo, m_audioDecoder, &AudioDecoder::getAudioParameters, Qt::QueuedConnection);

    // DL(+)
    // normal service
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlComplete, this, &Application::onDLComplete_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlComplete, m_audioRecManager, &AudioRecManager::onDLComplete);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlPlusObject, this, &Application::onDLPlusObjReceived_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlItemToggle, this, &Application::onDLPlusItemToggle_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlItemRunning, this, &Application::onDLPlusItemRunning_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::resetTerminal, this, &Application::onDLReset_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::resetTerminal, m_audioRecManager, &AudioRecManager::onDLReset);
    // announcement
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlComplete, this, &Application::onDLComplete_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlPlusObject, this, &Application::onDLPlusObjReceived_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlItemToggle, this, &Application::onDLPlusItemToggle_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlItemRunning, this, &Application::onDLPlusItemRunning_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::resetTerminal, this, &Application::onDLReset_Announcement);

    connect(m_audioDecoder, &AudioDecoder::audioParametersInfo, this, &Application::onAudioParametersInfo, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_audioDecoder, &AudioDecoder::start, Qt::QueuedConnection);

    // audio output is controlled by signals from decoder
    connect(m_radioControl, &RadioControl::stopAudio, m_audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
    connect(m_audioDecoder, &AudioDecoder::startAudio, m_audioOutput, &AudioOutput::start, Qt::QueuedConnection);
    connect(m_audioDecoder, &AudioDecoder::switchAudio, m_audioOutput, &AudioOutput::restart, Qt::QueuedConnection);
    connect(m_audioDecoder, &AudioDecoder::stopAudio, m_audioOutput, &AudioOutput::stop, Qt::QueuedConnection);

    // tune procedure:
    // 1. mainwindow tune -> radiocontrol tune (this stops DAB SDR - tune to 0)
    // 2. radiocontrol tuneInputDevice -> inputdevice tune (reset of input bufer and tune FE)
    // 3. inputDevice tuned -> radiocontrol start (this starts DAB SDR)
    // 4. notification to HMI
    connect(this, &Application::serviceRequest, m_radioControl, &RadioControl::tuneService, Qt::QueuedConnection);

    // these two signals have to be connected in initInputDevice() - left here as comment
    // connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
    // connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, this, &Application::onTuneDone, Qt::QueuedConnection);

    connect(this, &Application::exit, m_radioControl, &RadioControl::exit, Qt::QueuedConnection);

    // user applications

    // slide show application is created by default
    // ETSI TS 101 499 V3.1.1  [5.1.1]
    // The application should be automatically started when a SlideShow service is discovered for the current radio service
    m_slideShowApp[Instance::Service] = new SlideShowApp();
    m_slideShowApp[Instance::Announcement] = new SlideShowApp();

    m_slideShowApp[Instance::Service]->moveToThread(m_radioControlThread);
    m_slideShowApp[Instance::Announcement]->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_slideShowApp[Instance::Service], &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp[Instance::Service], &SlideShowApp::start);
    connect(m_radioControl, &RadioControl::userAppData_Service, m_slideShowApp[Instance::Service], &SlideShowApp::onUserAppData);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_slideShowApp[Instance::Service], &UserApplication::setEnsId);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp[Instance::Service], &UserApplication::setAudioServiceId);

    // connect(this, &MainWindow::serviceRequest, m_metadataManager, &MetadataManager::onServiceRequest);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::currentSlide, m_slsBackend[Instance::Service], &SLSBackend::showSlide,
            Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::resetTerminal, m_slsBackend[Instance::Service], &SLSBackend::reset,
            Qt::QueuedConnection);
    connect(
        m_slideShowApp[Instance::Service], &SlideShowApp::catSlsAvailable, this,
        [this]() { m_navigationModel->setEnabled(NavigationModel::CatSls, true); }, Qt::QueuedConnection);

    connect(this, &Application::stopUserApps, m_slideShowApp[Instance::Service], &SlideShowApp::stop, Qt::QueuedConnection);

    connect(m_radioControlThread, &QThread::finished, m_slideShowApp[Instance::Announcement], &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp[Instance::Announcement], &SlideShowApp::start);
    connect(m_radioControl, &RadioControl::userAppData_Announcement, m_slideShowApp[Instance::Announcement], &SlideShowApp::onUserAppData);
    connect(m_slideShowApp[Instance::Announcement], &SlideShowApp::currentSlide, m_slsBackend[Instance::Announcement], &SLSBackend::showSlide,
            Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Announcement], &SlideShowApp::resetTerminal, m_slsBackend[Instance::Announcement], &SLSBackend::reset,
            Qt::QueuedConnection);

    connect(m_radioControl, &RadioControl::announcement, m_slsBackend[Instance::Announcement], &SLSBackend::showAnnouncement, Qt::QueuedConnection);
    connect(this, &Application::stopUserApps, m_slideShowApp[Instance::Announcement], &SlideShowApp::stop, Qt::QueuedConnection);

    connect(m_settingsBackend, &SettingsBackend::uaDumpSettings, m_slideShowApp[Instance::Service], &SlideShowApp::setDataDumping,
            Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::uaDumpSettings, m_slideShowApp[Instance::Announcement], &SlideShowApp::setDataDumping,
            Qt::QueuedConnection);

    m_spiApp = new SPIApp();
    m_spiApp->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_spiApp, &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::userAppData_Service, m_spiApp, &SPIApp::onUserAppData);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_spiApp, &SPIApp::start);
    connect(this, &Application::stopUserApps, m_spiApp, &SPIApp::stop, Qt::QueuedConnection);
    connect(this, &Application::resetUserApps, m_spiApp, &SPIApp::reset, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::decodingStart, this, [this](bool isEns) { onSpiProgress(isEns, 0, 0); }, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::decodingProgress, this, &Application::onSpiProgress, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::uaDumpSettings, m_spiApp, &SPIApp::setDataDumping, Qt::QueuedConnection);

    connect(m_spiApp, &SPIApp::xmlDocument, m_metadataManager, &MetadataManager::processXML, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getSI, m_spiApp, &SPIApp::getSI, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getPI, m_spiApp, &SPIApp::getPI, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getFile, m_spiApp, &SPIApp::onFileRequest, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::requestedFile, m_metadataManager, &MetadataManager::onFileReceived, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::radioDNSAvailable, m_metadataManager, &MetadataManager::getEpgData);
    connect(m_spiApp, &SPIApp::radioDNSAvailable, m_metadataManager, &MetadataManager::getSiData);
    connect(m_settingsBackend, &SettingsBackend::spiApplicationEnabled, m_radioControl, &RadioControl::onSpiApplicationEnabled, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::spiApplicationEnabled, m_spiApp, &SPIApp::enable, Qt::QueuedConnection);
    connect(m_settingsBackend, &SettingsBackend::spiApplicationSettingsChanged, m_spiApp, &SPIApp::onSettingsChanged, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_metadataManager, &MetadataManager::onEnsembleInformation, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_metadataManager, &MetadataManager::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_spiApp, &UserApplication::setEnsId);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_spiApp, &UserApplication::setAudioServiceId);

    // connect(m_setupDialog, &SetupDialog::slsBgChanged, ui->slsView_Service, &SLSView::setBgColor);
    // connect(m_setupDialog, &SetupDialog::slsBgChanged, ui->slsView_Announcement, &SLSView::setBgColor);

    connect(m_settingsBackend, &SettingsBackend::slsBackgroundColorChanged, this,
            [this]()
            {
                m_slsBackend[Instance::Service]->setBgColor(m_settings->slsBackground);
                m_slsBackend[Instance::Announcement]->setBgColor(m_settings->slsBackground);
            });

    // this backedn needs to exist to collect slides
    createCatSlsBackend();

    // input device connections
    initInputDevice(InputDevice::Id::UNDEFINED, QVariant());

    // Handle application state changes for saving settings on Android
    // When the app is swiped out on Android, applicationStateChanged signal is emitted
    // before the app is terminated, allowing us to save settings
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &Application::onApplicationStateChanged);

    // Intercept application-level Quit (dock/taskbar/system) so we can route through Application::close()
    qApp->installEventFilter(this);

    loadSettings();

    // set application style after loading settings
    setColorTheme();

    // this causes focus to be set to service list when tune is finished
    m_hasListViewFocus = true;
    m_hasTreeViewFocus = false;

    setContextProperties();
    m_qmlEngine->addImageProvider(QLatin1String("metadata"), new Application::LogoProvider(m_metadataManager));
    m_qmlEngine->addImageProvider(QLatin1String("slsService"), m_slsBackend[Instance::Service]->provider());
    m_qmlEngine->addImageProvider(QLatin1String("slsAnnouncement"), m_slsBackend[Instance::Announcement]->provider());
    m_qmlEngine->loadFromModule("abracaComponents", "Main");

    QTimer::singleShot(5000, this, &Application::checkForUpdate);

#ifdef Q_OS_ANDROID
    // Acquire wake lock to keep all application threads active in background
    // This protects: RadioControl, AudioDecoder, AudioOutput, InputDevice threads, and any other worker threads
    // The wake lock is held for the lifetime of the application
    QJniObject::callStaticMethod<void>("org/qtproject/abracadabra/AudioServiceHelper", "acquireAppWakeLock", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());

#endif
}

Application::~Application()
{
    // Stop all worker threads FIRST, before deleting the QML engine
    // This ensures no threads are running when we destroy objects

    m_radioControlThread->quit();  // this deletes radioControl
    m_radioControlThread->wait();
    delete m_radioControlThread;

    m_audioDecoderThread->quit();  // this deletes audiodecoder
    m_audioDecoderThread->wait();
    delete m_audioDecoderThread;

    if (nullptr != m_audioOutputThread)
    {                                 // Qt audio
        m_audioOutputThread->quit();  // this deletes audiooutput
        m_audioOutputThread->wait();
        delete m_audioOutputThread;
    }
    else
    {  // PortAudio
        delete m_audioOutput;
    }

    // Now delete the QML engine to stop rendering threads
    delete m_qmlEngine;
    m_qmlEngine = nullptr;

    // Delete serviceList and metadataManager before navigationModel
    // to avoid use-after-free during signal emission in destructors
    delete m_serviceList;
    m_serviceList = nullptr;

    delete m_metadataManager;
    m_metadataManager = nullptr;

    delete m_navigationModel;
    m_navigationModel = nullptr;

#if HAVE_FMLIST_INTERFACE
    if (nullptr != m_fmlistInterface)
    {
        delete m_fmlistInterface;
        m_fmlistInterface = nullptr;
    }
#endif
    delete m_inputDevice;
    delete m_inputDeviceRecorder;

    delete m_dlDecoder[Instance::Service];
    delete m_dlDecoder[Instance::Announcement];
    delete m_dlPlusModel[Instance::Service];
    delete m_dlPlusModel[Instance::Announcement];
    delete m_settings;

#ifdef Q_OS_ANDROID
    // Release the application-level wake lock that protected all worker threads
    qCInfo(application) << "Android: Releasing application wake lock";
    QJniObject::callStaticMethod<void>("org/qtproject/abracadabra/AudioServiceHelper", "releaseAppWakeLock", "()V");

    if (m_exitCode != Application::EXIT_CODE_RESTART)
    {  // this is drity hack to avoid application crashing on mutex when even loop is exited
        ::_exit(0);
    }
#endif
}

QQmlApplicationEngine *Application::qmlEngine() const
{
    return m_qmlEngine;
}

void Application::setContextProperties()
{
    QQmlContext *context = m_qmlEngine->rootContext();
    context->setContextProperty("application", this);
    context->setContextProperty("appUI", m_ui);
    context->setContextProperty("navigationModel", m_navigationModel);
    context->setContextProperty("slModel", m_slModel);
    context->setContextProperty("slTreeModel", m_slTreeModel);
    context->setContextProperty("channelListModel", m_channelListModel);
    context->setContextProperty("slSelectionModel", m_slSelectionModel);
    context->setContextProperty("slTreeSelectionModel", m_slTreeSelectionModel);
    context->setContextProperty("dlPlusModelService", m_dlPlusModel[Instance::Service]);
    context->setContextProperty("dlPlusModelAnnouncement", m_dlPlusModel[Instance::Announcement]);
    context->setContextProperty("slsService", m_slsBackend[Instance::Service]);
    context->setContextProperty("slsAnnouncement", m_slsBackend[Instance::Announcement]);
    context->setContextProperty("ensembleInfo", m_ensembleInfoBackend);
    context->setContextProperty("log", m_logBackend);
    context->setContextProperty("audioRecording", m_audioRecManager);
}

void Application::onInputDeviceReady()
{
    m_ui->tuneEnabled(true);
    m_ui->infoLabelIndex(0);
}

void Application::onEnsembleInfo(const RadioControlEnsemble &ens)
{
    m_ui->ensembleLabel(ens.label);
    m_ui->ensembleLabelToolTip(QString(tr("<b>Ensemble:</b> %1<br>"
                                          "<b>Short label:</b> %2<br>"
                                          "<b>ECC:</b> 0x%3<br>"
                                          "<b>EID:</b> 0x%4<br>"
                                          "<b>Country:</b> %5"))
                                   .arg(ens.label, ens.labelShort, QString("%1").arg(ens.ecc(), 2, 16, QChar('0')).toUpper(),
                                        QString("%1").arg(ens.eid(), 4, 16, QChar('0')).toUpper(), DabTables::getCountryName(ens.ueid)));
    m_ueid = ens.ueid;

    m_ui->ensembleId(ServiceListId(ens.frequency, ens.ueid));

    QPixmap logo = m_metadataManager->data(ServiceListId(ens.frequency, ens.ueid), ServiceListId(), MetadataManager::SmallLogo).value<QPixmap>();
    if (!logo.isNull())
    {
        m_ui->isEnsembleLogoVisible(true);
    }
    else
    {
        m_ui->isEnsembleLogoVisible(false);
    }

    if (!m_isScannerRunning)
    {
        m_serviceList->beginEnsembleUpdate(ens);
    }
    onShowCountryFlagChanged();
}

void Application::onEnsembleReconfiguration(const RadioControlEnsemble &ens) const
{
    m_serviceList->beginEnsembleUpdate(ens);
}

void Application::onServiceListComplete(const RadioControlEnsemble &ens)
{
    if (m_isScannerRunning)
    {
        return;
    }

    m_serviceList->endEnsembleUpdate(ens);

    serviceListViewUpdateSelection();
    serviceTreeViewUpdateSelection();

    if (m_ensembleRemoved && m_frequency != 0 && m_SId.isValid())
    {  // ensemble reappeared -> restoring last service
        m_ensembleRemoved = false;
        emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);
    }
}

void Application::onEnsembleRemoved(const RadioControlEnsemble &ens)
{  // this happens when UEID in currently tuned channel changes
    m_ensembleRemoved = true;
    emit resetUserApps();

    m_dlDecoder[Instance::Service]->reset();
    m_dlDecoder[Instance::Announcement]->reset();

    clearServiceInformationLabels();
    m_ui->isServiceSelected(false);
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);

    m_serviceList->removeEnsemble(ens);
}

void Application::onSignalState(uint8_t sync, float snr)
{
    if (DabSyncLevel::FullSync > DabSyncLevel(sync))
    {  // hide time when no sync
        resetDabTime();
    }

    m_ui->snrLabel(QString("%1 dB").arg(snr, 0, 'f', 1));

    int syncSnrLevel = 0;
    if (sync > static_cast<int>(DabSyncLevel::NoSync))
    {
        syncSnrLevel = 3;
        if (snr < static_cast<float>(DabSnrThreshold::LowSNR))
        {
            syncSnrLevel = 1;
        }
        else if (snr < static_cast<float>(DabSnrThreshold::GoodSNR))
        {
            syncSnrLevel = 2;
        }
        if (sync == static_cast<int>(DabSyncLevel::FullSync))
        {
            syncSnrLevel += 3;
        }
    }
    m_ui->signalQualityLevel(syncSnrLevel);

    if (m_signalBackend != nullptr)
    {
        m_signalBackend->setSignalState(sync, snr);
    }
}

void Application::onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry)
{
    if ((slEntry.TMId != DabTMId::StreamAudio) || m_isScannerRunning)
    {  // do nothing - data services not supported or scanning tool is running
        return;
    }

    // add to service list
    m_serviceList->addService(ens, slEntry);
}

void Application::onDLComplete_Service(const QString &dl)
{
    m_ui->dlService(adjustDLString(dl));
}

void Application::onDLComplete_Announcement(const QString &dl)
{
    m_ui->dlAnnouncement(adjustDLString(dl));
}

QString Application::adjustDLString(const QString &dl)
{
    QString dlText = dl;
#if 0  // Example text for testing link detection
    dlText = R"(Here is plain text...
Trailing dots ... should not link.
Two dots .. should not link.
Mid-sentence ellipsis … also shouldn’t link.
Bare domain text.cz should link.
Subdomain aaa.xxx.eu should link.
Email x@y.z should link.
HTTP http://x.y.z should link.
HTTPS https://x.y.z should link.
HTTPS no path https://x.y should link.
No protocol www.example.org/path should link.
Mixed case Www.Example.CoM should link.
Hyphenated host my-site.example.co.uk should link.
Port and path example.com:8080/foo?bar=baz should link.
Query with dash https://api.example.com/v1/users?id=123 should link.
Tight punctuation (end): see example.com.
Tight punctuation (start): (example.com) should still link.
Underscore in user john_doe@example.com should link.
Username+tag tag+1@mail.example should link.
Dot before space example.com... then words (only domain should link, not the dots).)";
#endif

    // Regular expression to match URLs and email addresses but not plain ellipses like "..."
    static const QRegularExpression urlRegex(
        R"((^|(?<!\.)(?:[\s(\[{<]))((?:https?:\/\/)?(?:www\.)?(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z]{1,}(?:\/\S*)?|\b[a-zA-Z0-9._%+-]+@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z]{1,}))");

    // Replacing URLs with HTML links
    dlText.replace(urlRegex, "\\1<a href=\"\\2\">\\2</a>");

    dlText.replace(QRegularExpression(QString(QChar(0x0A))), "<br/>");
    if (dlText.indexOf(QChar(0x0B)) >= 0)
    {
        dlText.prepend("<b>");
        dlText.replace(dlText.indexOf(QChar(0x0B)), 1, "</b>");
    }
    dlText.remove(QChar(0x1F));
    return dlText;
}

void Application::onDabTime(const QDateTime &d)
{
#if 1
    m_dabTime = d;
    m_ui->timeLabel(m_timeLocale.toString(d, QString("dddd, dd.MM.yyyy, hh:mm")));
    EPGTime::getInstance()->onDabTime(d);
#else
    auto dd = QDateTime::currentDateTime();
    m_dabTime = dd;
    m_ui->timeLabel(m_timeLocale.toString(dd, QString("dddd, dd.MM.yyyy, hh:mm")));
    EPGTime::getInstance()->onDabTime(dd);
#endif
}

void Application::resetDabTime()
{
    m_dabTime = QDateTime();  // set invalid
    if (m_settings->showSystemTime && (m_inputDeviceId != InputDevice::Id::UNDEFINED) &&
        (m_inputDevice->capabilities() & InputDevice::Capability::LiveStream))
    {
        m_ui->timeLabel(
            QString("<font color='#808080'>%1</font>").arg(m_timeLocale.toString(QDateTime::currentDateTime(), QString("dddd, dd.MM.yyyy, hh:mm"))));
        m_ui->timeLabelToolTip(tr("System time"));

        if (m_sysTimeTimer != nullptr)
        {
            m_sysTimeTimer->start();
        }
    }
    else
    {
        m_ui->timeLabel("");
        m_ui->timeLabelToolTip(tr("DAB time"));
    }
}

void Application::onAudioParametersInfo(const struct AudioParameters &params)
{
    switch (params.coding)
    {
        case AudioCoding::MP2:
            m_ui->audioEncodingLabel("MP2");
            m_ui->audioEncodingLabelToolTip(QString(tr("<b>DAB audio encoding</b><br>%1")).arg(tr("MPEG-1 layer 2")));
            break;
        case AudioCoding::AACLC:
            m_ui->audioEncodingLabel("AAC-LC");
            m_ui->audioEncodingLabelToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 Low Complexity AAC")));
            break;
        case AudioCoding::HEAAC:
            m_ui->audioEncodingLabel("HE-AAC");
            m_ui->audioEncodingLabelToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 High Efficiency AAC")));
            break;
        case AudioCoding::HEAACv2:
            m_ui->audioEncodingLabel("HE-AACv2");
            m_ui->audioEncodingLabelToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 High Efficiency AAC v2")));
            break;
        case AudioCoding::None:
            m_ui->audioEncodingLabel("");
            m_ui->audioEncodingLabelToolTip("");
            m_ui->stereoLabel("");
            m_ui->stereoLabelToolTip("");
            m_ensembleInfoBackend->setAudioParameters(params);
            return;
    }

    if (params.stereo)
    {
        m_ui->stereoLabel("STEREO");
        if (AudioCoding::MP2 == params.coding)
        {
            m_ui->stereoLabelToolTip(
                QString(tr("<b>Audio signal</b><br>%1Stereo<br>Sample rate: %2 kHz")).arg(params.sbr ? "Joint " : "").arg(params.sampleRateKHz));
        }
        else
        {
            m_ui->stereoLabelToolTip(QString(tr("<b>Audio signal</b><br>Stereo (PS %1)<br>Sample rate: %2 kHz (SBR %3)"))
                                         .arg(params.parametricStereo ? tr("on") : tr("off"))
                                         .arg(params.sampleRateKHz)
                                         .arg(params.sbr ? tr("on") : tr("off")));
        }
    }
    else
    {
        m_ui->stereoLabel("MONO");
        if (AudioCoding::MP2 == params.coding)
        {
            m_ui->stereoLabelToolTip(QString(tr("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz")).arg(params.sampleRateKHz));
        }
        else
        {
            m_ui->stereoLabelToolTip(
                QString(tr("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz (SBR: %2)")).arg(params.sampleRateKHz).arg(params.sbr ? "on" : "off"));
        }
    }
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);

    m_audioRecManager->setHaveAudio(true);
    m_ensembleInfoBackend->setAudioParameters(params);
}

void Application::onProgrammeTypeChanged(const DabSId &sid, const DabPTy &pty)
{
    if (m_SId.value() == sid.value())
    {  // belongs to current service

        // ETSI EN 300 401 V2.1.1 [8.1.5]
        // At any one time, the PTy shall be either Static or Dynamic;
        // there shall be only one PTy per service.

        if (pty.d != 0xFF)
        {  // dynamic PTy available != static PTy
            m_ui->programTypeLabel(DabTables::getPtyName(pty.d));
            m_ui->programTypeLabelToolTip(QString(tr("<b>Programme Type</b><br>"
                                                     "%1 (dynamic)"))
                                              .arg(DabTables::getPtyName(pty.d)));
        }
        else
        {
            m_ui->programTypeLabel(DabTables::getPtyName(pty.s));
            m_ui->programTypeLabelToolTip(QString(tr("<b>Programme Type</b><br>"
                                                     "%1"))
                                              .arg(DabTables::getPtyName(pty.s)));
        }
    }
    else
    { /* ignoring - not current service */
    }
}

void Application::channelSelected()
{
    m_ueid = 0;  // no ensemble
    if (InputDevice::Id::RAWFILE != m_inputDeviceId)
    {
        m_ui->serviceSelectionEnabled(false);
    }

    clearEnsembleInformationLabels();
    m_ui->tuneEnabled(false);
    m_ui->frequencyLabel(tr("Tuning...  "));

    onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);
    if (m_tiiBackend != nullptr)
    {
        m_tiiBackend->onChannelSelection();
    }

    // hide switch to avoid conflict with tuning -> will be enabled when tune is finished
    m_serviceSourcesMenuModel->clear();
    serviceSelected();
}

void Application::serviceSelected()
{
    m_ensembleRemoved = false;  // clearing ensemble removed flag, new service was selected
    emit stopUserApps();
    m_audioRecManager->doAudioRecording(false);
    m_dlDecoder[Instance::Service]->reset();
    m_dlDecoder[Instance::Announcement]->reset();
    clearServiceInformationLabels();

    m_ui->serviceInstance(Instance::Service);
    m_ui->isServiceSelected(false);  // this will be set to tru when service selection is complete
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);
    m_serviceSourcesMenuModel->clear();
}

void Application::setChannelIndex(int index)
{
    if (m_ui->channelIndex() == index)
    {
        return;
    }
    std::function<void(bool)> setChannelIndexCallback = [this, index](bool canContinue)
    {
        if (canContinue == false)
        {
            int idx = m_channelListModel->findFrequency(m_frequency);
            // restore previous index
            m_ui->channelIndex(-1);  // this is to force change
            m_ui->channelIndex(idx);
            return;
        }

        uint32_t freq = m_channelListModel->data(m_channelListModel->index(index, 0), DABChannelListModel::Roles::FrequencyRole).toUInt();
        if (m_frequency != freq)
        {
            // no service is selected
            m_SId.set(0);

            // reset UI
            m_slSelectionModel->clearSelection();

            channelSelected();

            m_ui->tuneEnabled(index < 0);  // this index is set when service list is cleared by user -> we want tune enabled

            emit serviceRequest(freq, 0, 0);
        }

        // update up/down button tooltip text
        int nextIdx = (index + 1) % m_channelListModel->rowCount();
        // ui->channelUp->setTooltip(QString(tr("Tune to %1")).arg(ui->channelCombo->itemText(nextIdx)));
        m_ui->channelUpToolTip(
            QString(tr("Tune to %1")).arg(m_channelListModel->data(m_channelListModel->index(nextIdx, 0), Qt::DisplayRole).toString()));

        int prevIdx = index - 1;
        if (prevIdx < 0)
        {
            prevIdx = m_channelListModel->rowCount() - 1;
        }
        else
        {
            prevIdx = prevIdx % m_channelListModel->rowCount();
        }
        m_ui->channelDownToolTip(
            QString(tr("Tune to %1")).arg(m_channelListModel->data(m_channelListModel->index(prevIdx, 0), Qt::DisplayRole).toString()));
        m_ui->channelIndex(index);
    };

    // stopAudioRecordingMsg(tr("Audio recording is ongoing. It will be stopped and saved if you change DAB channel."));
    checkAudioRecording(AudioRecMsg_ChannelChange, setChannelIndexCallback);
}

void Application::onBandScanStart()
{
    stop();
    m_settingsBackend->isInputDeviceSelectionEnabled(false);
    if (!m_settings->keepServiceListOnScan && !m_isScannerRunning)
    {
        m_serviceList->clear(false);  // do not clear favorites
    }
}

void Application::onChannelUpClicked()
{
    int ch = (m_ui->channelIndex() + 1) % m_channelListModel->rowCount();
    setChannelIndex(ch);
}

void Application::onChannelDownClicked()
{
    int ch = m_ui->channelIndex() - 1;
    if (ch < 0)
    {
        ch = m_channelListModel->rowCount() - 1;
    }
    else
    {
        ch = ch % m_channelListModel->rowCount();
    }
    setChannelIndex(ch);
}

void Application::onTuneDone(uint32_t freq)
{  // this slot is called when tune is complete
    m_frequency = freq;
    if (freq != 0)
    {
        if (!m_isScannerRunning)
        {
            m_ui->tuneEnabled(true);
            m_ui->serviceSelectionEnabled(true);
        }

        m_ui->frequencyLabel(QString("%1 MHz").arg(freq / 1000.0, 3, 'f', 3, QChar('0')));
        m_isPlaying = true;

        // if current service has alternatives populate source switch menu immediately to avoid UI blocking when audio does not work
        populateServiceSourcesMenu();

        m_ui->infoLabelIndex(0);
        emit resetUserApps();  // new channel -> reset user apps
    }
    else
    {
        // this can only happen when device is changed, or when exit is requested
        if (m_exitRequested)
        {  // processing in IDLE, close window
            close();
            return;
        }

        m_ui->frequencyLabel("");
        m_isPlaying = false;
        resetDabTime();
        clearEnsembleInformationLabels();
        clearServiceInformationLabels();
        m_slSelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);
        if (m_deviceChangeRequested)
        {
            initInputDevice(m_inputDeviceRequest, m_inputDeviceIdRequest);
        }
    }
}

void Application::onInputDeviceError(const InputDevice::ErrorCode errCode)
{
    switch (errCode)
    {
        case InputDevice::ErrorCode::EndOfFile:
            // tune to 0
            m_ui->messageInfoTitle(tr("End of file"));
            m_ui->messageInfoDetails(tr("Select any service to restart"));
            m_ui->infoLabelIndex(1);
            if (!m_settings->rawfile.loopEna)
            {
                emit showMessage();
                setChannelIndex(-1);
                onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);
            }
            else
            {  // rewind - restore info after timeout
                QTimer::singleShot(2000, this, [this]() { m_ui->infoLabelIndex(0); });
            }
            break;
        case InputDevice::ErrorCode::DeviceDisconnected:
            m_ui->messageInfoTitle(tr("Input device error: Device disconnected"));
            m_ui->messageInfoDetails(tr("Try to reconnect the device or choose another device in Settings"));
            m_ui->infoLabelIndex(1);
            emit showMessage();
            onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);

            // force no device
            m_settingsBackend->resetInputDevice();
            changeInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            emit showPageRequest(NavigationModel::Settings);
            break;
        case InputDevice::ErrorCode::NoDataAvailable:
            m_ui->messageInfoTitle(tr("Input device error: No data"));
            m_ui->messageInfoDetails(tr("Try to reconnect the device or choose another device in Settings"));
            m_ui->infoLabelIndex(1);
            emit showMessage();
            onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);

            // force no device
            m_settingsBackend->resetInputDevice();
            changeInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            emit showPageRequest(NavigationModel::Settings);
            break;
        default:
            qCWarning(application) << "InputDevice error" << int(errCode);
    }
}

void Application::checkAudioRecording(AudioRecMsg msgId, std::function<void(bool)> callback)
{
    if ((m_audioRecManager->isAudioRecordingActive() || m_audioRecManager->isAudioScheduleActive()) && !m_settings->audioRec.autoStopEna)
    {
        QString infoText;
        switch (msgId)
        {
            case AudioRecMsg_ChannelChange:
                infoText = QString(tr("Audio recording is ongoing. It will be stopped and saved if you change DAB channel."));
                break;
            case AudioRecMsg_SeviceChange:
                infoText = QString(tr("Audio recording is ongoing. It will be stopped and saved if you switch current service."));
                break;
        }

        m_messageBoxBackend->showQuestion(
            tr("Stop audio recording?"), infoText,
            // Note: buttons order is fixed
            [this, callback](MessageBoxBackend::StandardButton result)
            {
                if (result == MessageBoxBackend::StandardButton::Cancel)
                {  // Keep recording
                    callback(false);
                    return;
                }
                if (result == MessageBoxBackend::StandardButton::Yes)
                {  // Stop recording and do not ask again
                    m_settingsBackend->audioRecAutoStop(true);
                }
                m_audioRecManager->doAudioRecording(false);
                callback(true);
            },
            {
                {MessageBoxBackend::StandardButton::Cancel, tr("Keep recording"), MessageBoxBackend::ButtonRole::Neutral},
                {MessageBoxBackend::StandardButton::Yes, tr("Stop recording and do not ask again"), MessageBoxBackend::ButtonRole::Neutral},
                {MessageBoxBackend::StandardButton::Ok, tr("Stop recording"), MessageBoxBackend::ButtonRole::Neutral},
            },
            MessageBoxBackend::StandardButton::Cancel,  // Default to "Keep recording" - safer option
            MessageBoxBackend::ButtonOrientation::Vertical,
            false  // use defined order
        );
    }
    else
    {
        callback(true);
    }
}

void Application::selectService(const ServiceListId &serviceId)
{
    // we need to find the item in model and select it
    for (int r = 0; r < m_slModel->rowCount(); ++r)
    {
        auto index = m_slModel->index(r, 0);
        if (m_slModel->id(index) == serviceId)
        {  // found
            m_slSelectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
            return;
        }
    }
}

void Application::uploadEnsembleCSV(const RadioControlEnsemble &ens, const QString &csv, bool isRequested)
{
#if HAVE_FMLIST_INTERFACE
    if ((m_settings->uploadEnsembleInfo || isRequested) && m_fmlistInterface && (m_inputDeviceId != InputDevice::Id::UNDEFINED) &&
        (m_inputDevice->capabilities() & InputDevice::Capability::LiveStream))
    {
        if (m_dabTime.isValid() && m_dabTime.secsTo(QDateTime::currentDateTime()) < 24 * 3600)
        {  // protection from uploading ensemble information from some recording that is older than 1 day
            qCInfo(application, "Uploading ensemble information to FMLIST (%lld bytes)", csv.length());
            QString ensLabel = ens.label;
            ensLabel.replace('/', '_');
            m_fmlistInterface->uploadEnsembleCSV(
                QString("%1_%2_%3")
                    .arg(DabTables::channelList.value(ens.frequency), ensLabel, EPGTime::getInstance()->dabTime().toString("yyyy-MM-dd_hhmmss")),
                csv);
        }
    }
    else
    {
        m_ensembleInfoBackend->enableEnsembleInfoUpload();
    }
#endif  // HAVE_FMLIST_INTERFACE
}

void Application::onServiceListSelection(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList selectedList = selected.indexes();
    if (0 == selectedList.count())
    {
        // no selection => return
        m_slSelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        return;
    }

    QModelIndex currentIndex = selectedList.at(0);
    const SLModel *model = reinterpret_cast<const SLModel *>(currentIndex.model());
    if (model->id(currentIndex) == ServiceListId(m_SId.value(), m_SCIdS))
    {
        return;
    }

    QPersistentModelIndex selectedIdx(currentIndex);
    QPersistentModelIndex deselectedIdx;
    if (deselected.size() > 0)
    {
        QModelIndexList deselectedList = deselected.indexes();
        deselectedIdx = deselectedList.first();
    }

    std::function<void(bool)> serviceListSelectionCallback = [model, selectedIdx, deselectedIdx, this](bool canContinue)
    {
        if (canContinue == false)
        {
            if (deselectedIdx.isValid())
            {
                m_slSelectionModel->setCurrentIndex(deselectedIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                return;
            }
        }

        ServiceListConstIterator it = m_serviceList->findService(model->id(selectedIdx));
        if (m_serviceList->serviceListEnd() != it)
        {
            m_SId = (*it)->SId();
            m_SCIdS = (*it)->SCIdS();
            uint32_t newFrequency = (*it)->getEnsemble()->frequency();

            if (newFrequency != m_frequency)
            {
                m_frequency = newFrequency;

                // shift execution in event loop
                // QTimer::singleShot(1, this, [this]() { setChannelIndex(m_channelListModel->findFrequency(m_frequency)); });
                setChannelIndex(m_channelListModel->findFrequency(m_frequency));

                // set UI to new channel tuning
                channelSelected();
            }
            else
            {  // if new service has alternatives populate source switch menu immediately to avoid UI blocking when audio does not work
                populateServiceSourcesMenu();
            }
            serviceSelected();
            emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

            // synchronize tree view with service selection
            serviceTreeViewUpdateSelection();

            m_slSelectionModel->setCurrentIndex(selectedIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        }
    };

    // this checks if audio recording is ongoing and if yes, it shows message to the user waiting for his feedback
    // it call callback after user interaction ot whne audio recoding is not ongoing
    checkAudioRecording(AudioRecMsg_SeviceChange, serviceListSelectionCallback);
}

void Application::onServiceListTreeSelection(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList selectedList = selected.indexes();
    if (0 == selectedList.count())
    {
        // no selection => return
        m_slTreeSelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        return;
    }

    QModelIndex currentIndex = selectedList.at(0);
    const SLTreeModel *model = reinterpret_cast<const SLTreeModel *>(currentIndex.model());

    if (currentIndex.parent().isValid())
    {  // service, not ensemble selected
        // if both service ID and ensemble ID are the same then return
        ServiceListId currentServiceId(m_SId.value(), m_SCIdS);
        ServiceListId currentEnsId(0);
        ServiceListConstIterator it = m_serviceList->findService(currentServiceId);
        if (m_serviceList->serviceListEnd() != it)
        {  // found
            currentEnsId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->id();
        }

        if ((model->id(currentIndex) == currentServiceId) && (model->id(currentIndex.parent()) == currentEnsId))
        {
            return;
        }

        QPersistentModelIndex selectedIdx(currentIndex);
        QPersistentModelIndex deselectedIdx;
        if (deselected.size() > 0)
        {
            QModelIndexList deselectedList = deselected.indexes();
            deselectedIdx = deselectedList.first();
        }
        std::function<void(bool)> serviceTreeSelectionCallback = [model, currentServiceId, selectedIdx, deselectedIdx, this](bool canContinue)
        {
            if (canContinue == false)
            {
                if (deselectedIdx.isValid())
                {
                    m_slTreeSelectionModel->setCurrentIndex(deselectedIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                    return;
                }
            }

            ServiceListConstIterator it = m_serviceList->findService(currentServiceId);
            it = m_serviceList->findService(model->id(selectedIdx));
            if (m_serviceList->serviceListEnd() != it)
            {
                m_SId = (*it)->SId();
                m_SCIdS = (*it)->SCIdS();

                uint32_t newFrequency = (*it)->switchEnsemble(model->id(selectedIdx.parent()))->frequency();
                if (newFrequency != m_frequency)
                {
                    m_frequency = newFrequency;

                    // change combo - shift execution in event loop
                    // QTimer::singleShot(1, this, [this]() { setChannelIndex(m_channelListModel->findFrequency(m_frequency)); });
                    setChannelIndex(m_channelListModel->findFrequency(m_frequency));

                    // set UI to new channel tuning
                    channelSelected();
                }
                else
                {  // if new service has alternatives populate source switch menu immediately to avoid UI blocking when audio does not work
                    populateServiceSourcesMenu();
                }
                serviceSelected();
                emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

                // we need to find the item in model and select it
                serviceListViewUpdateSelection();

                m_slTreeSelectionModel->setCurrentIndex(selectedIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
            }
        };

        // this checks if audio recording is ongoing and if yes, it shows message to the user waiting for his feedback
        // it call callback after user interaction ot when audio recoding is not ongoing
        checkAudioRecording(AudioRecMsg_SeviceChange, serviceTreeSelectionCallback);
    }
}

void Application::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    if (s.isAudioService())
    {
        if (s.SId.value() != m_SId.value())
        {  // this can happen when service is selected while still acquiring ensemble infomation
            m_SId = s.SId;
            m_SCIdS = s.SCIdS;
            selectService(ServiceListId(s.SId.value(), s.SCIdS));
            serviceTreeViewUpdateSelection();
        }

        if (s.label.isEmpty())
        {  // service component not valid -> shoudl not happen
            return;
        }
        // set service name in UI until information arrives from decoder

        m_ui->isServiceSelected(true);

        ServiceListId id(s);
        ServiceListConstIterator it = m_serviceList->findService(id);
        if (it != m_serviceList->serviceListEnd())
        {
            m_ui->isServiceFavorite(m_serviceList->isServiceFavorite(id));
            // populate source switch menu
            populateServiceSourcesMenu();
        }
        m_ui->serviceLabel(s.label);
        m_ui->serviceLabelToolTip(QString(tr("<b>Service:</b> %1<br>"
                                             "<b>Short label:</b> %2<br>"
                                             "<b>SId:</b> 0x%3<br>"
                                             "<b>SCIdS:</b> %4<br>"
                                             "<b>Language:</b> %5<br>"
                                             "<b>Country:</b> %6"))
                                      .arg(s.label, s.labelShort, QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper())
                                      .arg(s.SCIdS)
                                      .arg(DabTables::getLangName(s.lang), DabTables::getCountryName(s.SId.value())));

        onProgrammeTypeChanged(s.SId, s.pty);
        displaySubchParams(s);
        m_ui->infoLabelIndex(0);

        m_ui->serviceId(ServiceListId(s.SId.value(), s.SCIdS).value());
        QPixmap logo = m_metadataManager->data(m_ueid, s.SId.value(), s.SCIdS, MetadataManager::SmallLogo).value<QPixmap>();
        if (!logo.isNull())
        {
            m_ui->isServiceLogoVisible(true);
        }
        else
        {
            m_ui->isServiceLogoVisible(false);
        }
        logo = m_metadataManager->data(m_ueid, s.SId.value(), s.SCIdS, MetadataManager::SLSLogo).value<QPixmap>();
        if (!logo.isNull())
        {
            m_slsBackend[Instance::Service]->showServiceLogo(logo, true);
        }
        onShowCountryFlagChanged();
    }
    else
    {  // not audio service
        m_SId.set(0);

        m_slSelectionModel->clearSelection();
    }
}

void Application::displaySubchParams(const RadioControlServiceComponent &s)
{
    // update information about protection level and bitrate
    if (s.isAudioService())
    {
        QString label;
        QString toolTip;
        if (s.protection.isEEP())
        {  // EEP
            if (s.protection.level < DabProtectionLevel::EEP_1B)
            {  // EEP x-A
                label = QString("EEP %1-%2").arg(int(s.protection.level) - int(DabProtectionLevel::EEP_1A) + 1).arg("A");
            }
            else
            {  // EEP x+B
                label = QString("EEP %1-%2").arg(int(s.protection.level) - int(DabProtectionLevel::EEP_1B) + 1).arg("B");
            }
            toolTip = QString(tr("<B>Error protection</b><br>"
                                 "%1<br>Coderate: %2/%3<br>"
                                 "Capacity units: %4 CU"))
                          .arg(label)
                          .arg(s.protection.codeRateUpper)
                          .arg(s.protection.codeRateLower)
                          .arg(s.SubChSize);
        }
        else
        {  // UEP
            label = QString("UEP #%1").arg(s.protection.uepIndex);
            toolTip = QString(tr("<B>Error protection</b><br>"
                                 "%1<br>Protection level: %2<br>"
                                 "Capacity units: %3 CU"))
                          .arg(label)
                          .arg(int(s.protection.level))
                          .arg(s.SubChSize);
        }
        m_ui->protectionLabel(label);
        m_ui->protectionLabelToolTip(toolTip);

        QString br = QString(tr("%1 kbps")).arg(s.streamAudioData.bitRate);
        m_ui->audioBitrateLabel(br);
        m_ui->audioBitrateLabelToolTip(QString(tr("<b>Service bitrate</b><br>Audio & data: %1")).arg(br));
    }
    else
    { /* this should not happen */
    }
}

void Application::onAudioServiceReconfiguration(const RadioControlServiceComponent &s)
{
    if (s.SId.isValid() && s.isAudioService() && (s.SId.value() == m_SId.value()))
    {  // set UI
        onAudioServiceSelection(s);
        m_ensembleInfoBackend->setServiceInformation(s);
        emit getAudioInfo();
    }
    else
    {  // service probably disapeared
        // ETSI TS 103 176 V2.4.1 (2020-08) [6.4.3 Receiver behaviour]
        // If by the above methods a continuation of service cannot be established, the receiver shall stop the service.
        // It should display a 'service ceased' message as appropriate.

        QString serviceLabel = m_ui->serviceLabel();
        clearServiceInformationLabels();
        // ui->serviceListView->setCurrentIndex(QModelIndex());
        m_slSelectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        m_ui->serviceLabel(serviceLabel);
        m_ui->programTypeLabel(tr("Service currently unavailable"));
        m_ui->programTypeLabelToolTip(tr("Service was removed from ensemble"));

        emit stopUserApps();
        m_dlDecoder[Instance::Service]->reset();
        m_dlDecoder[Instance::Announcement]->reset();

        m_ui->isServiceSelected(false);
        m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);
    }
}

void Application::onAnnouncement(const DabAnnouncement id, const RadioControlAnnouncementState state, const RadioControlServiceComponent &s)
{
    switch (state)
    {
        case RadioControlAnnouncementState::None:
            m_ui->serviceInstance(Instance::Service);
            m_ui->dlAnnouncement("");  // clear for next announcment

            // reset DL+
            m_dlPlusModel[Instance::Announcement]->reset();

            // disabling DL+ if no objects
            // toggleDLPlus(!m_dlObjCache[Instance::Service].isEmpty());
            toggleDLPlus(m_dlPlusModel[Instance::Service]->rowCount() > 0);

            m_ui->isAnnouncementOngoing(false);

            // enable audio recording menu item
            m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
            break;
        case RadioControlAnnouncementState::OnCurrentService:
            m_ui->announcementButtonTooltip(QString(tr("<b>%1</b><br>"
                                                       "Ongoing announcement<br>"
                                                       "on current service"))
                                                .arg(DabTables::getAnnouncementName(id)));

            m_ui->isAnnouncementActive(true);
            m_ui->isAnnouncementButtonEnabled(false);
            m_ui->isAnnouncementOngoing(true);
            m_ui->serviceInstance(Instance::Service);

            // enable audio recording menu item
            m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
            break;
        case RadioControlAnnouncementState::OnOtherService:
            m_ui->announcementButtonTooltip(QString(tr("<b>%1</b><br>"
                                                       "Ongoing announcement<br>"
                                                       "Source service: <i>%2</i><br>"
                                                       "<br>"
                                                       "Click to suspend this announcement"))
                                                .arg(DabTables::getAnnouncementName(id), s.label));

            m_ui->isAnnouncementActive(true);
            m_ui->isAnnouncementButtonEnabled(true);
            m_ui->isAnnouncementOngoing(true);
            m_ui->serviceInstance(Instance::Announcement);

            // disable audio recording menu item
            m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);

            break;
        case RadioControlAnnouncementState::Suspended:
            m_ui->announcementButtonTooltip(QString(tr("<b>%1</b><br>"
                                                       "Suspended announcement<br>"
                                                       "Source service: <i>%2</i><br>"
                                                       "<br>"
                                                       "Click to resume this announcement"))
                                                .arg(DabTables::getAnnouncementName(id), s.label));

            m_ui->isAnnouncementActive(false);
            m_ui->isAnnouncementButtonEnabled(true);
            m_ui->isAnnouncementOngoing(true);

            m_ui->serviceInstance(Instance::Service);
            m_ui->dlAnnouncement("");  // clear for next announcment

            // reset DL+
            m_dlPlusModel[Instance::Announcement]->reset();

            // disabling DL+ if no objects
            // toggleDLPlus(!m_dlObjCache[Instance::Service].isEmpty());
            toggleDLPlus(m_dlPlusModel[Instance::Service]->rowCount() > 0);

            // enable audio recording menu item
            m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
            break;
    }

    displaySubchParams(s);

    if ((DabAnnouncement::Alarm == id) && (m_settings->bringWindowToForeground))
    {
        for (QWindow *appWindow : qApp->allWindows())
        {
            appWindow->show();             // bring window to top on OSX
            appWindow->raise();            // bring window from minimized state on OSX
            appWindow->requestActivate();  // bring window to front/unminimize on windows
        }
    }
}

void Application::onAudioDevicesList(QList<QAudioDevice> list)
{
    m_audioOutputMenuModel->clear();
    if (list.isEmpty() == false)
    {  // create menu if list is not empty
        int id = 0;
        m_audioOutputMenuModel->addMenuItem(tr("Default audio device"), id++, QVariant::fromValue(QAudioDevice()), true, true);
        for (const QAudioDevice &device : list)
        {
            m_audioOutputMenuModel->addMenuItem(device.description(), id++, QVariant::fromValue(device), true, true);
        }
    }
}

void Application::onAudioOutputError()
{  // tuning to 0
    // no service is selected
    m_SId.set(0);

    qCWarning(application, "Audio output error");
    // m_infoLabel->setText(tr("Audio Output Error"));
    // m_infoLabel->setToolTip(tr("Try to select other service to recover"));
    emit audioStop();
}

void Application::handleAudioOutputSelection(int actionId, const QVariant &data)
{
    // handle single selection
    for (int n = 0; n < m_audioOutputMenuModel->rowCount(); ++n)
    {
        m_audioOutputMenuModel->setChecked(n, n == actionId);
    }
    emit audioOutput(data.value<QAudioDevice>().id());
}

void Application::onAudioDeviceChanged(const QByteArray &id)
{
    if (id.isEmpty())
    {  // default audio device
        for (int n = 0; n < m_audioOutputMenuModel->rowCount(); ++n)
        {
            m_audioOutputMenuModel->setChecked(n, n == 0);
        }
        return;
    }
    bool itemFound = false;
    for (int n = 0; n < m_audioOutputMenuModel->rowCount(); ++n)
    {
        auto idx = m_audioOutputMenuModel->index(n, 0);
        auto data = m_audioOutputMenuModel->data(idx, ContextMenuModel::DataRole).value<QAudioDevice>().id();
        if (data == id)
        {
            itemFound = true;
            m_audioOutputMenuModel->setChecked(n, true);
        }
        else
        {
            m_audioOutputMenuModel->setChecked(n, false);
        }
    }
    if (!itemFound)
    {
        qCWarning(application)
            << "Default audio device selected"
            << m_audioOutputMenuModel->data(m_audioOutputMenuModel->index(0, 0), ContextMenuModel::DataRole).value<QAudioDevice>().id();
        for (int n = 0; n < m_audioOutputMenuModel->rowCount(); ++n)
        {
            m_audioOutputMenuModel->setChecked(n, n == 0);
        }
    }
}

void Application::audioRecordingToggle()
{
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);
    m_audioRecManager->doAudioRecording(!m_audioRecManager->isAudioRecordingActive());
}

void Application::onAudioRecordingStarted()
{
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
    emit announcementMask(0x0001);  // disable announcements during recording (only alarm is enabled)
    onAudioRecordingProgress(0, 0);
    m_navigationModel->setLabel(NavigationModel::AudioRecording, tr("Stop audio recording"));
}

void Application::onAudioRecordingStopped()
{
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
    m_navigationModel->setLabel(NavigationModel::AudioRecording, tr("Start audio recording"));
    emit announcementMask(m_settings->announcementEna);  // restore announcement settings
}

void Application::onAudioRecordingProgress(size_t bytes, qint64 timeSec)
{
    if (timeSec >= 0)
    {
        int min = timeSec / 60;
        m_ui->audioRecordingProgressLabel(QString("%1:%2").arg(min).arg(timeSec - min * 60, 2, 10, QChar('0')));
        m_ui->audioRecordingProgressLabelToolTip(QString(tr("Audio recording ongoing (%2 kBytes recorded)\n"
                                                            "File: %1"))
                                                     .arg(m_audioRecManager->audioRecordingFile())
                                                     .arg(bytes >> 10));
    }
    else
    {  // scheduled recording will start
        m_ui->audioRecordingProgressLabel("0:00");
        m_ui->audioRecordingProgressLabelToolTip(tr("Scheduled audio recording is getting ready"));
    }
}

void Application::onAudioRecordingCountdown(int numSec)
{
    m_navigationModel->setEnabled(NavigationModel::AudioRecording, false);
    if (numSec > 0)
    {
        QString message;
        if (m_audioRecManager->isAudioRecordingActive())
        {
            message = QString(tr("Scheduled recording should start in %1 seconds"));
            m_messageBoxBackend->showQuestion(
                message.arg(numSec),
                tr("Ongoing recording now prevents the start of a scheduled recording. "
                   "The schedule will be cancelled if you do not choose otherwise. "
                   "If you select to keep the schedule, the service might be switched."),
                // Note: buttons order is fixed
                [this](MessageBoxBackend::StandardButton result)
                {
                    if (result == MessageBoxBackend::StandardButton::Ok)
                    {  // cancel schedule
                        m_audioRecManager->requestCancelSchedule();
                    }
                    m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
                },
                {
                    {MessageBoxBackend::StandardButton::Ok, tr("Keep current recording"), MessageBoxBackend::ButtonRole::Neutral},
                    {MessageBoxBackend::StandardButton::Cancel, tr("Keep schedule"), MessageBoxBackend::ButtonRole::Neutral},
                },
                MessageBoxBackend::StandardButton::Ok,  // Default to "Keep current recording" - safer option
                MessageBoxBackend::ButtonOrientation::Vertical,
                false  // do not use platform order
            );
        }
        else
        {
            message = QString(tr("Scheduled recording starts in %1 seconds"));
            m_messageBoxBackend->showQuestion(
                message.arg(numSec),
                tr("Recording is going to start according to the schedule. The service might be switched if it differs from the current one."),
                // Note: buttons order is fixed
                [this](MessageBoxBackend::StandardButton result)
                {
                    if (result == MessageBoxBackend::StandardButton::Cancel)
                    {  // cancel plan
                        m_audioRecManager->requestCancelSchedule();
                    }
                    m_navigationModel->setEnabled(NavigationModel::AudioRecording, true);
                },
                {
                    {MessageBoxBackend::StandardButton::Ok, tr("Continue as planned"), MessageBoxBackend::ButtonRole::Neutral},
                    {MessageBoxBackend::StandardButton::Cancel, tr("Cancel plan"), MessageBoxBackend::ButtonRole::Neutral},
                },
                MessageBoxBackend::StandardButton::Ok,  // Default to "Continue as planned" - safer option
                MessageBoxBackend::ButtonOrientation::Vertical,
                false  // do not use platform order
            );
        }

        // countdown update
        QTimer *cntDown = new QTimer(this);
        cntDown->setProperty("cnt", numSec);
        connect(cntDown, &QTimer::timeout, this,
                [cntDown, message, this]()
                {
                    int cnt = cntDown->property("cnt").value<int>();
                    if (--cnt <= 0)
                    {
                        m_messageBoxBackend->handleButtonClicked(MessageBoxBackend::StandardButton::Ok);
                        cntDown->stop();
                        cntDown->deleteLater();
                    }
                    else
                    {
                        cntDown->setProperty("cnt", cnt);
                        m_messageBoxBackend->setMessage(message.arg(cnt));
                    }
                });
        cntDown->start(1000);
    }
}

void Application::onMetadataUpdated(const ServiceListId &id, MetadataManager::MetadataRole role)
{
    if (id.isService())
    {
        if ((id.sid() == m_SId.value()) && (id.scids() == m_SCIdS))
        {  // current service data
            switch (role)
            {
                case MetadataManager::MetadataRole::SLSLogo:
                {
                    QPixmap logo = m_metadataManager->data(ServiceListId(0, m_ueid), id, MetadataManager::SLSLogo).value<QPixmap>();
                    if (logo.isNull() == false)
                    {
                        m_slsBackend[Instance::Service]->showServiceLogo(logo);
                    }
                }
                break;
                case MetadataManager::SmallLogo:
                {
                    QPixmap logo = m_metadataManager->data(ServiceListId(0, m_ueid), id, MetadataManager::SmallLogo).value<QPixmap>();
                    if (!logo.isNull())
                    {
                        m_ui->isServiceLogoVisible(true);
                    }
                    else
                    {
                        m_ui->isServiceLogoVisible(false);
                    }
                }
                break;
                case MetadataManager::CountryFlag:
                {
                    if (m_settings->showServiceFlag)
                    {
                        QPixmap logo = m_metadataManager->data(ServiceListId(), id, MetadataManager::CountryFlag).value<QPixmap>();
                        if (!logo.isNull())
                        {
                            m_ui->isServiceFlagVisible(true);
                            m_ui->serviceFlagToolTip(QString("<b>Country:</b> %1").arg(DabTables::getCountryName(m_SId.value())));
                        }
                    }
                }
                break;
                default:
                    break;
            }
        }
    }
    else
    {  // ensemble
        if (id.ueid() == m_ueid)
        {
            switch (role)
            {
                case MetadataManager::SmallLogo:
                {
                    QPixmap logo = m_metadataManager->data(id, ServiceListId(), MetadataManager::SmallLogo).value<QPixmap>();
                    if (!logo.isNull())
                    {
                        m_ui->isEnsembleLogoVisible(true);
                    }
                    else
                    {
                        m_ui->isEnsembleLogoVisible(false);
                    }
                }
                break;

                case MetadataManager::CountryFlag:
                {
                    if (m_settings->showEnsFlag)
                    {
                        QPixmap flag = m_metadataManager->data(id, ServiceListId(), MetadataManager::CountryFlag).value<QPixmap>();
                        if (!flag.isNull())
                        {
                            m_ui->isEnsembleFlagVisible(true);
                            m_ui->ensembleFlagToolTip(QString("<b>Country:</b> %1").arg(DabTables::getCountryName(m_ueid)));
                        }
                    }
                }
                break;

                default:
                    // do nothing
                    break;
            }
        }
    }
}

void Application::onEpgEmpty()
{
    if (m_navigationModel)
    {
        m_navigationModel->setEnabled(NavigationModel::Epg, false);
    }
}

void Application::onSpiProgressSettingsChanged()
{
    if (m_settings->spiProgressEna)
    {  // enabled
        m_ui->isSpiProgressEnsVisible(m_settings->spiProgressEna && (m_ui->spiProgressEns() >= 0) &&
                                      ((m_ui->spiProgressEns() < 100) || !m_settings->spiProgressHideComplete));
        m_ui->isSpiProgressServiceVisible(m_settings->spiProgressEna && (m_ui->spiProgressService() >= 0) &&
                                          ((m_ui->spiProgressService() < 100) || !m_settings->spiProgressHideComplete));
    }
    else
    {  // disabled
        m_ui->isSpiProgressEnsVisible(false);
        m_ui->isSpiProgressServiceVisible(false);
    }
}

void Application::onSpiProgress(bool isEns, int decoded, int total)
{
    int progress = (total > 0) ? static_cast<int>(100.0 * decoded / total) : 0;

    QString tooltip;
    if (total > 0)
    {
        if (decoded < total)
        {
            tooltip = QString(tr("SPI MOT directory not complete\nDecoded %1 / %2 MOT objects")).arg(decoded).arg(total);
        }
        else
        {
            tooltip = QString(tr("SPI MOT directory complete\n%1 MOT objects decoded")).arg(total);
        }
    }
    else
    {  // this happens on decoding start
        tooltip = QString(tr("SPI MOT directory decoding started"));
    }

    if (isEns)
    {
        m_ui->spiProgressEns(progress);
        m_ui->spiProgressEnsToolTip(tooltip);
        m_ui->isSpiProgressEnsVisible(m_settings->spiProgressEna && (progress < 100 || !m_settings->spiProgressHideComplete));
    }
    else
    {
        m_ui->spiProgressService(progress);
        m_ui->spiProgressServiceToolTip(tooltip);
        m_ui->isSpiProgressServiceVisible(m_settings->spiProgressEna && (progress < 100 || !m_settings->spiProgressHideComplete));
    }
}

void Application::setProxy()
{
    QNetworkProxy proxy;
    switch (m_settings->proxy.config)
    {
        case Settings::ProxyConfig::NoProxy:
            qCInfo(application) << "Proxy config: No proxy";
            proxy.setType(QNetworkProxy::NoProxy);
            break;
        case Settings::ProxyConfig::System:
            qCInfo(application) << "Proxy config: System";
            QNetworkProxyFactory::setUseSystemConfiguration(true);
            return;
        case Settings::ProxyConfig::Manual:
            qCInfo(application) << "Proxy config: Manual";
            proxy.setType(QNetworkProxy::HttpProxy);
            proxy.setHostName(m_settings->proxy.server);
            proxy.setPort(m_settings->proxy.port);
            if (!m_settings->proxy.user.isEmpty())
            {
                proxy.setUser(m_settings->proxy.user);
                int key = 0;
                for (int n = 0; n < 4; ++n)
                {
                    key += m_settings->proxy.pass.at(n);
                }
                key = key & 0x00FF;
                if (key == 0)
                {
                    key = 0x5C;
                }
                QByteArray ba;
                for (int n = 4; n < m_settings->proxy.pass.length(); ++n)
                {
                    ba.append(m_settings->proxy.pass.at(n) ^ key);
                }
                QString pass = QString::fromUtf8(ba);
                proxy.setPassword(pass);
            }
            break;
    }
    QNetworkProxy::setApplicationProxy(proxy);
}

void Application::checkForUpdate()
{
    if (m_settings->updateCheckEna && m_settings->updateCheckTime.daysTo(QDateTime::currentDateTime()) >= 1)
    {
        UpdateChecker *updateChecker = new UpdateChecker(this);
        connect(
            updateChecker, &UpdateChecker::finished, this,
            [this, updateChecker](bool result)
            {
                if (result)
                {  // success
                    AppVersion ver(updateChecker->version());
                    if (ver.isValid())
                    {
                        m_settings->updateCheckTime = QDateTime::currentDateTime();
                        if (ver > AppVersion(PROJECT_VER))
                        {
                            qCInfo(application, "New application version found: %s", updateChecker->version().toUtf8().data());
                            m_ui->updateVersionInfo({PROJECT_VER, updateChecker->version(), "**Changelog:**\n\n" + updateChecker->releaseNotes()});
                            emit updateAvailable();
                        }
                    }
                }
                else
                {
                    qCWarning(application) << "Update check failed";
                }

                updateChecker->deleteLater();
            });
        updateChecker->check();
    }
}

void Application::pageUndocked(int id, bool isUndocked)
{
    switch (id)
    {
        case NavigationModel::DabSignal:
            if (m_signalBackend)
            {
                m_signalBackend->setIsUndocked(isUndocked);
            }
            break;
        case NavigationModel::Tii:
        case NavigationModel::Scanner:
        case NavigationModel::Undefined:
        case NavigationModel::Service:
        case NavigationModel::EnsembleInfo:
        case NavigationModel::Epg:
        case NavigationModel::ServiceList:
        case NavigationModel::Settings:
        case NavigationModel::ShowMore:
            break;
    }
}

void Application::pageActive(int id, bool isActive)
{
    switch (id)
    {
        case NavigationModel::Tii:
            if (m_tiiBackend)
            {
                m_tiiBackend->setIsActive(isActive);
            }
            break;
        case NavigationModel::Scanner:
            if (m_scannerBackend)
            {
                m_scannerBackend->setIsActive(isActive);
            }
            break;
        case NavigationModel::DabSignal:
            if (m_signalBackend)
            {
                m_signalBackend->setIsActive(isActive);
            }
            break;
        case NavigationModel::Undefined:
        case NavigationModel::Service:
        case NavigationModel::EnsembleInfo:

        case NavigationModel::Epg:
        case NavigationModel::ServiceList:
        case NavigationModel::Settings:
        case NavigationModel::ShowMore:
            break;
    }
}

void Application::populateServiceSourcesMenu()
{
    m_serviceSourcesMenuModel->clear();
    ServiceListConstIterator it = m_serviceList->findService(ServiceListId(m_SId.value(), m_SCIdS));
    if (it != m_serviceList->serviceListEnd())
    {
        int numEns = (*it)->numEnsembles();
        for (int n = 0; n < numEns; ++n)
        {
            bool isCurrent = ((*it)->getEnsemble(n)->frequency() == m_frequency);
            m_serviceSourcesMenuModel->addMenuItem(
                QString("%1 [%2]").arg((*it)->getEnsemble(n)->label(), DabTables::channelList.value((*it)->getEnsemble(n)->frequency())), n,
                QVariant(n), true, true, isCurrent);
        }
    }
}

void Application::handleServiceSourceSelection(int actionId, bool checked, const QVariant &data)
{
    int ensembleIdx = data.toInt();
    ServiceListConstIterator it = m_serviceList->findService(ServiceListId(m_SId.value(), m_SCIdS));
    if (it != m_serviceList->serviceListEnd())
    {
        auto ensemble = (*it)->getEnsemble(ensembleIdx);
        uint32_t newFrequency = ensemble->frequency();
        if (newFrequency == m_frequency)
        {
            // do nothing
            // populateServiceSourcesMenu();
            m_serviceSourcesMenuModel->setChecked(actionId, true);
            return;
        }

        m_frequency = newFrequency;
        (*it)->setCurrentEnsembleIdx(ensembleIdx);

        // change combo
        setChannelIndex(m_channelListModel->findFrequency(m_frequency));

        // set UI to new channel tuning
        channelSelected();
        serviceSelected();

        emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

        // synchronize tree view with service selection
        serviceListViewUpdateSelection();
        serviceTreeViewUpdateSelection();

        // update menu
        populateServiceSourcesMenu();
    }
}

void Application::clearEnsembleInformationLabels()
{
    m_ui->timeLabel("");
    m_ui->ensembleLabel(tr("No ensemble"));
    m_ui->ensembleLabelToolTip(tr("No ensemble tuned"));
    m_ui->frequencyLabel("");
    m_ui->isEnsembleLogoVisible(false);
    m_ui->isEnsembleFlagVisible(false);
    m_ui->isSpiProgressEnsVisible(false);
    m_ui->spiProgressEnsToolTip("");
    m_ui->spiProgressEns(-1);
}

void Application::clearServiceInformationLabels()
{
    m_ui->serviceLabel(tr("No service"));
    m_ui->isServiceFavorite(false);
    m_navigationModel->setEnabled(NavigationModel::CatSls, false);
    m_ui->isServiceLogoVisible(false);
    m_ui->isServiceFlagVisible(false);
    m_ui->isAnnouncementOngoing(false);
    m_ui->serviceLabelToolTip(tr("No service playing"));
    m_ui->programTypeLabel("");
    m_ui->programTypeLabelToolTip("");
    m_ui->audioEncodingLabel("");
    m_ui->audioEncodingLabelToolTip("");
    m_ui->stereoLabel("");
    m_ui->stereoLabelToolTip("");
    m_ui->protectionLabel("");
    m_ui->protectionLabelToolTip("");
    m_ui->audioBitrateLabel("");
    m_ui->audioBitrateLabelToolTip("");
    m_ui->isSpiProgressServiceVisible(false);
    m_ui->spiProgressServiceToolTip("");
    m_ui->spiProgressService(-1);
    m_slsBackend[Instance::Service]->reset();
    m_ui->serviceInstance(Instance::Service);
    onDLReset_Service();
    onDLReset_Announcement();
}

void Application::onNewAnnouncementSettings()
{
    if (!m_audioRecManager->isAudioRecordingActive())
    {
        emit announcementMask(m_settings->announcementEna);
    }
}

void Application::changeInputDevice(const InputDevice::Id &d, const QVariant &id)
{
    m_inputDeviceRequest = d;
    m_inputDeviceIdRequest = id;
    m_deviceChangeRequested = true;
    if (m_isPlaying)
    {  // stop
        stop();
        m_ui->tuneEnabled(false);  // enabled when device is ready
    }
    else
    {  // device is not playing
        initInputDevice(d, id);
    }
}

void Application::initInputDevice(const InputDevice::Id &d, const QVariant &id)
{
    m_deviceChangeRequested = false;
    if (nullptr != m_inputDevice)
    {
        delete m_inputDevice;
    }

    // disable band scan and scanner - will be enable when it makes sense
    m_navigationModel->setEnabled(NavigationModel::BandScan, false);
    m_navigationModel->setEnabled(NavigationModel::Scanner, false);

    // disable file recording
    m_ensembleInfoBackend->enableRecording(false);

    // restore all channels
    m_channelListModel->setChannelFilter(0);

    // metadata & EPG
    EPGTime::getInstance()->setIsLiveBroadcasting(false);

    switch (d)
    {
        case InputDevice::Id::UNDEFINED:
            // store service list if previous was not RAWFILE or UNDEFINED
            if ((InputDevice::Id::RAWFILE != m_inputDeviceId) && (InputDevice::Id::UNDEFINED != m_inputDeviceId))
            {  // if switching from live source save current service list & schedule
                m_serviceList->save(m_serviceListFilename);
                // save audio schedule
                m_audioRecScheduleModel->save(m_audioRecScheduleFilename);
            }
            else
            { /* do nothing if switching from RAW file */
            }

            m_inputDeviceId = InputDevice::Id::UNDEFINED;
            m_inputDevice = nullptr;
            m_ui->tuneEnabled(false);
            m_ui->serviceSelectionEnabled(false);
            m_ui->isServiceSelected(false);

            break;
        case InputDevice::Id::RTLSDR:
        {
            m_inputDevice = new RtlSdrInput();

            // signals have to be connected before calling openDevice

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            if (m_inputDevice->openDevice(id, m_settings->rtlsdr.fallbackConnection))
            {  // rtl sdr is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear schedule
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::RTLSDR;

                // setup dialog
                configureForInputDevice();
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
        }
        break;
        case InputDevice::Id::RTLTCP:
        {
            m_inputDevice = new RtlTcpInput();

            // signals have to be connected before calling openDevice
            // RTL_TCP is opened immediately and starts receiving data

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // set IP address and port
            dynamic_cast<RtlTcpInput *>(m_inputDevice)
                ->setTcpIp(m_settings->rtltcp.tcpAddress, m_settings->rtltcp.tcpPort, m_settings->rtltcp.controlSocketEna);

            if (m_inputDevice->openDevice())
            {  // rtl tcp is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear rec scheduile
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::RTLTCP;

                // setup dialog
                configureForInputDevice();
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
        }
        break;
        case InputDevice::Id::RARTTCP:
        {
#if HAVE_RARTTCP
            m_inputDevice = new RartTcpInput();

            // signals have to be connected before calling isAvailable
            // RTL_TCP is opened immediately and starts receiving data

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // set IP address and port
            dynamic_cast<RartTcpInput *>(m_inputDevice)->setTcpIp(m_settings->rarttcp.tcpAddress, m_settings->rarttcp.tcpPort);

            if (m_inputDevice->openDevice())
            {  // RaRT tcp is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear rec scheduile
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::RARTTCP;

                configureForInputDevice();
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
#endif
        }
        break;
        case InputDevice::Id::AIRSPY:
        {
#if HAVE_AIRSPY
            m_inputDevice = new AirspyInput(m_settings->airspy.prefer4096kHz);

            // signals have to be connected before calling isAvailable

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            if (m_inputDevice->openDevice(id, m_settings->airspy.fallbackConnection))
            {  // airspy is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear rec schedule
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::AIRSPY;

                configureForInputDevice();

                // these are settings that are configures in ini file manually
                // they are only set when device is initialized
                dynamic_cast<AirspyInput *>(m_inputDevice)->setDataPacking(m_settings->airspy.dataPacking);
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
#endif
        }
        break;
        case InputDevice::Id::SOAPYSDR:
        {
#if HAVE_SOAPYSDR
            m_inputDevice = new SoapySdrInput();

            // signals have to be connected before calling isAvailable

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            // set connection paramaters
            dynamic_cast<SoapySdrInput *>(m_inputDevice)->setDevArgs(m_settings->soapysdr.devArgs);
            dynamic_cast<SoapySdrInput *>(m_inputDevice)->setRxChannel(m_settings->soapysdr.channel);
            dynamic_cast<SoapySdrInput *>(m_inputDevice)->setAntenna(m_settings->soapysdr.antenna);

            if (m_inputDevice->openDevice())
            {  // SoapySDR is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear rec schedule
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::SOAPYSDR;

                configureForInputDevice();
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
#endif
        }
        break;
        case InputDevice::Id::SDRPLAY:
        {
#if HAVE_SOAPYSDR
            m_inputDevice = new SdrPlayInput();

            // signals have to be connected before calling isAvailable

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            // set connection paramaters
            dynamic_cast<SdrPlayInput *>(m_inputDevice)->setRxChannel(m_settings->sdrplay.channel);
            dynamic_cast<SdrPlayInput *>(m_inputDevice)->setAntenna(m_settings->sdrplay.antenna);

            if (m_inputDevice->openDevice(id, m_settings->sdrplay.fallbackConnection))
            {  // SoapySDR is available
                if ((InputDevice::Id::RAWFILE == m_inputDeviceId) || (InputDevice::Id::UNDEFINED == m_inputDeviceId))
                {  // if switching from RAW or UNDEFINED load service list & rec schedule

                    // clear service list
                    m_serviceList->clear();

                    // clear rec schedule
                    m_audioRecScheduleModel->clear();

                    m_serviceList->load(m_serviceListFilename);
                    m_audioRecScheduleModel->load(m_audioRecScheduleFilename);
                }
                else
                { /* keep service list as it is */
                }

                m_inputDeviceId = InputDevice::Id::SDRPLAY;

                configureForInputDevice();
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
#endif
        }
        break;
        case InputDevice::Id::RAWFILE:
        {
            m_inputDevice = new RawFileInput();

            // tuning procedure
            connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

            // HMI
            connect(m_inputDevice, &InputDevice::deviceReady, this, &Application::onInputDeviceReady, Qt::QueuedConnection);
            connect(m_inputDevice, &InputDevice::error, this, &Application::onInputDeviceError, Qt::QueuedConnection);

            RawFileInputFormat format = m_settings->rawfile.format;
            dynamic_cast<RawFileInput *>(m_inputDevice)->setFile(m_settings->rawfile.file, format);

            connect(dynamic_cast<RawFileInput *>(m_inputDevice), &RawFileInput::fileLength, m_settingsBackend, &SettingsBackend::setRawFileLength);
            connect(dynamic_cast<RawFileInput *>(m_inputDevice), &RawFileInput::fileProgress, m_settingsBackend,
                    &SettingsBackend::setRawFileProgressValue);
            connect(m_settingsBackend, &SettingsBackend::rawFileSeek, dynamic_cast<RawFileInput *>(m_inputDevice), &RawFileInput::seek);

            // we can open device now
            if (m_inputDevice->openDevice())
            {  // raw file is available
                if ((InputDevice::Id::RAWFILE != m_inputDeviceId) && (InputDevice::Id::UNDEFINED != m_inputDeviceId))
                {  // if switching from live source save current service list & rec schedule
                    m_serviceList->save(m_serviceListFilename);
                    m_audioRecScheduleModel->save(m_audioRecScheduleFilename);
                }
                else
                { /* do nothing if switching from RAW file */
                }

                // clear service list
                m_serviceList->clear();

                // clear rec schedule
                m_audioRecScheduleModel->clear();

                m_inputDeviceId = InputDevice::Id::RAWFILE;

                configureForInputDevice();

                if (m_inputDevice->deviceDescription().rawFile.frequency_kHz != 0)
                {
                    m_channelListModel->setChannelFilter(m_inputDevice->deviceDescription().rawFile.frequency_kHz);
                }
            }
            else
            {
                m_settingsBackend->resetInputDevice();
                initInputDevice(InputDevice::Id::UNDEFINED, QVariant());
            }
        }
        break;
    }
}

void Application::configureForInputDevice()
{
    if (m_inputDeviceId != InputDevice::Id::UNDEFINED)
    {
        // setup dialog
        m_settingsBackend->setInputDevice(m_inputDeviceId, m_inputDevice);

        bool isLive = m_inputDevice->capabilities() & InputDevice::Capability::LiveStream;
        bool hasRecording = m_inputDevice->capabilities() & InputDevice::Capability::Recording;

        // enable band scan
        m_navigationModel->setEnabled(NavigationModel::BandScan, isLive);
        m_navigationModel->setEnabled(NavigationModel::Scanner, isLive);

        // enable service list
        m_ui->serviceSelectionEnabled(true);
        m_ui->isServiceSelected(false);

        if (hasRecording)
        {
            // recorder
            m_inputDeviceRecorder->setDeviceDescription(m_inputDevice->deviceDescription());
            connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_inputDevice, &InputDevice::startStopRecording);
            connect(m_inputDevice, &InputDevice::recordBuffer, m_inputDeviceRecorder, &InputDeviceRecorder::writeBuffer, Qt::DirectConnection);
        }

        // ensemble info dialog
        connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoBackend, &EnsembleInfoBackend::updateAgcGain);
        connect(m_inputDevice, &InputDevice::rfLevel, m_ensembleInfoBackend, &EnsembleInfoBackend::updateRfLevel);
        if (m_signalBackend != nullptr)
        {
            m_signalBackend->setInputDevice(m_inputDeviceId);
            connect(m_inputDevice, &InputDevice::rfLevel, m_signalBackend, &SignalBackend::updateRfLevel);
        }
        m_ensembleInfoBackend->enableRecording(hasRecording);

        if (isLive && m_scannerBackend != nullptr)
        {
            // Scanner
            connect(m_inputDevice, &InputDevice::error, m_scannerBackend, &ScannerBackend::onInputDeviceError, Qt::QueuedConnection);
        }

        // metadata & EPG
        EPGTime::getInstance()->setIsLiveBroadcasting(isLive);
    }
}

void Application::getAudioSettings(Settings::AudioFramework &framework, Settings::AudioDecoder &decoder)
{
    QSettings *settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    int val;
#if (HAVE_PORTAUDIO)
    val = settings->value("audioFramework", Settings::AudioFramework::Pa).toInt();
    framework = static_cast<Settings::AudioFramework>(val);
#else
    framework = Settings::AudioFramework::Qt;
#endif

    val = settings->value("audioDecoderAAC", Settings::AudioDecoder::FAAD).toInt();
    decoder = static_cast<Settings::AudioDecoder>(val);

    delete settings;
}

void Application::restoreWindows()
{
    if (m_settings->restoreWindows)
    {
        if (m_settings->ensembleInfo.restore)
        {
            emit undockPageRequest(NavigationModel::EnsembleInfo);
        }
        if (m_settings->tii.restore)
        {
            emit undockPageRequest(NavigationModel::Tii);
        }
        if (m_settings->scanner.restore)
        {
            emit undockPageRequest(NavigationModel::Scanner);
        }
        if (m_settings->signal.restore)
        {
            emit undockPageRequest(NavigationModel::DabSignal);
        }
        if (m_settings->epg.restore)
        {
            emit undockPageRequest(NavigationModel::Epg);
        }
        if (m_settings->catSls.restore)
        {
            emit undockPageRequest(NavigationModel::CatSls);
        }
        if (m_settings->setupDialog.restore)
        {
            emit undockPageRequest(NavigationModel::Settings);
        }
        if (m_settings->log.restore)
        {
            emit undockPageRequest(NavigationModel::AppLog);
        }
    }
}

void Application::loadSettings()
{
    QSettings *settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    QFileInfo fi(settings->fileName());

    if (m_serviceListFilename.isEmpty())
    {  // create service list settings filename
        m_serviceListFilename = fi.path() + "/ServiceList.json";
    }
    m_audioRecScheduleFilename = fi.path() + "/AudioRecordingSchedule.json";
    m_settings->filePath = fi.path();

    if (AppVersion(settings->value("version", PROJECT_VER).toString()) < AppVersion("v2.9.2-75"))
    {  // old settings file detected

        // load servicelist
        m_serviceList->loadFromSettings(settings);
        // write to new file
        m_serviceList->save(m_serviceListFilename);

        // load recording schedule (using current instance)
        m_audioRecScheduleModel->loadFromSettings(settings);
        // write to new file
        m_audioRecScheduleModel->save(m_audioRecScheduleFilename);
    }
    else
    {  // new settings with dedicated JSON for service list and audio recording schedule
       // it will be loaded when device is initialized (this reduces duplication of service list information in log)
    }

    m_ui->audioVolume(settings->value("volume", 100).toInt());
    bool mute = settings->value("mute", false).toBool();
    onMuteButtonToggled(mute);
    emit audioOutput(settings->value("audioDevice", "").toByteArray());
    m_settings->keepServiceListOnScan = settings->value("keepServiceListOnScan", false).toBool();

    int inDevice = settings->value("inputDeviceId", int(InputDevice::Id::RTLSDR)).toInt();

    m_settings->appWindow.x = settings->value("AppWindow/x", -1).toInt();
    m_settings->appWindow.y = settings->value("AppWindow/y", -1).toInt();
    m_settings->appWindow.width = settings->value("AppWindow/width", -1).toInt();
    m_settings->appWindow.height = settings->value("AppWindow/height", -1).toInt();
    m_settings->appWindow.fullscreen = settings->value("AppWindow/fullscreen", false).toBool();

    m_settings->applicationStyle =
        static_cast<Settings::ApplicationStyle>(settings->value("style", static_cast<int>(Settings::ApplicationStyle::Default)).toInt());
    m_settings->dlPlusEna = settings->value("dlPlus", true).toBool();
    m_settings->lang = QLocale::codeToLanguage(settings->value("language", QString("")).toString());
    m_settings->inputDevice = static_cast<InputDevice::Id>(inDevice);
    m_settings->announcementEna = settings->value("announcementEna", 0x07FF).toUInt();
    m_settings->bringWindowToForeground = settings->value("bringWindowToForegroundOnAlarm", true).toBool();
    m_settings->noiseConcealmentLevel = settings->value("noiseConcealment", 0).toInt();
    m_settings->xmlHeaderEna = settings->value("rawFileXmlHeader", true).toBool();
    m_settings->spiAppEna = settings->value("spiAppEna", true).toBool();
    m_settings->spiProgressEna = settings->value("spiProgressEna", true).toBool();
    m_settings->spiProgressHideComplete = settings->value("spiProgressHideComplete", true).toBool();
    m_settings->useInternet = settings->value("useInternet", true).toBool();
    m_settings->radioDnsEna = settings->value("radioDNS", true).toBool();
    m_settings->slsBackground = QColor::fromString(settings->value("slsBg", QString("#000000")).toString());
    m_settings->updateCheckEna = settings->value("updateCheckEna", true).toBool();
    m_settings->updateCheckTime = settings->value("updateCheckTime", QDateTime::currentDateTime().addDays(-1)).value<QDateTime>();
    m_settings->uploadEnsembleInfo = settings->value("uploadEnsembleInfoEna", true).toBool();
#ifdef Q_OS_ANDROID
    m_settings->dataStoragePath = settings->value("dataStoragePath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
#else
    m_settings->dataStoragePath =
        settings->value("dataStoragePath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + appName).toString();
#endif
#if (HAVE_PORTAUDIO)
    if (nullptr != dynamic_cast<AudioOutputPa *>(m_audioOutput))
    {
        m_settings->audioFramework = Settings::AudioFramework::Pa;
    }
    else
#endif
    {
        m_settings->audioFramework = Settings::AudioFramework::Qt;
    }
    m_settings->audioDecoder = Settings::AudioDecoder::FAAD;
#if (!HAVE_FAAD)
    m_settings->audioDecoder = Settings::AudioDecoder::FDKAAC;
#elif (!HAVE_FDKAAC)
    m_settings->audioDecoder = Settings::AudioDecoder::FAAD;
#else
    m_settings->audioDecoder = static_cast<Settings::AudioDecoder>(settings->value("audioDecoderAAC", Settings::AudioDecoder::FAAD).toInt());
#endif

    m_settings->audioRec.captureOutput = settings->value("AudioRecording/captureOutput", false).toBool();
    m_settings->audioRec.autoStopEna = settings->value("AudioRecording/autoStop", false).toBool();
    m_settings->audioRec.dl = settings->value("AudioRecording/DL", false).toBool();
    m_settings->audioRec.dlAbsTime = settings->value("AudioRecording/DLAbsTime", false).toBool();

#ifdef Q_OS_MAC
    m_settings->trayIconEna = settings->value("showTrayIcon", false).toBool();
#else
    m_settings->trayIconEna = settings->value("showTrayIcon", true).toBool();
#endif
    m_settings->compactUi = settings->value("compactUi", false).toBool();
    m_settings->cableChannelsEna = settings->value("cableChannelsEna", false).toBool();
    m_settings->restoreWindows = settings->value("restoreWindows", false).toBool();
    m_settings->showSystemTime = settings->value("showSystemTime", false).toBool();
    m_settings->showEnsFlag = settings->value("showEnsembleCountryFlag", false).toBool();
    m_settings->showServiceFlag = settings->value("showServiceCountryFlag", false).toBool();

    m_settings->uaDump.dataStoragePath = m_settings->dataStoragePath;
    m_settings->uaDump.overwriteEna = settings->value("UA-STORAGE/overwriteEna", false).toBool();
    m_settings->uaDump.slsEna = settings->value("UA-STORAGE/slsEna", false).toBool();
    m_settings->uaDump.spiEna = settings->value("UA-STORAGE/spiEna", false).toBool();
    m_settings->uaDump.slsPattern = settings->value("UA-STORAGE/slsPattern", slsDumpPatern).toString();
    m_settings->uaDump.spiPattern = settings->value("UA-STORAGE/spiPattern", spiDumpPatern).toString();

    m_settings->tii.locationSource = static_cast<Settings::GeolocationSource>(
        settings->value("TII/locationSource", static_cast<int>(Settings::GeolocationSource::System)).toInt());
    m_settings->tii.coordinates = QGeoCoordinate(settings->value("TII/latitude", 0.0).toDouble(), settings->value("TII/longitude", 0.0).toDouble());
    m_settings->tii.serialPort = settings->value("TII/serialPort", "").toString();
    m_settings->tii.serialPortBaudrate = settings->value("TII/serialPortBaudrate", 4800).toInt();
    m_settings->tii.showSpectumPlot = settings->value("TII/showSpectrumPlot", false).toBool();
    m_settings->tii.restore = settings->value("TII/restore", false).toBool();
    m_settings->tii.splitterState = settings->value("TII/layout");
    m_settings->tii.mode = settings->value("TII/configuration", 0).toInt();
    m_settings->tii.showInactiveTx = settings->value("TII/showInactiveTx", false).toBool();
    m_settings->tii.inactiveTxTimeoutEna = settings->value("TII/inactiveTxTimeoutEna", false).toBool();
    m_settings->tii.inactiveTxTimeout = settings->value("TII/inactiveTxTimeout", 5).toInt();
    m_settings->tii.timestampInUTC = settings->value("TII/timestampInUTC", false).toBool();
    m_settings->tii.saveCoordinates = settings->value("TII/saveCoordinates", false).toBool();
    m_settings->tii.centerMapToCurrentPosition = settings->value("TII/mapCenterCurrPos", true).toBool();
    m_settings->tii.mapCenter =
        QGeoCoordinate(settings->value("TII/mapCenterLat", 50.08804).toDouble(), settings->value("TII/mapCenterLon", 14.42076).toDouble());
    m_settings->tii.mapZoom = settings->value("TII/mapZoom", 9.0).toFloat();

    // TxTable settings
    m_settings->tii.txTable.code.enabled = true;  // code column is always enabled
    m_settings->tii.txTable.code.order = settings->value("TII/txTable/codeIdx", 0).toInt();
    m_settings->tii.txTable.level.enabled = true;  // level column is always enabled
    m_settings->tii.txTable.level.order = settings->value("TII/txTable/levelIdx", 1).toInt();
    m_settings->tii.txTable.dist.enabled = settings->value("TII/txTable/distEna", true).toBool();
    m_settings->tii.txTable.dist.order = settings->value("TII/txTable/distIdx", 2).toInt();
    m_settings->tii.txTable.azimuth.enabled = settings->value("TII/txTable/azimuthEna", true).toBool();
    m_settings->tii.txTable.azimuth.order = settings->value("TII/txTable/azimuthIdx", 3).toInt();
    m_settings->tii.txTable.power.enabled = settings->value("TII/txTable/powerEna", false).toBool();
    m_settings->tii.txTable.power.order = settings->value("TII/txTable/powerIdx", 4).toInt();
    m_settings->tii.txTable.location.enabled = settings->value("TII/txTable/locationEna", false).toBool();
    m_settings->tii.txTable.location.order = settings->value("TII/txTable/locationIdx", 5).toInt();

    m_settings->scanner.splitterState = settings->value("Scanner/layout");
    m_settings->scanner.restore = settings->value("Scanner/restore", false).toBool();
    m_settings->scanner.mode = settings->value("Scanner/mode", 0).toInt();
    m_settings->scanner.numCycles = settings->value("Scanner/numCycles", 1).toInt();
    m_settings->scanner.clearOnStart = settings->value("Scanner/clearOnStart", true).toBool();
    m_settings->scanner.hideLocalTx = settings->value("Scanner/hideLocalTx", false).toBool();
    m_settings->scanner.autoSave = settings->value("Scanner/autoSave", false).toBool();
    m_settings->scanner.waitForSync = settings->value("Scanner/waitForSyncSec", 3).toInt();
    m_settings->scanner.waitForEnsemble = settings->value("Scanner/waitForEnsembleSec", 6).toInt();
    m_settings->scanner.centerMapToCurrentPosition = settings->value("Scanner/mapCenterCurrPos", true).toBool();
    m_settings->scanner.mapCenter =
        QGeoCoordinate(settings->value("Scanner/mapCenterLat", 50.08804).toDouble(), settings->value("Scanner/mapCenterLon", 14.42076).toDouble());
    m_settings->scanner.mapZoom = settings->value("Scanner/mapZoom", 9.0).toFloat();
    int numCh = settings->beginReadArray("Scanner/channels");
    for (int ch = 0; ch < numCh; ++ch)
    {
        settings->setArrayIndex(ch);
        m_settings->scanner.channelSelection.insert(settings->value("frequency").toInt(), settings->value("active").toBool());
    }
    settings->endArray();

#ifdef Q_OS_ANDROID
    m_settings->proxy.config =
        static_cast<Settings::ProxyConfig>(settings->value("Proxy/config", static_cast<int>(Settings::ProxyConfig::NoProxy)).toInt());
#else
    m_settings->proxy.config =
        static_cast<Settings::ProxyConfig>(settings->value("Proxy/config", static_cast<int>(Settings::ProxyConfig::System)).toInt());
    m_settings->proxy.server = settings->value("Proxy/server", "").toString();
    m_settings->proxy.port = settings->value("Proxy/port", "").toUInt();
    m_settings->proxy.user = settings->value("Proxy/user", "").toString();
    m_settings->proxy.pass = QByteArray::fromBase64(settings->value("Proxy/pass", "").toByteArray());
#endif

    m_settings->signal.splitterState = settings->value("SignalDialog/layout");
    m_settings->signal.restore = settings->value("SignalDialog/restore", false).toBool();
    m_settings->signal.spectrumMode = settings->value("SignalDialog/spectrumMode", 1).toInt();
    m_settings->signal.spectrumUpdate = settings->value("SignalDialog/spectrumUpdate", 1).toInt();
    m_settings->signal.showSNR = settings->value("SignalDialog/showSNR", 0).toBool();

    m_settings->ensembleInfo.restore = settings->value("EnsembleInfo/restore", false).toBool();
    m_settings->ensembleInfo.recordingTimeoutEna = settings->value("EnsembleInfo/recordingTimeoutEna", false).toBool();
    m_settings->ensembleInfo.recordingTimeoutSec = settings->value("EnsembleInfo/recordingTimeoutSec", 10).toInt();
    m_ensembleInfoBackend->loadSettings(m_settings);

    m_settings->log.restore = settings->value("Log/restore", false).toBool();

    m_settings->setupDialog.restore = settings->value("SetupDialog/restore", false).toBool();

    m_settings->catSls.restore = settings->value("CatSLS/restore", false).toBool();

    m_settings->epg.filterEmptyEpg = settings->value("EPG/filterEmpty", false).toBool();
    m_settings->epg.filterEnsemble = settings->value("EPG/filterOtherEnsembles", false).toBool();
    m_settings->epg.splitterState = settings->value("EPG/layout");
    m_settings->epg.restore = settings->value("EPG/restore", false).toBool();

    m_settings->rtlsdr.hwId = settings->value("RTL-SDR/lastDevice");
    m_settings->rtlsdr.fallbackConnection = settings->value("RTL-SDR/fallbackConnection", true).toBool();
    m_settings->rtlsdr.gainIdx = settings->value("RTL-SDR/gainIndex", 0).toInt();
    m_settings->rtlsdr.gainMode = static_cast<RtlGainMode>(settings->value("RTL-SDR/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    m_settings->rtlsdr.bandwidth = settings->value("RTL-SDR/bandwidth", 0).toUInt();
    m_settings->rtlsdr.biasT = settings->value("RTL-SDR/bias-T", false).toBool();
    m_settings->rtlsdr.agcLevelMax = settings->value("RTL-SDR/agcLevelMax", 0).toInt();
    m_settings->rtlsdr.ppm = settings->value("RTL-SDR/ppm", 0).toInt();
    m_settings->rtlsdr.rfLevelOffset = settings->value("RTL-SDR/rfLevelOffset", 0.0).toFloat();

    m_settings->rtltcp.gainIdx = settings->value("RTL-TCP/gainIndex", 0).toInt();
    m_settings->rtltcp.gainMode = static_cast<RtlGainMode>(settings->value("RTL-TCP/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    m_settings->rtltcp.tcpAddress = settings->value("RTL-TCP/address", QString("127.0.0.1")).toString();
    m_settings->rtltcp.tcpPort = settings->value("RTL-TCP/port", 1234).toInt();
    m_settings->rtltcp.controlSocketEna = settings->value("RTL-TCP/controlSocket", true).toBool();
    m_settings->rtltcp.agcLevelMax = settings->value("RTL-TCP/agcLevelMax", 0).toInt();
    m_settings->rtltcp.ppm = settings->value("RTL-TCP/ppm", 0).toInt();
    m_settings->rtltcp.rfLevelOffset = settings->value("RTL-TCP/rfLevelOffset", 0.0).toFloat();

#if HAVE_RARTTCP
    m_settings->rarttcp.tcpAddress = settings->value("RART-TCP/address", QString("127.0.0.1")).toString();
    m_settings->rarttcp.tcpPort = settings->value("RART-TCP/port", 1235).toInt();
#endif

#if HAVE_AIRSPY
    m_settings->airspy.hwId = settings->value("AIRSPY/lastDevice");
    m_settings->airspy.fallbackConnection = settings->value("AIRSPY/fallbackConnection", true).toBool();
    m_settings->airspy.gain.sensitivityGainIdx = settings->value("AIRSPY/sensitivityGainIdx", 9).toInt();
    m_settings->airspy.gain.lnaGainIdx = settings->value("AIRSPY/lnaGainIdx", 0).toInt();
    m_settings->airspy.gain.mixerGainIdx = settings->value("AIRSPY/mixerGainIdx", 0).toInt();
    m_settings->airspy.gain.ifGainIdx = settings->value("AIRSPY/ifGainIdx", 5).toInt();
    m_settings->airspy.gain.lnaAgcEna = settings->value("AIRSPY/lnaAgcEna", true).toBool();
    m_settings->airspy.gain.mixerAgcEna = settings->value("AIRSPY/mixerAgcEna", true).toBool();
    m_settings->airspy.gain.mode = static_cast<AirpyGainMode>(settings->value("AIRSPY/gainMode", static_cast<int>(AirpyGainMode::Hybrid)).toInt());
    m_settings->airspy.biasT = settings->value("AIRSPY/bias-T", false).toBool();
    m_settings->airspy.dataPacking = settings->value("AIRSPY/dataPacking", true).toBool();
    m_settings->airspy.prefer4096kHz = settings->value("AIRSPY/preferSampleRate4096kHz", true).toBool();
#endif
#if HAVE_SOAPYSDR
    m_settings->soapysdr.devArgs = settings->value("SOAPYSDR/devArgs", QString("driver=rtlsdr")).toString();
    m_settings->soapysdr.antenna = settings->value("SOAPYSDR/antenna", QString("RX")).toString();
    m_settings->soapysdr.channel = settings->value("SOAPYSDR/rxChannel", 0).toInt();
    m_settings->soapysdr.bandwidth = settings->value("SOAPYSDR/bandwidth", 0).toUInt();
    m_settings->soapysdr.ppm = settings->value("SOAPYSDR/ppm", 0).toInt();
    m_settings->soapysdr.driver = settings->value("SOAPYSDR/driver", 0).toString();
    settings->beginGroup("SOAPYSDR");
    QStringList groups = settings->childGroups();
    for (auto it = groups.cbegin(); it != groups.cend(); ++it)
    {
        SoapyGainStruct gainStr;
        gainStr.mode = static_cast<SoapyGainMode>(settings->value(*it + "/mode").toInt());
        int numGains = settings->beginReadArray(*it + "/gainList");
        for (int g = 0; g < numGains; ++g)
        {
            settings->setArrayIndex(g);
            gainStr.gainList.append(settings->value("gain").toFloat());
        }
        settings->endArray();
        m_settings->soapysdr.gainMap.insert(*it, gainStr);
    }
    settings->endGroup();

    m_settings->sdrplay.hwId = settings->value("SDRPLAY/lastDevice");
    m_settings->sdrplay.fallbackConnection = settings->value("SDRPLAY/fallbackConnection", true).toBool();
    m_settings->sdrplay.antenna = settings->value("SDRPLAY/antenna", QString("")).toString();
    m_settings->sdrplay.channel = settings->value("SDRPLAY/rxChannel", 0).toInt();
    m_settings->sdrplay.gain.mode =
        static_cast<SdrPlayGainMode>(settings->value("SDRPLAY/gainMode", static_cast<int>(SdrPlayGainMode::Software)).toInt());
    m_settings->sdrplay.gain.rfGain = settings->value("SDRPLAY/rfGain", -1).toInt();
    m_settings->sdrplay.gain.ifGain = settings->value("SDRPLAY/ifGain", 0).toInt();
    m_settings->sdrplay.gain.ifAgcEna = settings->value("SDRPLAY/ifAgcEna", true).toBool();
    m_settings->sdrplay.ppm = settings->value("SDRPLAY/ppm", 0).toInt();
    m_settings->sdrplay.biasT = settings->value("SDRPLAY/bias-T", false).toBool();

#endif
    m_settings->rawfile.file = settings->value("RAW-FILE/filename", QVariant(QString(""))).toString();
    m_settings->rawfile.format = RawFileInputFormat(settings->value("RAW-FILE/format", 0).toInt());
    m_settings->rawfile.loopEna = settings->value("RAW-FILE/loop", false).toBool();

    m_settingsBackend->setSettings(m_settings);

    // set DAB time locale
    m_timeLocale = QLocale(m_settingsBackend->applicationLanguage());
    EPGTime::getInstance()->setTimeLocale(m_timeLocale);

    setColorTheme();

    m_navigationModel->loadSettings(settings);

    if (InputDevice::Id::UNDEFINED != static_cast<InputDevice::Id>(inDevice))
    {
        switch (m_settings->inputDevice)
        {
            case InputDevice::Id::RTLSDR:
                // try to init last device
                initInputDevice(m_settings->inputDevice, m_settings->rtlsdr.hwId);
                break;
            case InputDevice::Id::AIRSPY:
                // try to init last device
#if HAVE_AIRSPY
                initInputDevice(m_settings->inputDevice, m_settings->airspy.hwId);
#endif
                break;
            case InputDevice::Id::SDRPLAY:
#if HAVE_SOAPYSDR
                initInputDevice(m_settings->inputDevice, m_settings->sdrplay.hwId);
#endif
                break;
            case InputDevice::Id::RTLTCP:
            case InputDevice::Id::RAWFILE:
            case InputDevice::Id::SOAPYSDR:
            case InputDevice::Id::RARTTCP:
                initInputDevice(m_settings->inputDevice, QVariant());
            default:
                // do nothing
                break;
        }

        // restore service for live stream devices
        if (m_inputDevice && (m_inputDevice->capabilities() & InputDevice::Capability::LiveStream))
        {  // restore channel
            int sid = settings->value("SID", 0).toInt();
            uint8_t scids = settings->value("SCIdS", 0).toInt();
            selectService(ServiceListId(sid, scids));
        }
    }

    delete settings;

    if (m_inputDevice && (m_inputDevice->capabilities() & InputDevice::Capability::LiveStream) && (m_serviceList->numServices() == 0))
    {
        QTimer::singleShot(1, this, [this]() { emit startBandScan(); });
    }

    bool showSettings =
        (InputDevice::Id::UNDEFINED == m_inputDeviceId) || ((InputDevice::Id::RAWFILE == m_inputDeviceId) && (m_settings->rawfile.file.isEmpty()));
    QTimer::singleShot(500, this,
                       [this, showSettings]()
                       {
                           restoreWindows();
                           if (showSettings)
                           {
                               emit showPageRequest(NavigationModel::Settings);
                           }
                       });
}

void Application::saveSettings()
{
    m_ensembleInfoBackend->saveSettings();

    QSettings *settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    if (AppVersion(settings->value("version").toString()) < AppVersion(PROJECT_VER))
    {
        if ((InputDevice::Id::RAWFILE != m_inputDeviceId) && (InputDevice::Id::UNDEFINED != m_inputDeviceId))
        {  // this prevents deleting service list
            settings->clear();
            settings->setValue("version", PROJECT_VER);
        }
    }
    settings->setValue("inputDeviceId", int(m_settings->inputDevice));

    if (m_audioOutputMenuModel)
    {
        for (int n = 0; n < m_audioOutputMenuModel->rowCount(); ++n)
        {
            auto idx = m_audioOutputMenuModel->index(n, 0);
            auto isChecked = m_audioOutputMenuModel->data(idx, ContextMenuModel::CheckedRole).toBool();
            if (isChecked)
            {
                auto id = m_audioOutputMenuModel->data(idx, ContextMenuModel::DataRole).value<QAudioDevice>().id();
                settings->setValue("audioDevice", id);
                break;
            }
        }
    }
    settings->setValue("audioFramework", m_settings->audioFramework);
    settings->setValue("audioDecoderAAC", m_settings->audioDecoder);
    settings->setValue("volume", m_ui->audioVolume());
    settings->setValue("mute", m_ui->isMuted());
    settings->setValue("keepServiceListOnScan", m_settings->keepServiceListOnScan);
    settings->setValue("style", static_cast<int>(m_settings->applicationStyle));
    settings->setValue("announcementEna", m_settings->announcementEna);
    settings->setValue("bringWindowToForegroundOnAlarm", m_settings->bringWindowToForeground);
    settings->setValue("dlPlus", m_settings->dlPlusEna);
    settings->setValue("language", QLocale::languageToCode(m_settings->lang));
    settings->setValue("noiseConcealment", m_settings->noiseConcealmentLevel);
    settings->setValue("rawFileXmlHeader", m_settings->xmlHeaderEna);
    settings->setValue("spiAppEna", m_settings->spiAppEna);
    settings->setValue("spiProgressEna", m_settings->spiProgressEna);
    settings->setValue("spiProgressHideComplete", m_settings->spiProgressHideComplete);
    settings->setValue("useInternet", m_settings->useInternet);
    settings->setValue("radioDNS", m_settings->radioDnsEna);
    settings->setValue("slsBg", m_settings->slsBackground.name(QColor::HexArgb));
    settings->setValue("updateCheckEna", m_settings->updateCheckEna);
    settings->setValue("updateCheckTime", m_settings->updateCheckTime);
    settings->setValue("uploadEnsembleInfoEna", m_settings->uploadEnsembleInfo);
    settings->setValue("showTrayIcon", m_settings->trayIconEna);
    settings->setValue("restoreWindows", m_settings->restoreWindows);
    settings->setValue("showSystemTime", m_settings->showSystemTime);
    settings->setValue("showEnsembleCountryFlag", m_settings->showEnsFlag);
    settings->setValue("showServiceCountryFlag", m_settings->showServiceFlag);
    settings->setValue("dataStoragePath", m_settings->dataStoragePath);
    settings->setValue("compactUi", m_settings->compactUi);
    settings->setValue("cableChannelsEna", m_settings->cableChannelsEna);

    settings->setValue("AppWindow/x", m_settings->appWindow.x);
    settings->setValue("AppWindow/y", m_settings->appWindow.y);
    settings->setValue("AppWindow/width", m_settings->appWindow.width);
    settings->setValue("AppWindow/height", m_settings->appWindow.height);
    settings->setValue("AppWindow/fullscreen", m_settings->appWindow.fullscreen);

    settings->setValue("AudioRecording/captureOutput", m_settings->audioRec.captureOutput);
    settings->setValue("AudioRecording/autoStop", m_settings->audioRec.autoStopEna);
    settings->setValue("AudioRecording/DL", m_settings->audioRec.dl);
    settings->setValue("AudioRecording/DLAbsTime", m_settings->audioRec.dlAbsTime);

    settings->setValue("EPG/filterEmpty", m_settings->epg.filterEmptyEpg);
    settings->setValue("EPG/filterOtherEnsembles", m_settings->epg.filterEnsemble);
    settings->setValue("EPG/layout", m_settings->epg.splitterState);
    settings->setValue("EPG/restore", m_settings->epg.restore);

    settings->setValue("TII/locationSource", static_cast<int>(m_settings->tii.locationSource));
    settings->setValue("TII/latitude", m_settings->tii.coordinates.latitude());
    settings->setValue("TII/longitude", m_settings->tii.coordinates.longitude());
    settings->setValue("TII/serialPort", m_settings->tii.serialPort);
    settings->setValue("TII/serialPortBaudrate", m_settings->tii.serialPortBaudrate);
    settings->setValue("TII/showSpectrumPlot", m_settings->tii.showSpectumPlot);
    settings->setValue("TII/restore", m_settings->tii.restore);
    settings->setValue("TII/layout", m_settings->tii.splitterState);
    settings->setValue("TII/configuration", m_settings->tii.mode);
    settings->setValue("TII/showInactiveTx", m_settings->tii.showInactiveTx);
    settings->setValue("TII/inactiveTxTimeoutEna", m_settings->tii.inactiveTxTimeoutEna);
    settings->setValue("TII/inactiveTxTimeout", m_settings->tii.inactiveTxTimeout);
    settings->setValue("TII/timestampInUTC", m_settings->tii.timestampInUTC);
    settings->setValue("TII/saveCoordinates", m_settings->tii.saveCoordinates);
    settings->setValue("TII/mapCenterCurrPos", m_settings->tii.centerMapToCurrentPosition);
    settings->setValue("TII/mapCenterLat", m_settings->tii.mapCenter.latitude());
    settings->setValue("TII/mapCenterLon", m_settings->tii.mapCenter.longitude());
    settings->setValue("TII/mapZoom", m_settings->tii.mapZoom);

    settings->setValue("TII/txTable/codeIdx", m_settings->tii.txTable.code.order);
    settings->setValue("TII/txTable/levelIdx", m_settings->tii.txTable.level.order);
    settings->setValue("TII/txTable/distEna", m_settings->tii.txTable.dist.enabled);
    settings->setValue("TII/txTable/distIdx", m_settings->tii.txTable.dist.order);
    settings->setValue("TII/txTable/azimuthEna", m_settings->tii.txTable.azimuth.enabled);
    settings->setValue("TII/txTable/azimuthIdx", m_settings->tii.txTable.azimuth.order);
    settings->setValue("TII/txTable/powerEna", m_settings->tii.txTable.power.enabled);
    settings->setValue("TII/txTable/powerIdx", m_settings->tii.txTable.power.order);
    settings->setValue("TII/txTable/locationEna", m_settings->tii.txTable.location.enabled);
    settings->setValue("TII/txTable/locationIdx", m_settings->tii.txTable.location.order);

    settings->setValue("Scanner/layout", m_settings->scanner.splitterState);
    settings->setValue("Scanner/restore", m_settings->scanner.restore);
    settings->setValue("Scanner/mode", m_settings->scanner.mode);
    settings->setValue("Scanner/numCycles", m_settings->scanner.numCycles);
    settings->setValue("Scanner/clearOnStart", m_settings->scanner.clearOnStart);
    settings->setValue("Scanner/hideLocalTx", m_settings->scanner.hideLocalTx);
    settings->setValue("Scanner/autoSave", m_settings->scanner.autoSave);
    settings->setValue("Scanner/mapCenterCurrPos", m_settings->scanner.centerMapToCurrentPosition);
    settings->setValue("Scanner/mapCenterLat", m_settings->scanner.mapCenter.latitude());
    settings->setValue("Scanner/mapCenterLon", m_settings->scanner.mapCenter.longitude());
    settings->setValue("Scanner/mapZoom", m_settings->scanner.mapZoom);

    settings->beginWriteArray("Scanner/channels");
    int ch = 0;
    for (auto it = m_settings->scanner.channelSelection.cbegin(); it != m_settings->scanner.channelSelection.cend(); ++it)
    {
        settings->setArrayIndex(ch++);
        settings->setValue("frequency", it.key());
        settings->setValue("active", it.value());
    }
    settings->endArray();

    settings->setValue("EnsembleInfo/restore", m_settings->ensembleInfo.restore);
    settings->setValue("EnsembleInfo/recordingTimeoutEna", m_settings->ensembleInfo.recordingTimeoutEna);
    settings->setValue("EnsembleInfo/recordingTimeoutSec", m_settings->ensembleInfo.recordingTimeoutSec);

    settings->setValue("Log/restore", m_settings->log.restore);

    settings->setValue("SetupDialog/restore", m_settings->setupDialog.restore);

    settings->setValue("CatSLS/restore", m_settings->catSls.restore);

#ifndef Q_OS_ANDROID
    settings->setValue("Proxy/config", static_cast<int>(m_settings->proxy.config));
    settings->setValue("Proxy/server", m_settings->proxy.server);
    settings->setValue("Proxy/port", m_settings->proxy.port);
    settings->setValue("Proxy/user", m_settings->proxy.user);
    settings->setValue("Proxy/pass", m_settings->proxy.pass.toBase64());
#endif

    settings->setValue("SignalDialog/layout", m_settings->signal.splitterState);
    settings->setValue("SignalDialog/restore", m_settings->signal.restore);
    settings->setValue("SignalDialog/spectrumMode", m_settings->signal.spectrumMode);
    settings->setValue("SignalDialog/spectrumUpdate", m_settings->signal.spectrumUpdate);
    settings->setValue("SignalDialog/showSNR", m_settings->signal.showSNR);

    settings->setValue("UA-STORAGE/overwriteEna", m_settings->uaDump.overwriteEna);
    settings->setValue("UA-STORAGE/slsEna", m_settings->uaDump.slsEna);
    settings->setValue("UA-STORAGE/spiEna", m_settings->uaDump.spiEna);
    settings->setValue("UA-STORAGE/slsPattern", m_settings->uaDump.slsPattern);
    settings->setValue("UA-STORAGE/spiPattern", m_settings->uaDump.spiPattern);

    settings->setValue("RTL-SDR/lastDevice", m_settings->rtlsdr.hwId);
    settings->setValue("RTL-SDR/fallbackConnection", m_settings->rtlsdr.fallbackConnection);
    settings->setValue("RTL-SDR/gainIndex", m_settings->rtlsdr.gainIdx);
    settings->setValue("RTL-SDR/gainMode", static_cast<int>(m_settings->rtlsdr.gainMode));
    settings->setValue("RTL-SDR/bandwidth", m_settings->rtlsdr.bandwidth);
    settings->setValue("RTL-SDR/bias-T", m_settings->rtlsdr.biasT);
    settings->setValue("RTL-SDR/agcLevelMax", m_settings->rtlsdr.agcLevelMax);
    settings->setValue("RTL-SDR/ppm", m_settings->rtlsdr.ppm);
    settings->setValue("RTL-SDR/rfLevelOffset", m_settings->rtlsdr.rfLevelOffset);

#if HAVE_AIRSPY
    settings->setValue("AIRSPY/lastDevice", m_settings->airspy.hwId);
    settings->setValue("AIRSPY/fallbackConnection", m_settings->airspy.fallbackConnection);
    settings->setValue("AIRSPY/sensitivityGainIdx", m_settings->airspy.gain.sensitivityGainIdx);
    settings->setValue("AIRSPY/lnaGainIdx", m_settings->airspy.gain.lnaGainIdx);
    settings->setValue("AIRSPY/mixerGainIdx", m_settings->airspy.gain.mixerGainIdx);
    settings->setValue("AIRSPY/ifGainIdx", m_settings->airspy.gain.ifGainIdx);
    settings->setValue("AIRSPY/lnaAgcEna", m_settings->airspy.gain.lnaAgcEna);
    settings->setValue("AIRSPY/mixerAgcEna", m_settings->airspy.gain.mixerAgcEna);
    settings->setValue("AIRSPY/gainMode", static_cast<int>(m_settings->airspy.gain.mode));
    settings->setValue("AIRSPY/bias-T", m_settings->airspy.biasT);
    settings->setValue("AIRSPY/dataPacking", m_settings->airspy.dataPacking);
    settings->setValue("AIRSPY/preferSampleRate4096kHz", m_settings->airspy.prefer4096kHz);
#endif

#if HAVE_SOAPYSDR
    settings->setValue("SOAPYSDR/devArgs", m_settings->soapysdr.devArgs);
    settings->setValue("SOAPYSDR/rxChannel", m_settings->soapysdr.channel);
    settings->setValue("SOAPYSDR/antenna", m_settings->soapysdr.antenna);
    settings->setValue("SOAPYSDR/bandwidth", m_settings->soapysdr.bandwidth);
    settings->setValue("SOAPYSDR/ppm", m_settings->soapysdr.ppm);
    settings->setValue("SOAPYSDR/driver", m_settings->soapysdr.driver);
    for (auto it = m_settings->soapysdr.gainMap.cbegin(); it != m_settings->soapysdr.gainMap.cend(); ++it)
    {
        settings->beginGroup("SOAPYSDR/" + it.key());
        settings->setValue("mode", static_cast<int>(it.value().mode));
        settings->beginWriteArray("gainList");
        for (int g = 0; g < it.value().gainList.count(); ++g)
        {
            settings->setArrayIndex(g);
            settings->setValue("gain", it.value().gainList.at(g));
        }
        settings->endArray();
        settings->endGroup();
    }
    settings->setValue("SDRPLAY/lastDevice", m_settings->sdrplay.hwId);
    settings->setValue("SDRPLAY/fallbackConnection", m_settings->sdrplay.fallbackConnection);
    settings->setValue("SDRPLAY/rxChannel", m_settings->sdrplay.channel);
    settings->setValue("SDRPLAY/antenna", m_settings->sdrplay.antenna);
    settings->setValue("SDRPLAY/gainMode", static_cast<int>(m_settings->sdrplay.gain.mode));
    settings->setValue("SDRPLAY/rfGain", m_settings->sdrplay.gain.rfGain);
    settings->setValue("SDRPLAY/ifGain", m_settings->sdrplay.gain.ifGain);
    settings->setValue("SDRPLAY/ifAgcEna", m_settings->sdrplay.gain.ifAgcEna);
    settings->setValue("SDRPLAY/ppm", m_settings->sdrplay.ppm);
    settings->setValue("SDRPLAY/bias-T", m_settings->sdrplay.biasT);
#endif

    settings->setValue("RTL-TCP/gainIndex", m_settings->rtltcp.gainIdx);
    settings->setValue("RTL-TCP/gainMode", static_cast<int>(m_settings->rtltcp.gainMode));
    settings->setValue("RTL-TCP/address", m_settings->rtltcp.tcpAddress);
    settings->setValue("RTL-TCP/port", m_settings->rtltcp.tcpPort);
    settings->setValue("RTL-TCP/controlSocket", m_settings->rtltcp.controlSocketEna);
    settings->setValue("RTL-TCP/agcLevelMax", m_settings->rtltcp.agcLevelMax);
    settings->setValue("RTL-TCP/ppm", m_settings->rtltcp.ppm);
    settings->setValue("RTL-TCP/rfLevelOffset", m_settings->rtltcp.rfLevelOffset);

#if HAVE_RARTTCP
    settings->setValue("RART-TCP/address", m_settings->rarttcp.tcpAddress);
    settings->setValue("RART-TCP/port", m_settings->rarttcp.tcpPort);
#endif

    settings->setValue("RAW-FILE/filename", m_settings->rawfile.file);
    settings->setValue("RAW-FILE/format", int(m_settings->rawfile.format));
    settings->setValue("RAW-FILE/loop", m_settings->rawfile.loopEna);

    if ((InputDevice::Id::RAWFILE != m_inputDeviceId) && (InputDevice::Id::UNDEFINED != m_inputDeviceId))
    {  // save current service and service list
        QModelIndex current = m_slSelectionModel->currentIndex();
        const SLModel *model = reinterpret_cast<const SLModel *>(current.model());
        ServiceListConstIterator it = m_serviceList->findService(model->id(current));
        if (m_serviceList->serviceListEnd() != it)
        {
            m_SId = (*it)->SId();
            m_SCIdS = (*it)->SCIdS();
            m_frequency = (*it)->getEnsemble()->frequency();

            settings->setValue("SID", m_SId.value());
            settings->setValue("SCIdS", m_SCIdS);
            settings->setValue("Frequency", m_frequency);
        }
        m_serviceList->save(m_serviceListFilename);

        // save audio schedule
        m_audioRecScheduleModel->save(m_audioRecScheduleFilename);
    }
    else
    { /* RAW file does not store service, service list and schedule */
    }

    m_navigationModel->saveSettings(settings);

    settings->sync();
    delete settings;
}

void Application::setCurrentServiceFavorite(bool checked)
{
    QModelIndex idx = m_slSelectionModel->currentIndex();
    ServiceListId id = m_slModel->id(idx);
    m_serviceList->setServiceFavorite(id, checked);
    m_ui->isServiceFavorite(checked);
    m_slModel->sort(0);

    // find new position of current service and select it
    for (int r = 0; r < m_slModel->rowCount(); ++r)
    {
        QModelIndex index = m_slModel->index(r, 0);
        if (m_slModel->id(index) == id)
        {  // found
            m_slSelectionModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
            break;
        }
    }
}

void Application::setServiceFavorite(const QModelIndex &index, bool checked)
{
    QModelIndex currentIdx = m_slSelectionModel->currentIndex();
    ServiceListId currentId = m_slModel->id(currentIdx);
    ServiceListId id;

    if (dynamic_cast<const SLModel *>(index.model()))
    {  // index belongs to service list model
        id = m_slModel->id(index);
        m_serviceList->setServiceFavorite(id, checked);
    }

    if (dynamic_cast<const SLTreeModel *>(index.model()))
    {  // index belongs to service list tree model
        id = m_slTreeModel->id(index);
        m_serviceList->setServiceFavorite(id, checked);
    }

    m_slModel->sort(0);
    m_slTreeModel->sort(0);

    // if current service favorite changed, update UI
    if (id == currentId)
    {
        m_ui->isServiceFavorite(checked);
    }

    // // find new position of current service and select it
    // for (int r = 0; r < m_slModel->rowCount(); ++r)
    // {
    //     QModelIndex idx = m_slModel->index(r, 0);
    //     if (m_slModel->id(idx) == currentId)
    //     {  // found
    //         m_slSelectionModel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    //         break;
    //     }
    // }
    serviceListViewUpdateSelection();
    serviceTreeViewUpdateSelection();
}

void Application::onAudioVolumeChanged()
{
    if (!m_ui->isMuted())
    {
        emit audioVolume(m_ui->audioVolume());
    }
    else
    { /* we are muted -> no volume change for user is possible */
    }
}

void Application::onMuteButtonToggled(bool doMute)
{
    m_ui->isMuted(doMute);
    emit audioMute(doMute);
}

void Application::copyDlToClipboard()
{
    // Retrieve the rich text from the QLabel
    QString richText = m_ui->dlService();
    if (m_ui->serviceInstance() == Instance::Announcement)
    {
        richText = m_ui->dlAnnouncement();
    }

    // Use QTextDocument to convert rich text to plain text
    QTextDocument textDoc;
    textDoc.setHtml(richText);
    QString plainText = textDoc.toPlainText();

    QGuiApplication::clipboard()->setText(plainText);
    // QToolTip::showText(label->mapToGlobal(event->pos()), tr("<i>DL text copied to clipboard</i>"));
}

void Application::copyDlPlusToClipboard()
{
    auto model = m_dlPlusModel[m_ui->serviceInstance()];
    QString text;
    // go through the model and take DL plus labels
    for (int r = 0; r < model->rowCount(); ++r)
    {
        QModelIndex index = model->index(r, 0);
        QString tag = model->data(index, DLPlusModel::Roles::TagRole).toString();

        // Use QTextDocument to convert rich text to plain text
        QTextDocument textDoc;
        textDoc.setHtml(model->data(index, DLPlusModel::Roles::ContentRole).toString());
        QString label = textDoc.toPlainText();
        text += QString("%1: %2\n").arg(tag, label);
    }
    QGuiApplication::clipboard()->setText(text);
    // QToolTip::showText(ui->dlPlusFrame_Service->mapToGlobal(event->pos()), tr("<i>DL+ text copied to clipboard</i>"));
}

void Application::onAnnouncementClicked()
{
    m_ui->isAnnouncementButtonEnabled(false);
    emit toggleAnnouncement();
}

void Application::serviceTreeViewUpdateSelection()
{
    ServiceListId serviceId(m_SId.value(), uint8_t(m_SCIdS));
    ServiceListId ensembleId;
    ServiceListConstIterator it = m_serviceList->findService(serviceId);
    if (m_serviceList->serviceListEnd() != it)
    {  // found
        ensembleId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->id();
    }
    for (int r = 0; r < m_slTreeModel->rowCount(); ++r)
    {  // go through ensembles
        QModelIndex ensembleIndex = m_slTreeModel->index(r, 0);
        if (m_slTreeModel->id(ensembleIndex) == ensembleId)
        {  // found ensemble
            for (int s = 0; s < m_slTreeModel->rowCount(ensembleIndex); ++s)
            {  // go through services
                QModelIndex serviceIndex = m_slTreeModel->index(s, 0, ensembleIndex);
                if (m_slTreeModel->id(serviceIndex) == serviceId)
                {  // found
                    // ui->serviceTreeView->setCurrentIndex(serviceIndex);
                    m_slTreeSelectionModel->setCurrentIndex(serviceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                    return;
                }
            }
        }
    }
}

void Application::serviceListViewUpdateSelection()
{
    ServiceListId id(m_SId.value(), uint8_t(m_SCIdS));
    QModelIndex index;
    for (int r = 0; r < m_slModel->rowCount(); ++r)
    {
        index = m_slModel->index(r, 0);
        if (m_slModel->id(index) == id)
        {  // found
            // ui->serviceListView->setCurrentIndex(index);
            m_slSelectionModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
            return;
        }
    }
}

void Application::toggleDLPlus(bool toggle)
{
    m_ui->isDlPlusVisible(toggle && m_settings->dlPlusEna);
}

QObject *Application::createBackend(int id)
{
    QObject *backend = nullptr;
    switch (static_cast<NavigationModel::NavigationId>(id))
    {
        case NavigationModel::About:
            backend = new AboutUI(this);
            break;

        case NavigationModel::BandScan:
        {
            auto bandScanBackend = new BandScanBackend((m_serviceList->numServices() == 0), m_settings, this);
            backend = bandScanBackend;
            connect(bandScanBackend, &BandScanBackend::tuneChannel, this, &Application::onTuneChannel);
            connect(m_radioControl, &RadioControl::signalState, bandScanBackend, &BandScanBackend::onSyncStatus, Qt::QueuedConnection);
            connect(m_radioControl, &RadioControl::ensembleInformation, bandScanBackend, &BandScanBackend::onEnsembleFound, Qt::QueuedConnection);
            connect(m_radioControl, &RadioControl::tuneDone, bandScanBackend, &BandScanBackend::onTuneDone, Qt::QueuedConnection);
            connect(m_radioControl, &RadioControl::serviceListComplete, bandScanBackend, &BandScanBackend::onServiceListComplete,
                    Qt::QueuedConnection);
            connect(m_radioControl, &RadioControl::serviceListEntry, bandScanBackend, &BandScanBackend::onServiceListEntry, Qt::QueuedConnection);
            connect(bandScanBackend, &BandScanBackend::scanStarts, this, &Application::onBandScanStart);
            connect(bandScanBackend, &BandScanBackend::done, this, &Application::onBandScanFinished);
        }
        break;
        case NavigationModel::Tii:
            backend = createTiiBackend();
            break;
        case NavigationModel::DabSignal:
            backend = createSignalBackend();
            break;
        case NavigationModel::Scanner:
            backend = createScannerBackend();
            break;
        case NavigationModel::Epg:
            backend = createEpgBackend();
            break;
        case NavigationModel::CatSls:
            backend = createCatSlsBackend();
            break;
        case NavigationModel::Undefined:
        case NavigationModel::Service:
        case NavigationModel::EnsembleInfo:
        case NavigationModel::ServiceList:
        case NavigationModel::Settings:
        case NavigationModel::AppLog:
        case NavigationModel::ShowMore:
        default:
            break;
    }
    return backend;
}

void Application::saveWindowGeometry(int x, int y, int width, int height) const
{
    m_settings->appWindow.x = x;
    m_settings->appWindow.y = y;
    m_settings->appWindow.width = width;
    m_settings->appWindow.height = height;
}

QVariantMap Application::restoreWindowGeometry()
{
    QVariantMap geometry;
    geometry["x"] = m_settings->appWindow.x;
    geometry["y"] = m_settings->appWindow.y;
    geometry["width"] = m_settings->appWindow.width;
    geometry["height"] = m_settings->appWindow.height;

    return geometry;
}

void Application::saveUndockedWindows() const
{
    for (int i = 0; i < m_navigationModel->rowCount(); ++i)
    {
        auto item = m_navigationModel->getItem(i);
        if (item && item->isUndockable())
        {
            switch (item->id())
            {
                case NavigationModel::EnsembleInfo:
                    m_settings->ensembleInfo.restore = item->isUndocked();
                    break;
                case NavigationModel::Tii:
                    m_settings->tii.restore = item->isUndocked();
                    break;
                case NavigationModel::Scanner:
                    m_settings->scanner.restore = item->isUndocked();
                    break;
                case NavigationModel::Epg:
                    m_settings->epg.restore = item->isUndocked();
                    break;
                case NavigationModel::DabSignal:
                    m_settings->signal.restore = item->isUndocked();
                    break;
                case NavigationModel::CatSls:
                    m_settings->catSls.restore = item->isUndocked();
                    break;
                case NavigationModel::Settings:
                    m_settings->setupDialog.restore = item->isUndocked();
                    break;
                case NavigationModel::AppLog:
                    m_settings->log.restore = item->isUndocked();
                    break;
                default:
                    break;
            }
        }
    }
}

void Application::close()
{
    // Other platforms can use graceful shutdown
    if (0 == m_frequency)
    {  // in idle
        emit exit();

        saveSettings();

        m_allowQuitEvent = true;  // allow the quit event to proceed if we intercepted it

        qApp->exit(m_exitCode);
    }
    else
    {  // message is needed for Windows -> stopping RTL SDR thread may take long time :-(
        // m_infoLabel->setText(tr("Stopping DAB processing, please wait..."));
        // m_infoLabel->setToolTip("");

        m_exitRequested = true;
        emit resetUserApps();
        emit serviceRequest(0, 0, 0);
    }
}

bool Application::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == qApp) && (event->type() == QEvent::Quit))
    {
        // this is triggered when application is close from OS, eg. from task bar or when OS reboots/shuts down
        if (m_allowQuitEvent)
        {  // we've already cleaned up, let Qt proceed
            return QObject::eventFilter(obj, event);
        }

        if (!m_quitEventIntercepted)
        {
            m_quitEventIntercepted = true;
            emit applicationQuitEvent();
        }
        return true;  // swallow the quit until cleanup finishes
    }
    return QObject::eventFilter(obj, event);
}

void Application::onApplicationStateChanged(Qt::ApplicationState state)
{
    // On Android, when the user swipes the app out of the recent apps list,
    // the applicationStateChanged signal is emitted with Qt::ApplicationSuspended
    // before the app is terminated. This is our opportunity to save settings.
#ifdef Q_OS_ANDROID
    qCInfo(application) << "Application state changed:" << state;

    if (state == Qt::ApplicationSuspended)
    {
        qCInfo(application, "Application suspended - saving settings");
        saveSettings();
        // Note: Audio playback continues in background via wake lock and audio focus.
        // The decoder will naturally buffer audio as needed.
        // Audio will only stop when the app is terminated by the system.
    }
    else if (state == Qt::ApplicationInactive)
    {
        // App is losing focus but not suspended yet
        // Don't stop audio here - let it continue playing in background
        qCInfo(application, "Application becoming inactive - audio continues");
    }
    else if (state == Qt::ApplicationActive)
    {
        qCInfo(application, "Application active - audio continues playing");
    }
#else
    // On desktop platforms, we rely on the close() method being called when
    // the user closes the window via the X button, which triggers saveSettings().
    // This handler exists for future multi-platform enhancements if needed.
    (void)state;  // Suppress unused parameter warning on desktop platforms
#endif
}

void Application::setAndroidNavigationBar()
{
#ifdef Q_OS_ANDROID
    try
    {
        // Get the Activity context
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid())
        {
            return;
        }

        // Get the color and convert to ARGB int
        QColor color = m_ui->isPortraitView() ? m_ui->color(ApplicationUI::Background) : m_ui->color(ApplicationUI::StatusbarBackground);
        int colorInt = (color.alpha() << 24) | (color.red() << 16) | (color.green() << 8) | color.blue();

        // Call the Java helper class - it handles threading automatically via runOnUiThread()
        QJniObject::callStaticMethod<void>("org/qtproject/abracadabra/NavigationBarHelper", "updateNavigationBarColor", "(Landroid/app/Activity;IZ)V",
                                           activity.object(), colorInt, isDarkMode());
    }
    catch (const std::exception &e)
    {
        qWarning() << "Exception setting Android navigation bar:" << QString::fromStdString(e.what());
    }
#endif
}

QObject *Application::createTiiBackend()
{
    if (m_tiiBackend == nullptr)
    {
        m_tiiBackend = new TIIBackend(m_settings, this);
        connect(m_settingsBackend, &SettingsBackend::tiiSettingsChanged, m_tiiBackend, &TIIBackend::onSettingsChanged);
        connect(m_settingsBackend, &SettingsBackend::tiiTableSettingsChanged, m_tiiBackend, &TIIBackend::onTxTableSettingsChanged);
        connect(m_tiiBackend, &TIIBackend::setTii, m_radioControl, &RadioControl::startTii, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::signalState, m_tiiBackend, &TIIBackend::onSignalState, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::tiiData, m_tiiBackend, &TIIBackend::onTiiData, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::ensembleInformation, m_tiiBackend, &TIIBackend::onEnsembleInformation, Qt::QueuedConnection);
        m_tiiBackend->setIsActive(m_navigationModel->isActive(NavigationModel::Tii));
        emit getEnsembleInfo();  // this triggers ensemble infomation used to configure EPG dialog
    }
    return m_tiiBackend;
}

QObject *Application::createSignalBackend()
{
    if (m_signalBackend == nullptr)
    {
        m_signalBackend = new SignalBackend(m_settings, m_frequency);
        connect(m_signalBackend, &SignalBackend::setSignalSpectrum, m_radioControl, &RadioControl::setSignalSpectrum, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::tuneDone, m_signalBackend, &SignalBackend::onTuneDone, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::freqOffset, m_signalBackend, &SignalBackend::updateFreqOffset, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::signalSpectrum, m_signalBackend, &SignalBackend::onSignalSpectrum, Qt::QueuedConnection);
        if (m_inputDevice)
        {
            connect(m_inputDevice, &InputDevice::rfLevel, m_signalBackend, &SignalBackend::updateRfLevel);
        }
        m_signalBackend->setIsActive(m_navigationModel->isActive(NavigationModel::DabSignal));

        m_signalBackend->setInputDevice(m_inputDeviceId);
    }
    return m_signalBackend;
}

QObject *Application::createScannerBackend()
{
    if (m_scannerBackend == nullptr)
    {
        m_scannerBackend = new ScannerBackend(m_settings, this);
        connect(m_scannerBackend, &ScannerBackend::tuneChannel, this, &Application::onTuneChannel);
        connect(m_settingsBackend, &SettingsBackend::tiiSettingsChanged, m_scannerBackend, &ScannerBackend::onSettingsChanged);
        connect(m_radioControl, &RadioControl::signalState, m_scannerBackend, &ScannerBackend::onSignalState, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::ensembleInformation, m_scannerBackend, &ScannerBackend::onEnsembleInformation, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::tuneDone, m_scannerBackend, &ScannerBackend::onTuneDone, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::serviceListEntry, m_scannerBackend, &ScannerBackend::onServiceListEntry, Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::tiiData, m_scannerBackend, &ScannerBackend::onTiiData, Qt::QueuedConnection);
        // ensemble configuration
        connect(m_scannerBackend, &ScannerBackend::requestEnsembleConfiguration, m_radioControl, &RadioControl::getEnsembleConfigurationAndCSV,
                Qt::QueuedConnection);
        connect(m_radioControl, &RadioControl::ensembleConfigurationAndCSV, m_scannerBackend, &ScannerBackend::onEnsembleConfigurationAndCSV,
                Qt::QueuedConnection);
        connect(m_scannerBackend, &ScannerBackend::scanStarts, this,
                [this]()
                {
                    m_scannerBackend->setServiceToRestore(m_SId, m_SCIdS);
                    m_ui->tuneEnabled(false);
                    // m_hasListViewFocus = ui->serviceListView->hasFocus();
                    // m_hasTreeViewFocus = ui->serviceTreeView->hasFocus();
                    m_ui->serviceSelectionEnabled(false);
                    m_settingsBackend->isInputDeviceSelectionEnabled(false);
                    m_navigationModel->setEnabled(NavigationModel::BandScan, false);
                    m_isScannerRunning = true;

                    onBandScanStart();
                });

        connect(m_scannerBackend, &ScannerBackend::setTii, m_radioControl, &RadioControl::startTii, Qt::QueuedConnection);

        connect(m_scannerBackend, &ScannerBackend::scanFinished, this,
                [this]()
                {
                    m_isScannerRunning = false;
                    m_ui->tuneEnabled(true);
                    m_ui->serviceSelectionEnabled(true);
                    m_settingsBackend->isInputDeviceSelectionEnabled(true);
                    m_navigationModel->setEnabled(NavigationModel::BandScan, true);

                    ServiceListId serviceToRestore = m_scannerBackend->getServiceToRestore();
                    if (serviceToRestore.isValid())
                    {
                        QTimer::singleShot(1500, this, [this, serviceToRestore]() { selectService(serviceToRestore); });
                    }
                    else
                    {
                        onBandScanFinished(BandScanBackendResult::Done);
                    }
                });
        if (m_inputDevice)
        {
            connect(m_inputDevice, &InputDevice::error, m_scannerBackend, &ScannerBackend::onInputDeviceError, Qt::QueuedConnection);
        }
        m_scannerBackend->setIsActive(m_navigationModel->isActive(NavigationModel::Scanner));
    }

    return m_scannerBackend;
}

QObject *Application::createEpgBackend()
{
    if (m_epgBackend == nullptr)
    {
        // Create EPG backend
        m_epgBackend = new EPGBackend(m_slModel, m_slSelectionModel, m_metadataManager, m_settings, this);
        connect(m_epgBackend, &EPGBackend::scheduleAudioRecording, this,
                [this](const AudioRecScheduleItem &item) { m_audioRecManager->addItem(item); });

        connect(m_radioControl, &RadioControl::ensembleInformation, m_epgBackend, &EPGBackend::onEnsembleInformation, Qt::QueuedConnection);
        emit getEnsembleInfo();  // this triggers ensemble infomation used to configure EPG dialog
    }
    return m_epgBackend;
}

QObject *Application::createCatSlsBackend()
{
    if (m_catSlsBackend == nullptr)
    {
        m_catSlsBackend = new CatSLSBackend(m_settings, this);
        connect(m_slideShowApp[Instance::Service], &SlideShowApp::categoryUpdate, m_catSlsBackend, &CatSLSBackend::onCategoryUpdate,
                Qt::QueuedConnection);
        connect(m_slideShowApp[Instance::Service], &SlideShowApp::catSlide, m_catSlsBackend, &CatSLSBackend::onCatSlide, Qt::QueuedConnection);
        connect(m_slideShowApp[Instance::Service], &SlideShowApp::resetTerminal, m_catSlsBackend, &CatSLSBackend::reset, Qt::QueuedConnection);
        connect(m_catSlsBackend, &CatSLSBackend::getCurrentCatSlide, m_slideShowApp[Instance::Service], &SlideShowApp::getCurrentCatSlide,
                Qt::QueuedConnection);
        connect(m_catSlsBackend, &CatSLSBackend::getNextCatSlide, m_slideShowApp[Instance::Service], &SlideShowApp::getNextCatSlide,
                Qt::QueuedConnection);

        m_catSlsBackend->slsBackend()->setupDarkMode(isDarkMode());
        connect(m_settingsBackend, &SettingsBackend::slsBackgroundColorChanged, this,
                [this]() { m_catSlsBackend->setSlsBgColor(m_settings->slsBackground); });
        m_qmlEngine->addImageProvider(QStringLiteral("catSLS"), m_catSlsBackend->slsBackend()->provider());
        connect(m_catSlsBackend, &QObject::destroyed, this, [this]() { m_qmlEngine->removeImageProvider("catSLS"); });
    }
    return m_catSlsBackend;
}

void Application::onShowSystemTimeChanged()
{
    if (m_settings->showSystemTime)
    {  // create times and start it if invalid time
        if (m_sysTimeTimer == nullptr)
        {
            m_sysTimeTimer = new QTimer(this);
        }
        m_sysTimeTimer->setTimerType(Qt::VeryCoarseTimer);
        m_sysTimeTimer->setInterval(2 * 1000);  // 2 sec interval
        connect(m_sysTimeTimer, &QTimer::timeout, this,
                [this]()
                {
                    if (m_dabTime.isValid() == false)
                    {  // this shows system time
                        resetDabTime();
                    }
                    else
                    {
                        m_ui->timeLabelToolTip(tr("DAB time"));
                        m_sysTimeTimer->stop();
                    }
                });
    }
    else
    {  // delete timer
        if (m_sysTimeTimer)
        {
            m_sysTimeTimer->stop();
            delete m_sysTimeTimer;
            m_sysTimeTimer = nullptr;
        }
    }
    if (m_dabTime.isValid() == false)
    {
        resetDabTime();
    }
}

void Application::onShowCountryFlagChanged()
{
    if (m_settings->showServiceFlag && m_SId.isValid())
    {  // valid service
        QPixmap countryFlag =
            m_metadataManager->data(ServiceListId(), ServiceListId(m_SId.value(), m_SCIdS), MetadataManager::CountryFlag).value<QPixmap>();
        if (!countryFlag.isNull())
        {
            m_ui->isServiceFlagVisible(true);
            m_ui->serviceFlagToolTip(QString("<b>Country:</b> %1").arg(DabTables::getCountryName(m_SId.value())));
        }
        else
        {
            m_ui->isServiceFlagVisible(false);
        }
    }
    else
    {
        m_ui->isServiceFlagVisible(false);
    }

    if (m_settings->showEnsFlag && m_ueid != 0)
    {  // valid ensemble
        QPixmap countryFlag =
            m_metadataManager->data(ServiceListId(m_frequency, m_ueid), ServiceListId(), MetadataManager::CountryFlag).value<QPixmap>();
        if (!countryFlag.isNull())
        {
            m_ui->isEnsembleFlagVisible(true);
            m_ui->ensembleFlagToolTip(QString("<b>Country:</b> %1").arg(DabTables::getCountryName(m_ueid)));
        }
        else
        {
            m_ui->isEnsembleFlagVisible(false);
        }
    }
    else
    {
        m_ui->isEnsembleFlagVisible(false);
    }
}

void Application::onBandScanFinished(int result)
{
    m_settingsBackend->isInputDeviceSelectionEnabled(true);
    switch (result)
    {
        case BandScanBackendResult::Done:
        case BandScanBackendResult::Interrupted:
        {
            if (m_serviceList->numServices() > 0)
            {  // going to select first service
                QModelIndex index = m_slModel->index(0, 0);
                m_slSelectionModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                // ui->serviceListView->setFocus();
            }
        }
        break;
        case BandScanBackendResult::Cancelled:
            // do nothing
            break;
    }
}

void Application::onTuneChannel(uint32_t freq)
{
    // change combo - find combo index
    setChannelIndex(m_channelListModel->findFrequency(freq));
}

void Application::stop()
{
    if (m_isPlaying)
    {  // stop
        m_audioRecManager->doAudioRecording(false);

        // tune to 0
        setChannelIndex(-1);
    }
}

void Application::exportServiceList()
{
    QString fileName = QString("servicelist_%1.csv").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));

    if (m_serviceList->exportCSV(m_settings->dataStoragePath, fileName))
    {
        emit showInfoMessage(tr("Service list exported"), 1);
    }
    else
    {
        emit showInfoMessage(tr("Failed to export service list"), -1);
    }
}

void Application::clearServiceList()
{
    m_messageBoxBackend->showQuestion(
        tr("Clear service list?"), tr("Current service list including favorites will be deleted. This action is irreversible."),
        [this](MessageBoxBackend::StandardButton result)
        {
            if (result == MessageBoxBackend::StandardButton::Yes)
            {
                QTimer::singleShot(10, this,
                                   [this]()
                                   {
                                       stop();
                                       m_serviceList->clear();
                                   });
            }
        },
        {
            {MessageBoxBackend::StandardButton::Yes, tr("Clear"), MessageBoxBackend::ButtonRole::Negative},
            {MessageBoxBackend::StandardButton::No, tr("Cancel"), MessageBoxBackend::ButtonRole::Neutral},
        },
        MessageBoxBackend::StandardButton::Yes  // default button
    );
}

bool Application::isDarkMode()
{
    if (Settings::ApplicationStyle::Dark == m_settings->applicationStyle)
    {
        return true;
    }
    // else
    if (Settings::ApplicationStyle::Light == m_settings->applicationStyle)
    {
        return false;
    }
    // else
    return m_ui->isSystemDarkMode();
}

void Application::setColorTheme()
{
    bool darkMode = isDarkMode();
    m_ui->setupDarkMode(darkMode);
    m_logBackend->setupDarkMode(darkMode);
    m_slsBackend[Instance::Service]->setupDarkMode(darkMode);
    m_slsBackend[Instance::Announcement]->setupDarkMode(darkMode);
    if (m_catSlsBackend)
    {
        m_catSlsBackend->slsBackend()->setupDarkMode(darkMode);
    }
    setAndroidNavigationBar();
}

void Application::onDLPlusObjReceived_Service(const DLPlusObject &object)
{
    m_dlPlusModel[Instance::Service]->setDlPlusObject(object);
}

void Application::onDLPlusObjReceived_Announcement(const DLPlusObject &object)
{
    m_dlPlusModel[Instance::Announcement]->setDlPlusObject(object);
}

void Application::onDLPlusItemToggle_Service()
{
    m_dlPlusModel[Instance::Service]->itemsToggle();
}

void Application::onDLPlusItemToggle_Announcement()
{
    m_dlPlusModel[Instance::Announcement]->itemsToggle();
}

void Application::onDLPlusItemRunning_Service(bool isRunning)
{
    m_dlPlusModel[Instance::Service]->itemsRunning(isRunning);
}

void Application::onDLPlusItemRunning_Announcement(bool isRunning)
{
    m_dlPlusModel[Instance::Announcement]->itemsRunning(isRunning);
}

void Application::onDLReset_Service()
{
    m_ui->dlService("");
    m_dlPlusModel[Instance::Service]->reset();
    toggleDLPlus(false);
}

void Application::onDLReset_Announcement()
{
    m_ui->dlAnnouncement("");
    m_dlPlusModel[Instance::Announcement]->reset();
    toggleDLPlus(false);
}

SignalBackend *Application::signalBackend()
{
    createSignalBackend();
    return m_signalBackend;
}

TIIBackend *Application::tiiBackend()
{
    createTiiBackend();
    return m_tiiBackend;
}

ScannerBackend *Application::scannerBackend()
{
    createScannerBackend();
    return m_scannerBackend;
}

EPGBackend *Application::epgBackend()
{
    createEpgBackend();
    return m_epgBackend;
}

CatSLSBackend *Application::catSlsBackend()
{
    createCatSlsBackend();
    return m_catSlsBackend;
}

void ApplicationUI::setupDarkMode(bool isDarkMode)
{
    if (isDarkMode)
    {
        // Dark mode colors
        m_colors[Background] = QColor(0x1e1e1e);                   // Dark gray background
        m_colors[BackgroundLight] = QColor(0x2d2d2d);              // Lighter dark gray for elevation
        m_colors[BackgroundDark] = QColor(0x121212);               // Darker for depth
        m_colors[StatusbarBackground] = QColor(0x252525);          // Slightly lighter than background
        m_colors[Divider] = QColor(0x404040);                      // Darker divider for dark theme
        m_colors[Accent] = QColor(0x42A5F5);                       // Lighter blue for better contrast
        m_colors[Disabled] = QColor(0x666666);                     // Darker gray for disabled state
        m_colors[Hovered] = QColor(0x64B5F6);                      // Lighter on hover
        m_colors[Clicked] = QColor(0x1E88E5);                      // Darker on click
        m_colors[Highlight] = QColor(0x1976D2).darker(160);        // Darker highlight for dark theme
        m_colors[Inactive] = QColor(0x757575);                     // Medium gray for inactive
        m_colors[SelectionColor] = QColor(0x42A5F5);               // Matches accent
        m_colors[ListItemHovered] = QColor(0x2d2d2d);              // Subtle hover effect
        m_colors[ListItemSelected] = QColor(0x42A5F5);             // Selected item in the lists
        m_colors[ButtonPrimary] = QColor(0x42A5F5);                // Lighter blue button
        m_colors[ButtonNeutral] = QColor(0x424242);                // Dark gray neutral button
        m_colors[ButtonPositive] = QColor(0x66BB6A);               // Slightly darker green
        m_colors[ButtonNegative] = QColor(0xEF5350);               // Slightly lighter red
        m_colors[ButtonTextPrimary] = QColor(0xe0e0e0);            // Off-white for less strain
        m_colors[ButtonTextNeutral] = QColor(0xe0e0e0);            // Light text on dark button
        m_colors[ButtonTextPositive] = QColor(0x121212);           // Dark text on green
        m_colors[ButtonTextNegative] = QColor("white");            // White text on red
        m_colors[ButtonPrimaryHover] = QColor(0x64B5F6);           // Lighter on hover
        m_colors[ButtonNeutralHover] = QColor(0x4f4f4f);           // Slightly lighter gray
        m_colors[ButtonPositiveHover] = QColor(0x81C784);          // Lighter green
        m_colors[ButtonNegativeHover] = QColor(0xE57373);          // Lighter red
        m_colors[ButtonPrimaryClicked] = QColor(0x1E88E5);         // Darker on click
        m_colors[ButtonNeutralClicked] = QColor(0x303030);         // Darker gray
        m_colors[ButtonPositiveClicked] = QColor(0x4CAF50);        // Darker green
        m_colors[ButtonNegativeClicked] = QColor(0xE53935);        // Darker red
        m_colors[TextPrimary] = QColor(0xe0e0e0);                  // Off-white, easier on eyes
        m_colors[TextSecondary] = QColor(0x9e9e9e);                // Medium gray, readable
        m_colors[TextDisabled] = QColor(0x666666);                 // Darker gray for disabled
        m_colors[TextSelected] = QColor(0xe0e0e0);                 // Off-white for selection
        m_colors[Link] = QColor(0x64B5F6);                         // Lighter blue for links
        m_colors[Icon] = QColor(0xe0e0e0);                         // Off-white icons
        m_colors[IconInactive] = QColor(0x9e9e9e);                 // Medium gray
        m_colors[IconDisabled] = QColor(0x666666);                 // Darker gray
        m_colors[InputBackground] = QColor(0x2d2d2d);              // Dark input background
        m_colors[ControlBackground] = QColor(0x424242);            // Dark control background
        m_colors[ControlBorder] = QColor(0x555555);                // Medium gray border
        m_colors[NoSignal] = QColor(0x757575);                     // Gray for no signal
        m_colors[LowSignal] = QColor(0xff6659);                    // Slightly lighter red
        m_colors[MidSignal] = QColor(0xffc247);                    // Slightly lighter orange
        m_colors[GoodSignal] = QColor(0x66d932);                   // Slightly lighter green
        m_colors[EpgCurrentProgColor] = QColor(0x1565C0);          // Darker blue for current program
        m_colors[EpgCurrentProgProgressColor] = QColor(0x1976D2);  // Medium blue for progress
        m_colors[EmptyLogoColor] = QColor(0x424242);               // Dark gray for empty logo
    }
    else
    {
        // Light mode colors
        m_colors[Background] = QColor(0xf9fafb);
        m_colors[BackgroundLight] = QColor("white");
        m_colors[BackgroundDark] = QColor(0xf9fafb).darker(120);
        m_colors[StatusbarBackground] = QColor(0xf9fafb).darker(110);
        m_colors[Divider] = QColor(0x919eab);
        m_colors[Accent] = QColor(0x1976D2);
        m_colors[Disabled] = QColor(0xcccccc);
        m_colors[Hovered] = QColor(0x1976D2).lighter(130);
        m_colors[Clicked] = QColor(0x1976D2).darker(130);
        m_colors[Highlight] = QColor(0x1976D2).lighter(210);
        m_colors[Inactive] = QColor(0x919eab);
        m_colors[SelectionColor] = QColor(0x1976D2);
        m_colors[ListItemHovered] = QColor(0xeeeeee);
        m_colors[ListItemSelected] = QColor(0x1976D2);
        m_colors[ButtonPrimary] = QColor(0x1976D2);
        m_colors[ButtonNeutral] = QColor(0xd4d6d8);
        m_colors[ButtonPositive] = QColor(0xE0FBE7);
        m_colors[ButtonNegative] = QColor(0xE6543D);
        m_colors[ButtonTextPrimary] = QColor("white");
        m_colors[ButtonTextNeutral] = QColor(0x474F5B);
        m_colors[ButtonTextPositive] = QColor(0x474F5B);
        m_colors[ButtonTextNegative] = QColor("white");
        m_colors[ButtonPrimaryHover] = QColor(0x1976D2).lighter(120);
        m_colors[ButtonNeutralHover] = QColor(0xE0E3E8);
        m_colors[ButtonPositiveHover] = QColor(0xE0FBE7).lighter(120);
        m_colors[ButtonNegativeHover] = QColor(0xE6543D).lighter(120);
        m_colors[ButtonPrimaryClicked] = QColor(0x1976D2).darker(120);
        m_colors[ButtonNeutralClicked] = QColor(0xd4d6d8).darker(120);
        m_colors[ButtonPositiveClicked] = QColor(0xE0FBE7).darker(120);
        m_colors[ButtonNegativeClicked] = QColor(0xE6543D).darker(120);
        m_colors[TextPrimary] = QColor(0x26282a);
        m_colors[TextSecondary] = QColor(0x637381);
        m_colors[TextDisabled] = QColor(0x919eab);
        m_colors[TextSelected] = QColor("white");
        m_colors[Link] = QColor(0x1976D2);
        m_colors[Icon] = QColor(0x26282a);
        m_colors[IconInactive] = QColor(0x637381);
        m_colors[IconDisabled] = QColor(0x919eab);
        m_colors[InputBackground] = QColor("white");
        m_colors[ControlBackground] = QColor(0xd9d9d9);
        m_colors[ControlBorder] = QColor(0x999999);
        m_colors[NoSignal] = QColor("white");
        m_colors[LowSignal] = QColor(0xff4b4b);
        m_colors[MidSignal] = QColor(0xffb527);
        m_colors[GoodSignal] = QColor(0x5bc214);
        m_colors[EpgCurrentProgColor] = QColor(0x1976D2).lighter(220);
        m_colors[EpgCurrentProgProgressColor] = QColor(0x1976D2).lighter(200);
        m_colors[EmptyLogoColor] = QColor("white");
    }
    emit colorsChanged();
}

void ApplicationUI::setCurrentView(int newCurrentView)
{
    if (m_currentView == newCurrentView)
    {
        return;
    }
    m_currentView = newCurrentView;
    emit currentViewChanged();

    setIsPortraitView(m_currentView != ApplicationUI::LandscapeView);
}

void ApplicationUI::setIsPortraitView(bool portraitView)
{
    if (m_isPortraitView == portraitView)
    {
        return;
    }
    m_isPortraitView = portraitView;
    emit isPortraitViewChanged();
}
