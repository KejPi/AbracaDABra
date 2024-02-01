/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QString>
#include <QFileDialog>
#include <QDir>
#include <QMap>
#include <QDebug>
#include <QLoggingCategory>
#include <QGraphicsScene>
#include <QToolButton>
#include <QFormLayout>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QStyleFactory>
#include <QtGlobal>
#include <QStyleHints>
#include <QClipboard>
#include <QToolTip>
#include <QActionGroup>
#include <QStandardPaths>
#include <QtGlobal>
#include <iostream>

#include "mainwindow.h"
#include "slsview.h"
#include "./ui_mainwindow.h"
#include "dabtables.h"
#include "epgtime.h"
#include "radiocontrol.h"
#include "bandscandialog.h"
#include "config.h"
#include "aboutdialog.h"
#include "audiooutputqt.h"
#if HAVE_PORTAUDIO
#include "audiooutputpa.h"
#endif
#include "metadatamanager.h"


// Input devices
#include "rawfileinput.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#if HAVE_AIRSPY
#include "airspyinput.h"
#endif
#if HAVE_SOAPYSDR
#include "soapysdrinput.h"
#endif

#ifdef Q_OS_MACOS
#include "mac.h"
#endif

Q_LOGGING_CATEGORY(application, "Application", QtInfoMsg)

const QString MainWindow::appName("AbracaDABra");
const char * MainWindow::syncLevelLabels[] = {QT_TR_NOOP("No signal"), QT_TR_NOOP("Signal found"), QT_TR_NOOP("Sync")};
const char * MainWindow::syncLevelTooltip[] = {QT_TR_NOOP("DAB signal not detected<br>Looking for signal..."),
                                               QT_TR_NOOP("Found DAB signal,<br>trying to synchronize..."),
                                               QT_TR_NOOP("Synchronized to DAB signal")};
const QStringList MainWindow::snrProgressStylesheet = {
    QString::fromUtf8("QProgressBar::chunk {background-color: #ff4b4b; }"),  // red
    QString::fromUtf8("QProgressBar::chunk {background-color: #ffb527; }"),  // yellow
    QString::fromUtf8("QProgressBar::chunk {background-color: #5bc214; }")   // green
};

enum class SNR10Threhold
{
    SNR_BAD = 70,
    SNR_GOOD = 100
};

static LogModel * logModel;

// this is default log handler printing the sam format to log windows and to stderr
void logToModelHandlerDefault(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timeStamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    QString txt;
    switch (type) {
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

    QMetaObject::invokeMethod(logModel, "appendRow", Qt::QueuedConnection,
                              Q_ARG(QString, txt),
                              Q_ARG(int, type));

    std::cerr << txt.toStdString() << std::endl;
}

// this is custom log handler printing default format to log window and custom format to stderr
void logToModelHandlerCustom(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timeStamp = QTime::currentTime().toString("HH:mm:ss.zzz");
    QString txt;

    std::cerr << qFormatLogMessage(type, context, msg).toStdString() << std::endl;

    switch (type) {
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

    QMetaObject::invokeMethod(logModel, "appendRow", Qt::QueuedConnection,
                              Q_ARG(QString, txt),
                              Q_ARG(int, type));
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

MainWindow::MainWindow(const QString &iniFilename, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_iniFilename(iniFilename)
{
    initStyle(); // init style as soon as possible

    m_dlDecoder[Instance::Service] = new DLDecoder();
    m_dlDecoder[Instance::Announcement] = new DLDecoder();

    ui->setupUi(this);

    // creating log windows as soon as possible
    m_logDialog = new LogDialog(this);
    setLogToModel(m_logDialog->getModel());

    ui->serviceListView->setIconSize(QSize(16,16));

    // set UI
    setWindowTitle("Abraca DAB Radio");

#ifdef Q_OS_WIN
    // this is Windows specific code, not portable - allows to bring window to front (used for alarm announcement)
    // does not exit on Qt6 yet
    // workaround: https://forum.qt.io/topic/133694/using-alwaysactivatewindow-to-gain-foreground-in-win10-using-qt6-2/3
    //QWindowsWindowFunctions::setWindowActivationBehavior(QWindowsWindowFunctions::AlwaysActivateWindow);
#endif

    ui->channelDown->setText(QString::fromUtf8("\u2039"));
    ui->channelUp->setText(QString::fromUtf8("\u203A"));

    connect(ui->channelDown, &ClickableLabel::clicked, this, &MainWindow::onChannelDownClicked);
    connect(ui->channelUp, &ClickableLabel::clicked, this, &MainWindow::onChannelUpClicked);

    // favorites control
    ui->favoriteLabel->setCheckable(true);
    ui->favoriteLabel->setTooltip(tr("Add service to favorites"), false);
    ui->favoriteLabel->setTooltip(tr("Remove service from favorites"), true);
    ui->favoriteLabel->setChecked(false);

    m_inputDeviceRecorder = new InputDeviceRecorder();

    m_setupDialog = new SetupDialog(this);
    connect(m_setupDialog, &SetupDialog::inputDeviceChanged, this, &MainWindow::changeInputDevice);
    connect(this, &MainWindow::expertModeChanged, m_setupDialog, &SetupDialog::onExpertMode);
    connect(m_setupDialog, &SetupDialog::newInputDeviceSettings, this, &MainWindow::onNewInputDeviceSettings);
    connect(m_setupDialog, &SetupDialog::applicationStyleChanged, this, &MainWindow::onApplicationStyleChanged);
    connect(m_setupDialog, &SetupDialog::expertModeToggled, this, &MainWindow::onExpertModeToggled);
    connect(m_setupDialog, &SetupDialog::newAnnouncementSettings, this, &MainWindow::onNewAnnouncementSettings);
    connect(m_setupDialog, &SetupDialog::xmlHeaderToggled, m_inputDeviceRecorder, &InputDeviceRecorder::setXmlHeaderEnabled);

    m_ensembleInfoDialog = new EnsembleInfoDialog(this);
    connect(m_ensembleInfoDialog, &EnsembleInfoDialog::recordingStart, m_inputDeviceRecorder, &InputDeviceRecorder::start);
    connect(m_ensembleInfoDialog, &EnsembleInfoDialog::recordingStop, m_inputDeviceRecorder, &InputDeviceRecorder::stop);
    connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_ensembleInfoDialog, &EnsembleInfoDialog::onRecording);       
    connect(m_inputDeviceRecorder, &InputDeviceRecorder::bytesRecorded, m_ensembleInfoDialog, &EnsembleInfoDialog::updateRecordingStatus, Qt::QueuedConnection);

    // status bar
    QWidget * widget = new QWidget();
    m_timeLabel = new QLabel("");
    m_timeLabel->setToolTip(tr("DAB time"));

    m_basicSignalQualityLabel = new QLabel("");
    m_basicSignalQualityLabel->setToolTip(tr("DAB signal quality"));

    m_infoLabel = new QLabel();
    QFont boldFont;
    boldFont.setBold(true);
    m_infoLabel->setFont(boldFont);

    m_timeBasicQualInfoWidget = new QStackedWidget;
    m_timeBasicQualInfoWidget->addWidget(m_basicSignalQualityLabel);
    m_timeBasicQualInfoWidget->addWidget(m_timeLabel);
    m_timeBasicQualInfoWidget->addWidget(m_infoLabel);

    m_audioRecordingLabel = new ClickableLabel(this);
    m_audioRecordingLabel->setTooltip(tr("Stop audio recording"));
    connect(m_audioRecordingLabel, &ClickableLabel::clicked, this, &MainWindow::audioRecordingToggle);
    m_audioRecordingProgressLabel = new QLabel();
    // m_audioRecordingProgressLabel->setFont(boldFont);   // bold font
    m_audioRecordingProgressLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout * audioRecordingLayout = new QHBoxLayout();
    audioRecordingLayout->addWidget(m_audioRecordingLabel);
    audioRecordingLayout->addWidget(m_audioRecordingProgressLabel);
    audioRecordingLayout->setStretch(1, 100);
    audioRecordingLayout->setSpacing(5);
    audioRecordingLayout->setContentsMargins(0,0,0,0);
    audioRecordingLayout->setAlignment(m_audioRecordingLabel, Qt::AlignCenter);
    audioRecordingLayout->setAlignment(m_audioRecordingProgressLabel, Qt::AlignLeft | Qt::AlignHCenter);
    m_audioRecordingWidget = new QWidget(this);
    m_audioRecordingWidget->setLayout(audioRecordingLayout);
    m_audioRecordingWidget->setVisible(false);
    m_audioRecordingActive = false;

    m_syncLabel = new QLabel();
    m_syncLabel->setAlignment(Qt::AlignRight);

    m_snrProgressbar = new QProgressBar();
    m_snrProgressbar->setMaximum(30*10);
    m_snrProgressbar->setMinimum(0);
    m_snrProgressbar->setFixedWidth(80);
    m_snrProgressbar->setFixedHeight(m_syncLabel->fontInfo().pixelSize());
    m_snrProgressbar->setTextVisible(false);
    m_snrProgressbar->setToolTip(QString(tr("DAB signal SNR")));

    m_snrLabel = new QLabel();
    int width = m_snrLabel->fontMetrics().boundingRect("100.0 dB").width();
    m_snrLabel->setFixedWidth(width);
    m_snrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_snrLabel->setToolTip(QString(tr("DAB signal SNR")));

    QHBoxLayout * signalQualityLayout = new QHBoxLayout();
    signalQualityLayout->addWidget(m_syncLabel);
    signalQualityLayout->addWidget(m_snrProgressbar);
    signalQualityLayout->setAlignment(m_snrProgressbar, Qt::AlignCenter);
    signalQualityLayout->setStretch(0, 100);
    signalQualityLayout->setAlignment(m_syncLabel, Qt::AlignCenter);
    signalQualityLayout->addWidget(m_snrLabel);
    signalQualityLayout->setSpacing(10);
#ifdef Q_OS_MAC
    signalQualityLayout->setContentsMargins(0,2,0,0);
#else
    signalQualityLayout->setContentsMargins(0,0,0,0);
#endif
    m_signalQualityWidget = new QWidget();
    m_signalQualityWidget->setLayout(signalQualityLayout);

    m_setupAction = new QAction(tr("Settings..."), this);
    connect(m_setupAction, &QAction::triggered, this, &MainWindow::showSetupDialog);

    m_clearServiceListAction = new QAction(tr("Clear service list"), this);
    connect(m_clearServiceListAction, &QAction::triggered, this, &MainWindow::clearServiceList);

    m_bandScanAction = new QAction(tr("Band scan..."), this);
    connect(m_bandScanAction, &QAction::triggered, this, &MainWindow::bandScan);

    m_ensembleInfoAction = new QAction(tr("Ensemble information"), this);
    connect(m_ensembleInfoAction, &QAction::triggered, this, &MainWindow::showEnsembleInfo);

    m_audioRecordingAction = new QAction(tr("Start audio recording"), this);
    connect(m_audioRecordingAction, &QAction::triggered, this, &MainWindow::audioRecordingToggle);

    m_epgAction = new QAction(tr("Program guide..."), this);
    m_epgAction->setEnabled(false);
    connect(m_epgAction, &QAction::triggered, this, &MainWindow::showEPG);    

    m_logAction = new QAction(tr("Application log"), this);
    connect(m_logAction, &QAction::triggered, this, &MainWindow::showLog);

    m_aboutAction = new QAction(tr("About"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    // load audio framework setting from ini file
    AudioFramework audioFramework = getAudioFramework();

    m_menu = new QMenu(this);

#ifdef Q_OS_LINUX
    if (AudioFramework::Qt == audioFramework)
#endif
    {
        m_audioOutputMenu = m_menu->addMenu(tr("Audio output"));
        connect(m_audioOutputMenu, &QMenu::triggered, this, &MainWindow::onAudioOutputSelected);
    }
    m_menu->addAction(m_audioRecordingAction);
    m_menu->addSeparator();
    m_menu->addAction(m_setupAction);   
    m_menu->addAction(m_bandScanAction);
    m_menu->addAction(m_clearServiceListAction);
    m_menu->addSeparator();
    m_menu->addAction(m_epgAction);
    m_menu->addAction(m_ensembleInfoAction);
    m_menu->addAction(m_logAction);
    m_menu->addAction(m_aboutAction);

    onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);

    m_menuLabel = new ClickableLabel(this);
    m_menuLabel->setToolTip(tr("Open menu"));
    m_menuLabel->setMenu(m_menu);

    m_muteLabel = new ClickableLabel(this);
    m_muteLabel->setCheckable(true);
    m_muteLabel->setTooltip(tr("Mute audio"), false);
    m_muteLabel->setTooltip(tr("Unmute audio"), true);
    m_muteLabel->setChecked(false);

    m_audioVolumeSlider = new QSlider(Qt::Horizontal, this);
    m_audioVolumeSlider->setMinimum(0);
    m_audioVolumeSlider->setMaximum(100);
    m_audioVolumeSlider->setSingleStep(10);
    m_audioVolumeSlider->setToolTip(tr("Audio volume"));
#ifdef Q_OS_WIN
    m_audioVolumeSlider->setMaximumHeight(15);
#endif
    connect(m_audioVolumeSlider, &QSlider::valueChanged, this, &MainWindow::onAudioVolumeSliderChanged);
    connect(m_muteLabel, &ClickableLabel::toggled, this, &MainWindow::onMuteLabelToggled);


    QHBoxLayout * volumeLayout = new QHBoxLayout();
    volumeLayout->addWidget(m_muteLabel);
    volumeLayout->addWidget(m_audioVolumeSlider);
    volumeLayout->setAlignment(m_audioVolumeSlider, Qt::AlignCenter);
    volumeLayout->setStretch(0, 100);
    volumeLayout->setAlignment(m_muteLabel, Qt::AlignCenter);
    volumeLayout->setSpacing(10);
    volumeLayout->setContentsMargins(10,0,0,0);
    QWidget * volumeWidget = new QWidget();
    volumeWidget->setLayout(volumeLayout);

    QGridLayout * layout = new QGridLayout(widget);
    layout->addWidget(m_timeBasicQualInfoWidget, 0, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(m_audioRecordingWidget, 0, 1, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(m_signalQualityWidget, 0, 2, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(volumeWidget, 0, 3, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(m_menuLabel, 0, 4, Qt::AlignVCenter | Qt::AlignRight);
    layout->setColumnStretch(1, 100);
    layout->setSpacing(20);
    ui->statusbar->addWidget(widget,1);

    // set fonts
    QFont f;
    f.setPointSize(qRound(1.5 * ui->programTypeLabel->fontInfo().pointSize()));
    ui->ensembleLabel->setFont(f);
    f.setBold(true);
    ui->serviceLabel->setFont(f);

    // service list
    m_serviceList = new ServiceList;

    // metadata
    m_metadataManager = new MetadataManager(m_serviceList, this);
    connect(m_metadataManager, &MetadataManager::dataUpdated, this, &MainWindow::onMetadataUpdated);
    connect(m_serviceList, &ServiceList::serviceAddedToEnsemble, m_metadataManager, &MetadataManager::addServiceEpg);
    connect(m_serviceList, &ServiceList::serviceRemoved, m_metadataManager, &MetadataManager::removeServiceEpg);
    connect(m_serviceList, &ServiceList::empty, m_metadataManager, &MetadataManager::clearEpg);
    connect(EPGTime::getInstance(), &EPGTime::haveValidTime, m_metadataManager, &MetadataManager::getEpgData);

    m_slModel = new SLModel(m_serviceList, m_metadataManager, this);
    connect(m_serviceList, &ServiceList::serviceAdded, m_slModel, &SLModel::addService);
    connect(m_serviceList, &ServiceList::serviceUpdated, m_slModel, &SLModel::updateService);
    connect(m_serviceList, &ServiceList::serviceRemoved, m_slModel, &SLModel::removeService);
    connect(m_serviceList, &ServiceList::empty, m_slModel, &SLModel::clear);

    ui->serviceListView->setModel(m_slModel);
    ui->serviceListView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceListView->setEditTriggers(QAbstractItemView::NoEditTriggers);    
    ui->serviceListView->installEventFilter(this);
    connect(ui->serviceListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onServiceListSelection);

    m_slTreeModel = new SLTreeModel(m_serviceList, m_metadataManager, this);
    connect(m_serviceList, &ServiceList::serviceAddedToEnsemble, m_slTreeModel, &SLTreeModel::addEnsembleService);
    connect(m_serviceList, &ServiceList::serviceUpdatedInEnsemble, m_slTreeModel, &SLTreeModel::updateEnsembleService);
    connect(m_serviceList, &ServiceList::serviceRemovedFromEnsemble, m_slTreeModel, &SLTreeModel::removeEnsembleService);
    connect(m_serviceList, &ServiceList::ensembleRemoved, m_slTreeModel, &SLTreeModel::removeEnsemble);
    ui->serviceTreeView->setModel(m_slTreeModel);
    ui->serviceTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceTreeView->setHeaderHidden(true);
    ui->serviceTreeView->installEventFilter(this);
    connect(ui->serviceTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onServiceListTreeSelection);
    connect(m_serviceList, &ServiceList::empty, m_slTreeModel, &SLTreeModel::clear);

    // EPG dialog
    m_epgDialog = new EPGDialog(m_slModel, ui->serviceListView->selectionModel(), m_metadataManager, this);
    connect(m_metadataManager, &MetadataManager::epgAvailable, this, [this](){ m_epgAction->setEnabled(true); } );
    connect(m_metadataManager, &MetadataManager::epgEmpty, this, &MainWindow::onEpgEmpty);

    // fill channel list
    int freqLabelMaxWidth = 0;
    dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
    while (it != DabTables::channelList.constEnd()) {
        // insert to combo
        ui->channelCombo->addItem(it.value(), it.key());

        // calculate label size
        QString freqStr = QString("%1 MHz").arg(it.key()/1000.0, 3, 'f', 3, QChar('0'));
        if (freqLabelMaxWidth < ui->frequencyLabel->fontMetrics().boundingRect(freqStr).width())
        {
            freqLabelMaxWidth = ui->frequencyLabel->fontMetrics().boundingRect(freqStr).width();
        }
        ++it;
    }
    connect(ui->channelCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onChannelChange);
    ui->channelCombo->setCurrentIndex(-1);
    ui->channelCombo->setDisabled(true);
    ui->channelDown->setDisabled(true);
    ui->channelUp->setDisabled(true);

    ui->frequencyLabel->setFixedWidth(freqLabelMaxWidth);

    // disable service list - it is enabled when some valid device is selected1
    ui->serviceListView->setEnabled(false);
    ui->serviceTreeView->setEnabled(false);
    ui->favoriteLabel->setEnabled(false);

    m_audioRecordingAction->setEnabled(false);
    onAudioRecordingStopped();

    clearEnsembleInformationLabels();
    clearServiceInformationLabels();    
    ui->dynamicLabel_Service->clear();
    ui->dynamicLabel_Announcement->clear();
    ui->dlWidget->setCurrentIndex(Instance::Service);
    ui->dlPlusWidget->setCurrentIndex(Instance::Service);
    ui->dlPlusWidget->setVisible(false);

    // text copying
    ui->dynamicLabel_Service->installEventFilter(this);
    ui->dlPlusFrame_Service->installEventFilter(this);
    ui->dynamicLabel_Announcement->installEventFilter(this);
    ui->dlPlusFrame_Announcement->installEventFilter(this);

    ui->audioEncodingLabel->setToolTip(tr("Audio coding"));

    ui->slsView_Service->setMinimumSize(QSize(322, 242));
    ui->slsView_Announcement->setMinimumSize(QSize(322, 242));
    ui->slsView_Service->reset();
    ui->slsView_Announcement->reset();
    ui->slsWidget->setCurrentIndex(Instance::Service);

    ui->announcementLabel->setToolTip(tr("Ongoing announcement"));
    ui->announcementLabel->setHidden(true);
    ui->announcementLabel->setCheckable(true);

    ui->catSlsLabel->setToolTip(tr("Browse categorized slides"));
    ui->catSlsLabel->setHidden(true);

    ui->switchSourceLabel->setToolTip(tr("Change service source (ensemble)"));
    ui->switchSourceLabel->setHidden(true);

    ui->logoLabel->setHidden(true);

    ui->serviceTreeView->setVisible(false);

    setupDarkMode();

    // focus polisy
    ui->channelCombo->setFocusPolicy(Qt::StrongFocus);
    ui->scrollArea->setFocusPolicy(Qt::ClickFocus);

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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        qCFatal(application) << "RadioControl() init failed";
#else
        qCCritical(application) << "RadioControl() init failed";
#endif
        ::exit(1);
    }

    m_audioRecorder = new AudioRecorder();
    m_audioDecoder = new AudioDecoder(m_audioRecorder);
    m_audioDecoderThread = new QThread(this);
    m_audioDecoderThread->setObjectName("audioDecoderThr");
    m_audioDecoder->moveToThread(m_audioDecoderThread);
    m_audioRecorder->moveToThread(m_audioDecoderThread);
    connect(m_audioDecoderThread, &QThread::finished, m_audioDecoder, &QObject::deleteLater);
    connect(m_audioDecoderThread, &QThread::finished, m_audioRecorder, &QObject::deleteLater);
    m_audioDecoderThread->start();

    connect(m_setupDialog, &SetupDialog::noiseConcealmentLevelChanged, m_audioDecoder, &AudioDecoder::setNoiseConcealment, Qt::QueuedConnection);
    connect(this, &MainWindow::audioStop, m_audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
    connect(this, &MainWindow::startAudioRecording, m_audioRecorder, &AudioRecorder::start, Qt::QueuedConnection);
    connect(this, &MainWindow::stopAudioRecording, m_audioRecorder, &AudioRecorder::stop, Qt::QueuedConnection);
    connect(m_audioRecorder, &AudioRecorder::recordingStarted, this, &MainWindow::onAudioRecordingStarted, Qt::QueuedConnection);
    connect(m_audioRecorder, &AudioRecorder::recordingStopped, this, &MainWindow::onAudioRecordingStopped, Qt::QueuedConnection);
    connect(m_audioRecorder, &AudioRecorder::recordingProgress, this, &MainWindow::onAudioRecordingProgress, Qt::QueuedConnection);
    connect(m_setupDialog, &SetupDialog::audioRecordingSettings, m_audioRecorder, &AudioRecorder::setup, Qt::QueuedConnection);

#if (HAVE_PORTAUDIO)
    if (AudioFramework::Pa == audioFramework)
    {
        m_audioOutput = new AudioOutputPa();
#ifndef Q_OS_LINUX
        connect(m_audioOutput, &AudioOutput::audioDevicesList, this, &MainWindow::onAudioDevicesList);
        connect(m_audioOutput, &AudioOutput::audioDeviceChanged, this, &MainWindow::onAudioDeviceChanged);
        connect(this, &MainWindow::audioOutput, m_audioOutput, &AudioOutput::setAudioDevice);
        onAudioDevicesList(m_audioOutput->getAudioDevices());

        qCInfo(application) << "Using PortAudio output";
#endif
    }
    else
#endif
    {
        m_audioOutput = new AudioOutputQt();
        connect(m_audioOutput, &AudioOutput::audioDevicesList, this, &MainWindow::onAudioDevicesList);
        connect(m_audioOutput, &AudioOutput::audioDeviceChanged, this, &MainWindow::onAudioDeviceChanged);
        connect(static_cast<AudioOutputQt *>(m_audioOutput), &AudioOutputQt::audioOutputError, this, &MainWindow::onAudioOutputError, Qt::QueuedConnection);
        connect(this, &MainWindow::audioOutput, m_audioOutput, &AudioOutput::setAudioDevice, Qt::QueuedConnection);
        onAudioDevicesList(m_audioOutput->getAudioDevices());
        m_audioOutputThread = new QThread(this);
        m_audioOutputThread->setObjectName("audioOutThr");
        m_audioOutput->moveToThread(m_audioOutputThread);
        connect(m_audioOutputThread, &QThread::finished, m_audioOutput, &QObject::deleteLater);
        m_audioOutputThread->start();

        qCInfo(application) << "Using Qt audio output";
    }
    connect(this, &MainWindow::audioVolume, m_audioOutput, &AudioOutput::setVolume, Qt::QueuedConnection);
    connect(this, &MainWindow::audioMute, m_audioOutput, &AudioOutput::mute, Qt::QueuedConnection);

    // Connect signals
    connect(m_muteLabel, &ClickableLabel::toggled, m_audioOutput, &AudioOutput::mute, Qt::QueuedConnection);
    connect(ui->favoriteLabel, &ClickableLabel::toggled, this, &MainWindow::onFavoriteToggled);
    connect(ui->switchSourceLabel, &ClickableLabel::clicked, this, &MainWindow::onSwitchSourceClicked);
    connect(ui->catSlsLabel, &ClickableLabel::clicked, this, &MainWindow::showCatSLS);
    connect(ui->announcementLabel, &ClickableLabel::toggled, this, &MainWindow::onAnnouncementClicked);

    connect(m_radioControl, &RadioControl::ensembleInformation, this, &MainWindow::onEnsembleInfo, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleReconfiguration, this, &MainWindow::onEnsembleReconfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, this, &MainWindow::onServiceListComplete, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::signalState, this, &MainWindow::onSignalState, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dabTime, this, &MainWindow::onDabTime, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListEntry, this, &MainWindow::onServiceListEntry, Qt::BlockingQueuedConnection);
    connect(m_radioControl, &RadioControl::announcement, this, &MainWindow::onAnnouncement, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::programmeTypeChanged, this, &MainWindow::onProgrammeTypeChanged, Qt::QueuedConnection);    
    connect(this, &MainWindow::announcementMask, m_radioControl, &RadioControl::setupAnnouncements, Qt::QueuedConnection);
    connect(m_audioOutput, &AudioOutput::audioOutputRestart, m_radioControl, &RadioControl::onAudioOutputRestart, Qt::QueuedConnection);
    connect(this, &MainWindow::toggleAnnouncement, m_radioControl, &RadioControl::suspendResumeAnnouncement, Qt::QueuedConnection);

    connect(m_ensembleInfoDialog, &EnsembleInfoDialog::requestEnsembleConfiguration, m_radioControl, &RadioControl::getEnsembleConfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::signalState, m_ensembleInfoDialog, &EnsembleInfoDialog::updateSnr, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::freqOffset, m_ensembleInfoDialog, &EnsembleInfoDialog::updateFreqOffset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleConfiguration, m_ensembleInfoDialog, &EnsembleInfoDialog::refreshEnsembleConfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, m_ensembleInfoDialog, &EnsembleInfoDialog::newFrequency, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, m_inputDeviceRecorder, &InputDeviceRecorder::setCurrentFrequency, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::fibCounter, m_ensembleInfoDialog, &EnsembleInfoDialog::updateFIBstatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::mscCounter, m_ensembleInfoDialog, &EnsembleInfoDialog::updateMSCstatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_ensembleInfoDialog, &EnsembleInfoDialog::serviceChanged, Qt::QueuedConnection);

    connect(m_radioControl, &RadioControl::dlDataGroup_Service, m_dlDecoder[Instance::Service], &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dlDataGroup_Announcement, m_dlDecoder[Instance::Announcement], &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, this, &MainWindow::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_dlDecoder[Instance::Service], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_dlDecoder[Instance::Announcement], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioData, m_audioDecoder, &AudioDecoder::decodeData, Qt::QueuedConnection);

    // service stopped
    connect(m_radioControl, &RadioControl::ensembleRemoved, this, &MainWindow::onEnsembleRemoved, Qt::QueuedConnection);

    // reconfiguration
    connect(m_radioControl, &RadioControl::audioServiceReconfiguration, this, &MainWindow::onAudioServiceReconfiguration, Qt::QueuedConnection);
    connect(this, &MainWindow::getAudioInfo, m_audioDecoder, &AudioDecoder::getAudioParameters, Qt::QueuedConnection);

    // DL(+)
    // normal service
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlComplete, this, &MainWindow::onDLComplete_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlPlusObject, this, &MainWindow::onDLPlusObjReceived_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlItemToggle, this, &MainWindow::onDLPlusItemToggle_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::dlItemRunning, this, &MainWindow::onDLPlusItemRunning_Service);
    connect(m_dlDecoder[Instance::Service], &DLDecoder::resetTerminal, this, &MainWindow::onDLReset_Service);
    // announcement
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlComplete, this, &MainWindow::onDLComplete_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlPlusObject, this, &MainWindow::onDLPlusObjReceived_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlItemToggle, this, &MainWindow::onDLPlusItemToggle_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::dlItemRunning, this, &MainWindow::onDLPlusItemRunning_Announcement);
    connect(m_dlDecoder[Instance::Announcement], &DLDecoder::resetTerminal, this, &MainWindow::onDLReset_Announcement);

    connect(m_audioDecoder, &AudioDecoder::audioParametersInfo, this, &MainWindow::onAudioParametersInfo, Qt::QueuedConnection);
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
    connect(this, &MainWindow::serviceRequest, m_radioControl, &RadioControl::tuneService, Qt::QueuedConnection);

    // these two signals have to be connected in initInputDevice() - left here as comment
    // connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
    // connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, this, &MainWindow::onTuneDone, Qt::QueuedConnection);

    connect(this, &MainWindow::exit, m_radioControl, &RadioControl::exit, Qt::QueuedConnection);

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

    //connect(this, &MainWindow::serviceRequest, m_metadataManager, &MetadataManager::onServiceRequest);

    connect(m_slideShowApp[Instance::Service], &SlideShowApp::currentSlide, ui->slsView_Service, &SLSView::showSlide, Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::resetTerminal, ui->slsView_Service, &SLSView::reset, Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::catSlsAvailable, ui->catSlsLabel, &ClickableLabel::setVisible, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, m_slideShowApp[Instance::Service], &SlideShowApp::stop, Qt::QueuedConnection);

    connect(m_radioControlThread, &QThread::finished, m_slideShowApp[Instance::Announcement], &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp[Instance::Announcement], &SlideShowApp::start);
    connect(m_radioControl, &RadioControl::userAppData_Announcement, m_slideShowApp[Instance::Announcement], &SlideShowApp::onUserAppData);
    connect(m_slideShowApp[Instance::Announcement], &SlideShowApp::currentSlide, ui->slsView_Announcement, &SLSView::showSlide, Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Announcement], &SlideShowApp::resetTerminal, ui->slsView_Announcement, &SLSView::reset, Qt::QueuedConnection);
    //connect(m_radioControl, &RadioControl::announcement, ui->slsView_Service, &SLSView::showAnnouncement, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::announcement, ui->slsView_Announcement, &SLSView::showAnnouncement, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, m_slideShowApp[Instance::Announcement], &SlideShowApp::stop, Qt::QueuedConnection);


    m_catSlsDialog = new CatSLSDialog(this);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::categoryUpdate, m_catSlsDialog, &CatSLSDialog::onCategoryUpdate, Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::catSlide, m_catSlsDialog, &CatSLSDialog::onCatSlide, Qt::QueuedConnection);
    connect(m_slideShowApp[Instance::Service], &SlideShowApp::resetTerminal, m_catSlsDialog, &CatSLSDialog::reset, Qt::QueuedConnection);
    connect(m_catSlsDialog, &CatSLSDialog::getCurrentCatSlide, m_slideShowApp[Instance::Service], &SlideShowApp::getCurrentCatSlide, Qt::QueuedConnection);
    connect(m_catSlsDialog, &CatSLSDialog::getNextCatSlide, m_slideShowApp[Instance::Service], &SlideShowApp::getNextCatSlide, Qt::QueuedConnection);

    m_spiApp = new SPIApp();
    m_spiApp->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_spiApp, &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::userAppData_Service, m_spiApp, &SPIApp::onUserAppData);
    connect(m_radioControl, &RadioControl::audioServiceSelection,  m_spiApp, &SPIApp::start);
    connect(this, &MainWindow::stopUserApps, m_spiApp, &SPIApp::stop, Qt::QueuedConnection);
    connect(this, &MainWindow::resetUserApps, m_spiApp, &SPIApp::reset, Qt::QueuedConnection);

    connect(m_spiApp, &SPIApp::xmlDocument, m_metadataManager, &MetadataManager::processXML, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getSI, m_spiApp, &SPIApp::getSI, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getPI, m_spiApp, &SPIApp::getPI, Qt::QueuedConnection);
    connect(m_metadataManager, &MetadataManager::getFile, m_spiApp, &SPIApp::onFileRequest, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::requestedFile, m_metadataManager, &MetadataManager::onFileReceived, Qt::QueuedConnection);
    connect(m_spiApp, &SPIApp::radioDNSAvailable, m_metadataManager, &MetadataManager::getEpgData);
    connect(m_setupDialog, &SetupDialog::spiApplicationEnabled, m_radioControl, &RadioControl::onSpiApplicationEnabled, Qt::QueuedConnection);
    connect(m_setupDialog, &SetupDialog::spiApplicationEnabled, m_spiApp, &SPIApp::enable, Qt::QueuedConnection);
    connect(m_setupDialog, &SetupDialog::spiApplicationSettingsChanged, m_spiApp, &SPIApp::onSettingsChanged, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_metadataManager, &MetadataManager::onEnsembleInformation, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_metadataManager, &MetadataManager::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, m_epgDialog, &EPGDialog::onEnsembleInformation, Qt::QueuedConnection);    

    // input device connections
    initInputDevice(InputDeviceId::UNDEFINED);

    loadSettings();

    // setting focus to something harmless that does not do eny visual effects
    m_menuLabel->setFocus();

    // this causes focus to be set to service list when tune is finished
    m_hasListViewFocus = true;
    m_hasTreeViewFocus = false;

    // //QFile xmlfile("/Users/kejpi/Devel/AbracaDABra/20231206_PI.xml");
    // QFile xmlfile("/Users/kejpi/Devel/AbracaDABra/_cache/w20081011dD220c0.EHB.xml");
    // if (xmlfile.open(QIODevice::ReadOnly | QIODevice::Text))
    // {
    //     QTextStream in(&xmlfile);
    //     m_metadataManager->processXML(qPrintable(in.readAll()), "dab:de0.d06c.d220.0");
    //     xmlfile.close();
    // }
}

MainWindow::~MainWindow()
{
    delete m_inputDevice;
    delete m_inputDeviceRecorder;

    m_radioControlThread->quit();  // this deletes radioControl
    m_radioControlThread->wait();
    delete m_radioControlThread;

    m_audioDecoderThread->quit();  // this deletes audiodecoder
    m_audioDecoderThread->wait();
    delete m_audioDecoderThread;

    if (nullptr != m_audioOutputThread)
    {  // Qt audio
        m_audioOutputThread->quit();  // this deletes audiooutput
        m_audioOutputThread->wait();
        delete m_audioOutputThread;
    }
    else
    {   // PortAudio
        delete m_audioOutput;
    }

    delete m_dlDecoder[Instance::Service];
    delete m_dlDecoder[Instance::Announcement];
    delete m_serviceList;
    delete m_metadataManager;
    delete ui;
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o == ui->serviceListView)
    {
        if(e->type() == QEvent::KeyPress)
        {
            QKeyEvent * event = static_cast<QKeyEvent *>(e);
            if (Qt::Key_Space == event->key())
            {
                ui->favoriteLabel->toggle();
                return true;
            }
        }
        return QObject::eventFilter(o, e);
    }
    if ((o == ui->dynamicLabel_Service) || (o == ui->dynamicLabel_Announcement))
    {
        if (e->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent * event = static_cast<QMouseEvent *>(e);
            if (Qt::LeftButton == event->button())
            {
                QLabel * label = static_cast<QLabel *>(o);
                QGuiApplication::clipboard()->setText(label->text());
                QToolTip::showText(label->mapToGlobal(event->pos()), tr("<i>DL text copied to clipboard</i>"));
                return true;
            }
        }
        return QObject::eventFilter(o, e);
    }
    if ((o == ui->dlPlusFrame_Service) || (o == ui->dlPlusFrame_Announcement))
    {
        if (e->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent * event = static_cast<QMouseEvent *>(e);
            if (Qt::LeftButton == event->button())
            {   // create text representation
                QMap<DLPlusContentType, DLPlusObjectUI*> * dlObjCachePtr = &m_dlObjCache[Instance::Service];
                if (o == ui->dlPlusFrame_Announcement)
                {
                    dlObjCachePtr = &m_dlObjCache[Instance::Announcement];
                }
                QString text;
                for (auto & obj : *dlObjCachePtr)
                {
                    text += QString("%1: %2\n").arg(obj->getLabel(obj->getDlPlusObject().getType()), obj->getDlPlusObject().getTag());
                }
                QGuiApplication::clipboard()->setText(text);
                QToolTip::showText(ui->dlPlusFrame_Service->mapToGlobal(event->pos()), tr("<i>DL+ text copied to clipboard</i>"));
                return true;
            }
        }
        return QObject::eventFilter(o, e);
    }
    return QObject::eventFilter(o, e);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (0 == m_frequency)
    {   // in idle
        saveSettings();

        emit exit();
        event->accept();
    }
    else
    {   // message is needed for Windows -> stopping RTL SDR thread may take long time :-(
        m_infoLabel->setText(tr("Stopping DAB processing, please wait..."));
        m_infoLabel->setToolTip("");
        m_timeBasicQualInfoWidget->setCurrentWidget(m_infoLabel);

        m_exitRequested = true;
        emit resetUserApps();
        emit serviceRequest(0,0,0);
        event->ignore();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (ui->dynamicLabel_Service->width() < ui->dynamicLabel_Service->fontMetrics().boundingRect(ui->dynamicLabel_Service->text()).width())
    {
        ui->dynamicLabel_Service->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        ui->dynamicLabel_Service->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    }
    if (ui->dynamicLabel_Announcement->width() < ui->dynamicLabel_Announcement->fontMetrics().boundingRect(ui->dynamicLabel_Announcement->text()).width())
    {
        ui->dynamicLabel_Announcement->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        ui->dynamicLabel_Announcement->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    }

    QMainWindow::resizeEvent(event);
}

void MainWindow::onInputDeviceReady()
{
    ui->channelCombo->setEnabled(true);
    ui->channelDown->setEnabled(true);
    ui->channelUp->setEnabled(true);
    restoreTimeQualWidget();
}

void MainWindow::onEnsembleInfo(const RadioControlEnsemble &ens)
{
    ui->ensembleLabel->setText(ens.label);
    ui->ensembleLabel->setToolTip(QString(tr("<b>Ensemble:</b> %1<br>"
                                          "<b>Short label:</b> %2<br>"
                                          "<b>ECC:</b> 0x%3<br>"
                                          "<b>EID:</b> 0x%4<br>"
                                             "<b>Country:</b> %5"))
                                  .arg(ens.label)
                                  .arg(ens.labelShort)
                                  .arg(QString("%1").arg(ens.ecc(), 2, 16, QChar('0')).toUpper())
                                  .arg(QString("%1").arg(ens.eid(), 4, 16, QChar('0')).toUpper())
                                  .arg(DabTables::getCountryName(ens.ueid)));

    m_serviceList->beginEnsembleUpdate(ens);
}

void MainWindow::onEnsembleReconfiguration(const RadioControlEnsemble &ens) const
{
    m_serviceList->beginEnsembleUpdate(ens);
}

void MainWindow::onServiceListComplete(const RadioControlEnsemble &ens)
{
    m_serviceList->endEnsembleUpdate(ens);

    serviceListViewUpdateSelection();
    serviceTreeViewUpdateSelection();


}

void MainWindow::onEnsembleRemoved(const RadioControlEnsemble &ens)
{
    emit resetUserApps();

    m_dlDecoder[Instance::Service]->reset();
    m_dlDecoder[Instance::Announcement]->reset();

    clearServiceInformationLabels();
    ui->serviceListView->setCurrentIndex(QModelIndex());
    ui->serviceTreeView->setCurrentIndex(QModelIndex());
    ui->favoriteLabel->setEnabled(false);
    m_audioRecordingAction->setDisabled(true);

    m_serviceList->removeEnsemble(ens);        
}

void MainWindow::onSignalState(uint8_t sync, float snr)
{
    if (DabSyncLevel::FullSync > DabSyncLevel(sync))
    {   // hide time when no sync
        m_timeLabel->setText("");

        // set no signal quality when no sync
        if (isDarkMode())
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal0_dark.png"));
        }
        else
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal0.png"));
        }
    }
    m_syncLabel->setText(tr(syncLevelLabels[sync]));
    m_syncLabel->setToolTip(tr(syncLevelTooltip[sync]));


    // limit SNR to progressbar maximum
    int snr10 = qRound(snr*10);
    if (snr10 > m_snrProgressbar->maximum())
    {
        snr10 = m_snrProgressbar->maximum();
    }
    m_snrProgressbar->setValue(snr10);

    m_snrLabel->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
    // progressbar styling -> it does not look good on Apple
    if (static_cast<int>(SNR10Threhold::SNR_BAD) > snr10)
    {   // bad SNR
#ifndef __APPLE__
        m_snrProgressbar->setStyleSheet(snrProgressStylesheet[0]);
#endif
        if (isDarkMode())
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal1_dark.png"));
        }
        else
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal1.png"));
        }
    }
    else if (static_cast<int>(SNR10Threhold::SNR_GOOD) > snr10)
    {   // medium SNR
#ifndef __APPLE__
        m_snrProgressbar->setStyleSheet(snrProgressStylesheet[1]);
#endif
        if (isDarkMode())
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal2_dark.png"));
        }
        else
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal2.png"));
        }
    }
    else
    {   // good SNR
#ifndef __APPLE__
        m_snrProgressbar->setStyleSheet(snrProgressStylesheet[2]);
#endif
        if (isDarkMode())
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal3_dark.png"));
        }
        else
        {
            m_basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal3.png"));
        }
    }
}

void MainWindow::onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry)
{
    if (slEntry.TMId != DabTMId::StreamAudio)
    {  // do nothing - data services not supported
        return;
    }

    // add to service list
    m_serviceList->addService(ens, slEntry);
}


void MainWindow::onDLComplete_Service(const QString & dl)
{
    onDLComplete(dl, ui->dynamicLabel_Service);
}

void MainWindow::onDLComplete_Announcement(const QString & dl)
{
    onDLComplete(dl, ui->dynamicLabel_Announcement);
}

void MainWindow::onDLComplete(const QString & dl, QLabel * dlLabel)
{
    QString label = dl;

    label.replace(QRegularExpression(QString(QChar(0x0A))), "<br/>");
    if (label.indexOf(QChar(0x0B)) >= 0)
    {
        label.prepend("<b>");
        label.replace(label.indexOf(QChar(0x0B)), 1, "</b>");
    }
    label.remove(QChar(0x1F));

    dlLabel->setText(label);
    if (dlLabel->width() < dlLabel->fontMetrics().boundingRect(label).width())
    {
        dlLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        dlLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    }
}


void MainWindow::onDabTime(const QDateTime & d)
{
    m_timeLabel->setText(m_timeLocale.toString(d, QString("dddd, dd.MM.yyyy, hh:mm")));
    EPGTime::getInstance()->onDabTime(d);
}

void MainWindow::onAudioParametersInfo(const struct AudioParameters & params)
{
    switch (params.coding)
    {
    case AudioCoding::MP2:
        ui->audioEncodingLabel->setText("MP2");
        ui->audioEncodingLabel->setToolTip(QString(tr("<b>DAB audio encoding</b><br>%1")).arg(tr("MPEG-1 layer 2")));
        break;
    case AudioCoding::AACLC:
        ui->audioEncodingLabel->setText("AAC-LC");
        ui->audioEncodingLabel->setToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 Low Complexity AAC")));
        break;
    case AudioCoding::HEAAC:
        ui->audioEncodingLabel->setText("HE-AAC");
        ui->audioEncodingLabel->setToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 High Efficiency AAC")));
        break;
    case AudioCoding::HEAACv2:
        ui->audioEncodingLabel->setText("HE-AACv2");
        ui->audioEncodingLabel->setToolTip(QString(tr("<b>DAB+ audio encoding</b><br>%1")).arg(tr("MPEG-4 High Efficiency AAC v2")));
        break;
    }

    if (params.stereo)
    {
        ui->stereoLabel->setText("STEREO");
        if (AudioCoding::MP2 == params.coding)
        {
            ui->stereoLabel->setToolTip(QString(tr("<b>Audio signal</b><br>%1Stereo<br>Sample rate: %2 kHz"))
                    .arg(params.sbr ? "Joint " : "")
                    .arg(params.sampleRateKHz)
                );
        }
        else
        {
            ui->stereoLabel->setToolTip(QString(tr("<b>Audio signal</b><br>Stereo (PS %1)<br>Sample rate: %2 kHz (SBR %3)"))
                                            .arg(params.parametricStereo? tr("on") : tr("off"))
                                            .arg(params.sampleRateKHz)
                                            .arg(params.sbr ? tr("on") : tr("off"))
                );
        }
    }
    else
    {
        ui->stereoLabel->setText("MONO");
        if (AudioCoding::MP2 == params.coding)
        {
            ui->stereoLabel->setToolTip(QString(tr("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz"))
                        .arg(params.sampleRateKHz)
                    );
        }
        else
        {
            ui->stereoLabel->setToolTip(QString(tr("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz (SBR: %2)"))
                        .arg(params.sampleRateKHz)
                        .arg(params.sbr ? "on" : "off")
                    );
        }
    }
    m_audioRecordingAction->setEnabled(true);
}

void MainWindow::onProgrammeTypeChanged(const DabSId &sid, const DabPTy &pty)
{
    if (m_SId.value() == sid.value())
    {   // belongs to current service

        // ETSI EN 300 401 V2.1.1 [8.1.5]
        // At any one time, the PTy shall be either Static or Dynamic;
        // there shall be only one PTy per service.

        if (pty.d != 0)
        {   // dynamic PTy available != static PTy
            ui->programTypeLabel->setText(DabTables::getPtyName(pty.d));
            ui->programTypeLabel->setToolTip(QString(tr("<b>Programme Type</b><br>"
                                                        "%1 (dynamic)"))
                                                 .arg(DabTables::getPtyName(pty.d)));


        }
        else
        {
            ui->programTypeLabel->setText(DabTables::getPtyName(pty.s));
            ui->programTypeLabel->setToolTip(QString(tr("<b>Programme Type</b><br>"
                                                        "%1"))
                                                 .arg(DabTables::getPtyName(pty.s)));
        }

    }
    else { /* ignoring - not current service */ }
}

void MainWindow::channelSelected()
{
    m_hasListViewFocus = ui->serviceListView->hasFocus();
    m_hasTreeViewFocus = ui->serviceTreeView->hasFocus();
    if (InputDeviceId::RAWFILE != m_inputDeviceId)
    {
        ui->serviceListView->setEnabled(false);
        ui->serviceTreeView->setEnabled(false);
    }

    clearEnsembleInformationLabels();
    ui->channelCombo->setDisabled(true);
    ui->channelDown->setDisabled(true);
    ui->channelUp->setDisabled(true);
    ui->frequencyLabel->setText(tr("Tuning...  "));

    onSignalState(uint8_t(DabSyncLevel::NoSync), 0.0);

    // hide switch to avoid conflict with tuning -> will be enabled when tune is finished
    ui->switchSourceLabel->setHidden(true);
    serviceSelected();
}

void MainWindow::serviceSelected()
{
    emit stopUserApps();
    m_dlDecoder[Instance::Service]->reset();
    m_dlDecoder[Instance::Announcement]->reset();
    clearServiceInformationLabels();
    ui->dlWidget->setCurrentIndex(Instance::Service);
    ui->dlPlusWidget->setCurrentIndex(Instance::Service);
    ui->favoriteLabel->setEnabled(false);
    ui->slsWidget->setCurrentIndex(Instance::Service);
    m_audioRecordingAction->setDisabled(true);
}

void MainWindow::onChannelChange(int index)
{
    int idx = ui->channelCombo->findData(m_frequency, Qt::UserRole);
    if (index == idx)
    {   // no change (it can happen when audio recording is ongoing and channel change is rejected)
        return;
    }

    if (!stopAudioRecordingMsg(tr("Audio recording is ongoing. It will be stopped and saved if you change DAB channel.")))
    {
        // restore previous index
        ui->channelCombo->setCurrentIndex(idx);
        return;
    }

    if (m_frequency != ui->channelCombo->itemData(index).toUInt())
    {
        // no service is selected
        m_SId.set(0);

        // reset UI
        ui->serviceListView->clearSelection();
        ui->serviceTreeView->clearSelection();
        channelSelected();

        if (index < 0)
        {   // this indx is set when service list is cleared by user -> we want combo enabled
            ui->channelCombo->setEnabled(true);
            ui->channelDown->setEnabled(true);
            ui->channelUp->setEnabled(true);
        }

        emit serviceRequest(ui->channelCombo->itemData(index).toUInt(), 0, 0);
    }

    // update up/down button tooltip text
    int nextIdx = (index + 1) % ui->channelCombo->count();
    ui->channelUp->setTooltip(QString(tr("Tune to %1")).arg(ui->channelCombo->itemText(nextIdx)));

    int prevIdx = index - 1;
    if (prevIdx < 0)
    {
        prevIdx = ui->channelCombo->count() - 1;
    }
    else
    {
        prevIdx = prevIdx % ui->channelCombo->count();
    }
    ui->channelDown->setTooltip(QString(tr("Tune to %1")).arg(DabTables::channelList[ui->channelCombo->itemData(prevIdx).toUInt()]));
}

void MainWindow::onBandScanStart()
{
    stop();
    if (!m_keepServiceListOnScan)
    {
        m_serviceList->clear(false);  // do not clear favorites
    }
}

void MainWindow::onChannelUpClicked()
{
    int ch = ui->channelCombo->currentIndex();
    ch = (ch + 1) % ui->channelCombo->count();
    ui->channelCombo->setCurrentIndex(ch);
}

void MainWindow::onChannelDownClicked()
{
    int ch = ui->channelCombo->currentIndex();
    ch -= 1;
    if (ch < 0)
    {
        ch = ui->channelCombo->count() - 1;
    }
    else
    {
        ch = ch % ui->channelCombo->count();
    }
    ui->channelCombo->setCurrentIndex(ch);
}

void MainWindow::onTuneDone(uint32_t freq)
{   // this slot is called when tune is complete
    m_frequency = freq;
    if (freq != 0)
    {
        ui->channelCombo->setEnabled(true);
        ui->channelDown->setEnabled(true);
        ui->channelUp->setEnabled(true);
        ui->serviceListView->setEnabled(true);
        ui->serviceTreeView->setEnabled(true);
        if (m_hasListViewFocus)
        {
            ui->serviceListView->setFocus();
        }
        else if (m_hasTreeViewFocus)
        {
            ui->serviceTreeView->setFocus();
        }
        else
        {
            ui->channelCombo->setFocus();
        }

        ui->frequencyLabel->setText(QString("%1 MHz").arg(freq/1000.0, 3, 'f', 3, QChar('0')));
        m_isPlaying = true;

        // if current service has alternatives show icon immediately to avoid UI blocking when audio does not work
        if (m_serviceList->numEnsembles(ServiceListId(m_SId.value(), m_SCIdS)) > 1)
        {
            ui->switchSourceLabel->setVisible(true);
        }

        emit resetUserApps();  // new channel -> reset user apps
    }
    else
    {
        // this can only happen when device is changed, or when exit is requested
        if (m_exitRequested)
        {   // processing in IDLE, close window
            close();
            return;
        }

        ui->frequencyLabel->setText("");
        m_isPlaying = false;
        clearEnsembleInformationLabels();
        clearServiceInformationLabels();
        ui->serviceListView->setCurrentIndex(QModelIndex());
        ui->serviceTreeView->setCurrentIndex(QModelIndex());
        m_audioRecordingAction->setDisabled(true);
        if (m_deviceChangeRequested)
        {
            initInputDevice(m_inputDeviceIdRequest);
        }
    }
}

void MainWindow::onInputDeviceError(const InputDeviceErrorCode errCode)
{
    switch (errCode)
    {
    case InputDeviceErrorCode::EndOfFile:
        // tune to 0
        m_infoLabel->setText(tr("End of file"));
        m_timeBasicQualInfoWidget->setCurrentWidget(m_infoLabel);
        if (!m_setupDialog->settings().rawfile.loopEna)
        {
            m_infoLabel->setToolTip(tr("Select any service to restart"));
            ui->channelCombo->setCurrentIndex(-1);
        }
        else
        {   // rewind - restore info after timeout
            m_infoLabel->setToolTip("");
            QTimer::singleShot(2000, this, [this](){ restoreTimeQualWidget(); });
        }
        break;
    case InputDeviceErrorCode::DeviceDisconnected:
        m_infoLabel->setText(tr("Input device error: Device disconnected"));
        m_infoLabel->setToolTip(tr("Go to settings and try to reconnect the device"));
        m_timeBasicQualInfoWidget->setCurrentWidget(m_infoLabel);

        // force no device
        m_setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    case InputDeviceErrorCode::NoDataAvailable:
        m_infoLabel->setText(tr("Input device error: No data"));
        m_infoLabel->setToolTip(tr("Go to settings and try to reconnect the device"));
        m_timeBasicQualInfoWidget->setCurrentWidget(m_infoLabel);

        // force no device
        m_setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    default:
        qCWarning(application) << "InputDevice error" << int(errCode);
    }
}

bool MainWindow::stopAudioRecordingMsg(const QString & infoText)
{
    if (m_audioRecordingActive && !m_setupDialog->settings().audioRecAutoStopEna)
    //if (!m_setupDialog->settings().audioRecAutoStopEna)
    {
        QMessageBox msgBox(QMessageBox::Warning, tr("Warning"),
                           tr("Do you want to stop audio recording?"), {}, this);
        msgBox.setInformativeText(infoText);
#ifdef Q_OS_LINUX
#define KEEP_BUTTON_ROLE      QMessageBox::RejectRole
#define DONOTSHOW_BUTTON_ROLE QMessageBox::YesRole
#define STOP_BUTTON_ROLE      QMessageBox::AcceptRole
#else
#define KEEP_BUTTON_ROLE      QMessageBox::AcceptRole
#define DONOTSHOW_BUTTON_ROLE QMessageBox::RejectRole
#define STOP_BUTTON_ROLE      QMessageBox::RejectRole
#endif
        QPushButton * keepButton = msgBox.addButton(tr("Keep recording"), KEEP_BUTTON_ROLE);
        QPushButton * doNotShowButton = msgBox.addButton(tr("Stop recording and do not ask again"), DONOTSHOW_BUTTON_ROLE);
        msgBox.addButton(tr("Stop recording"), STOP_BUTTON_ROLE);

        msgBox.setEscapeButton(keepButton);
        msgBox.exec();
        if (msgBox.clickedButton() == keepButton)
        {
            return false;
        }
        if (msgBox.clickedButton() == doNotShowButton)
        {
            m_setupDialog->setAudioRecAutoStop(true);
        }
        return true;
    }
    return true;
}

void MainWindow::onServiceListSelection(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList	selectedList = selected.indexes();
    if (0 == selectedList.count()) {
        // no selection => return
        return;
    }

    QModelIndex currentIndex = selectedList.at(0);
    const SLModel * model = reinterpret_cast<const SLModel*>(currentIndex.model());
    if (model->id(currentIndex) == ServiceListId(m_SId.value(), m_SCIdS))
    {
        return;
    }

    if (!stopAudioRecordingMsg(tr("Audio recording is ongoing. It will be stopped and saved if you switch current service.")))
    {
        QModelIndexList	deselectedList = deselected.indexes();
        if (deselectedList.count() > 0)
        {
            QModelIndex previousIndex = deselectedList.at(0);
            ui->serviceListView->setCurrentIndex(previousIndex);
            return;
        }
    }

    ServiceListConstIterator it = m_serviceList->findService(model->id(currentIndex));
    if (m_serviceList->serviceListEnd() != it)
    {
        m_SId = (*it)->SId();
        m_SCIdS = (*it)->SCIdS();
        uint32_t newFrequency = (*it)->getEnsemble()->frequency();

        if (newFrequency != m_frequency)
        {
           m_frequency = newFrequency;

            // change combo
            ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(m_frequency));

            // set UI to new channel tuning
            channelSelected();
        }
        else
        {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
            ui->switchSourceLabel->setVisible(m_serviceList->numEnsembles(ServiceListId(m_SId.value(), m_SCIdS)) > 1);
        }
        serviceSelected();
        emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

        // synchronize tree view with service selection
        serviceTreeViewUpdateSelection();
    }
}

void MainWindow::onServiceListTreeSelection(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList	selectedList = selected.indexes();
    if (0 == selectedList.count()) {
        // no selection => return
        return;
    }

    QModelIndex currentIndex = selectedList.at(0);
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(currentIndex.model());

    if (currentIndex.parent().isValid())
    {   // service, not ensemble selected
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

        if (!stopAudioRecordingMsg(tr("Audio recording is ongoing. It will be stopped and saved if you switch current service.")))
        {
            QModelIndexList	deselectedList = deselected.indexes();
            if (deselectedList.count() > 0)
            {
                QModelIndex previousIndex = deselectedList.at(0);
                ui->serviceTreeView->setCurrentIndex(previousIndex);
                return;
            }
        }

        it = m_serviceList->findService(model->id(currentIndex));
        if (m_serviceList->serviceListEnd() != it)
        {
            m_SId = (*it)->SId();
            m_SCIdS = (*it)->SCIdS();

            uint32_t newFrequency = (*it)->switchEnsemble(model->id(currentIndex.parent()))->frequency();
            if (newFrequency != m_frequency)
            {
                m_frequency = newFrequency;

                // change combo
                ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(m_frequency));

                // set UI to new channel tuning
                channelSelected();
            }
            else
            {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
                ui->switchSourceLabel->setVisible(m_serviceList->numEnsembles(ServiceListId(m_SId.value(), m_SCIdS)) > 1);
            }
            serviceSelected();
            emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

            // we need to find the item in model and select it
            serviceListViewUpdateSelection();
        }
    }
}

void MainWindow::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    if (s.isAudioService())
    {
        if (s.SId.value() != m_SId.value())
        {   // this can happen when service is selected while still acquiring ensemble infomation
            m_SId = s.SId;
            m_SCIdS = s.SCIdS;
        }

        if (s.label.isEmpty())
        {   // service component not valid -> shoudl not happen
            return;
        }
        // set service name in UI until information arrives from decoder

        ui->favoriteLabel->setEnabled(true);

        ServiceListId id(s);
        ServiceListConstIterator it = m_serviceList->findService(id);
        if (it != m_serviceList->serviceListEnd())
        {
            //ui->favoriteLabel->setChecked((*it)->isFavorite());
            ui->favoriteLabel->setChecked(m_serviceList->isServiceFavorite(id));
            int numEns = (*it)->numEnsembles();
            if (numEns > 1)
            {
                ui->switchSourceLabel->setVisible(true);
                int current = (*it)->currentEnsembleIdx();
                const EnsembleListItem * ens = (*it)->getEnsemble(current+1);  // get next ensemble
                ui->switchSourceLabel->setToolTip(QString(tr("<b>Ensemble %1/%2</b><br>"
                                                             "Click for switching to:<br>"
                                                             "<i>%3</i>"))
                                                  .arg(current+1)
                                                  .arg(numEns)
                                                  .arg(ens->label()));
            }
        }
        ui->serviceLabel->setText(s.label);
        ui->serviceLabel->setToolTip(QString(tr("<b>Service:</b> %1<br>"
                                             "<b>Short label:</b> %2<br>"
                                             "<b>SId:</b> 0x%3<br>"
                                             "<b>SCIdS:</b> %4<br>"
                                             "<b>Language:</b> %5<br>"
                                                "<b>Country:</b> %6"))
                                     .arg(s.label)
                                     .arg(s.labelShort)
                                     .arg(QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper() )
                                     .arg(s.SCIdS)
                                     .arg(DabTables::getLangName(s.lang))
                                     .arg(DabTables::getCountryName(s.SId.value())));


        onProgrammeTypeChanged(s.SId, s.pty);
        displaySubchParams(s);
        restoreTimeQualWidget();

        QPixmap logo = m_metadataManager->data(s.SId.value(), s.SCIdS, MetadataManager::SmallLogo).value<QPixmap>();
        if (!logo.isNull())
        {
            ui->logoLabel->setPixmap(logo);
            ui->logoLabel->setVisible(true);
        }

        ui->slsView_Service->showServiceLogo(m_metadataManager->data(s.SId.value(), s.SCIdS, MetadataManager::SLSLogo).value<QPixmap>());
    }
    else
    {   // sid it not equal to selected sid -> this should not happen
        m_SId.set(0);

        ui->serviceListView->clearSelection();
        ui->serviceTreeView->clearSelection();
        //ui->serviceListView->selectionModel()->select(ui->serviceListView->selectionModel()->selection(), QItemSelectionModel::Deselect);
    }
}

void MainWindow::displaySubchParams(const RadioControlServiceComponent &s)
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
        ui->protectionLabel->setText(label);
        ui->protectionLabel->setToolTip(toolTip);

        QString br = QString(tr("%1 kbps")).arg(s.streamAudioData.bitRate);
        ui->audioBitrateLabel->setText(br);
        ui->audioBitrateLabel->setToolTip(QString(tr("<b>Service bitrate</b><br>Audio & data: %1")).arg(br));
    }
    else
    {   /* this should not happen */ }
}

void MainWindow::onAudioServiceReconfiguration(const RadioControlServiceComponent &s)
{
    if (s.SId.isValid() && s.isAudioService() && (s.SId.value() == m_SId.value()))
    {   // set UI
        onAudioServiceSelection(s);
        emit getAudioInfo();
    }
    else
    {   // service probably disapeared
        // ETSI TS 103 176 V2.4.1 (2020-08) [6.4.3 Receiver behaviour]
        // If by the above methods a continuation of service cannot be established, the receiver shall stop the service.
        // It should display a 'service ceased' message as appropriate.

        QString serviceLabel = ui->serviceLabel->text();
        clearServiceInformationLabels();
        ui->serviceListView->setCurrentIndex(QModelIndex());
        ui->serviceTreeView->setCurrentIndex(QModelIndex());
        ui->serviceLabel->setText(serviceLabel);
        ui->programTypeLabel->setText(tr("Service currently unavailable"));
        ui->programTypeLabel->setToolTip(tr("Service was removed from ensemble"));

        emit stopUserApps();
        m_dlDecoder[Instance::Service]->reset();
        m_dlDecoder[Instance::Announcement]->reset();

        ui->favoriteLabel->setEnabled(false);
        m_audioRecordingAction->setDisabled(true);
    }
}

void MainWindow::onAnnouncement(const DabAnnouncement id, const RadioControlAnnouncementState state, const RadioControlServiceComponent & s)
{
    switch (state)
    {
    case RadioControlAnnouncementState::None:
        ui->dlWidget->setCurrentIndex(Instance::Service);
        ui->dynamicLabel_Announcement->clear();   // clear for next announcment
        ui->dlPlusWidget->setCurrentIndex(Instance::Service);
        // reset DL+
        for (auto objPtr : m_dlObjCache[Instance::Announcement])
        {
            delete objPtr;
        }
        m_dlObjCache[Instance::Announcement].clear();

        // disabling DL+ if no objects
        toggleDLPlus(!m_dlObjCache[Instance::Service].isEmpty());

        ui->slsWidget->setCurrentIndex(Instance::Service);
        ui->announcementLabel->setVisible(false);

        // enable audio recording menu item
        m_audioRecordingAction->setEnabled(true);
        break;
    case RadioControlAnnouncementState::OnCurrentService:
        ui->announcementLabel->setToolTip(QString(tr("<b>%1</b><br>"
                                                     "Ongoing announcement<br>"
                                                     "on current service"))
                                              .arg(DabTables::getAnnouncementName(id)));
        ui->announcementLabel->setChecked(true);
        ui->announcementLabel->setEnabled(false);
        ui->announcementLabel->setVisible(true);
        ui->slsWidget->setCurrentIndex(Instance::Service);

        // enable audio recording menu item
        m_audioRecordingAction->setEnabled(true);
        break;
    case RadioControlAnnouncementState::OnOtherService:
        ui->announcementLabel->setToolTip(QString(tr("<b>%1</b><br>"
                                                     "Ongoing announcement<br>"
                                                     "Source service: <i>%2</i><br>"
                                                     "<br>"
                                                     "Click to suspend this announcement"))
                                              .arg(DabTables::getAnnouncementName(id), s.label));
        ui->announcementLabel->setChecked(true);
        ui->announcementLabel->setEnabled(true);
        ui->announcementLabel->setVisible(true);        

        ui->dlWidget->setCurrentIndex(Instance::Announcement);
        ui->dlPlusWidget->setCurrentIndex(Instance::Announcement);
        ui->slsWidget->setCurrentIndex(Instance::Announcement);

        // disable audio recording menu item
        m_audioRecordingAction->setEnabled(false);

        break;
    case RadioControlAnnouncementState::Suspended:
        ui->announcementLabel->setToolTip(QString(tr("<b>%1</b><br>"
                                                     "Suspended announcement<br>"
                                                     "Source service: <i>%2</i><br>"
                                                     "<br>"
                                                     "Click to resume this announcement"))
                                              .arg(DabTables::getAnnouncementName(id), s.label));
        ui->announcementLabel->setChecked(false);
        ui->announcementLabel->setEnabled(true);
        ui->announcementLabel->setVisible(true);

        ui->dlWidget->setCurrentIndex(Instance::Service);
        ui->dynamicLabel_Announcement->clear();   // clear for next announcment
        ui->dlPlusWidget->setCurrentIndex(Instance::Service);
        // reset DL+
        for (auto objPtr : m_dlObjCache[Instance::Announcement])
        {
            delete objPtr;
        }
        m_dlObjCache[Instance::Announcement].clear();

        // disabling DL+ if no objects
        toggleDLPlus(!m_dlObjCache[Instance::Service].isEmpty());

        ui->slsWidget->setCurrentIndex(Instance::Service);

        // enable audio recording menu item
        m_audioRecordingAction->setEnabled(true);
        break;
    }

    displaySubchParams(s);

    if ((DabAnnouncement::Alarm == id) && (m_setupDialog->settings().bringWindowToForeground))
    {
        show(); //bring window to top on OSX
        raise(); //bring window from minimized state on OSX
        activateWindow(); //bring window to front/unminimize on windows
    }
}

void MainWindow::onAudioDevicesList(QList<QAudioDevice> list)
{
    m_audioOutputMenu->clear();
    if (nullptr != m_audioDevicesGroup)
    {
        delete m_audioDevicesGroup;
    }
    m_audioDevicesGroup = new QActionGroup(m_audioOutputMenu);
    for (const QAudioDevice &device : list)
    {
        QAction * action = new QAction(device.description(), m_audioDevicesGroup);
        action->setData(QVariant::fromValue(device));
        action->setCheckable(true);
    }
    m_audioOutputMenu->addActions(m_audioDevicesGroup->actions());
}

void MainWindow::onAudioOutputError()
{   // tuning to 0
    // no service is selected
    m_SId.set(0);

    qCWarning(application, "Audio output error");
    m_infoLabel->setText(tr("Audio Output Error"));
    m_infoLabel->setToolTip(tr("Try to select other service to recover"));
    m_timeBasicQualInfoWidget->setCurrentWidget(m_infoLabel);
    emit audioStop();
}

void MainWindow::onAudioOutputSelected(QAction *action)
{
    emit audioOutput(action->data().value<QAudioDevice>().id());
}

void MainWindow::onAudioDeviceChanged(const QByteArray &id)
{
    for (const auto & action : m_audioDevicesGroup->actions())
    {
        if (action->data().value<QAudioDevice>().id() == id)
        {
            action->setChecked(true);
            return;
        }
    }

    if (m_audioDevicesGroup->actions().size() > 0)
    {
        qCWarning(application) << "Default audio device selected" << m_audioDevicesGroup->actions().at(0)->data().value<QAudioDevice>().description();
        m_audioDevicesGroup->actions().at(0)->setChecked(true);
    }
}

void MainWindow::audioRecordingToggle()
{
    if (m_audioRecordingActive)
    {
        emit stopAudioRecording();
    }
    else
    {
        emit startAudioRecording();
    }
}

void MainWindow::onAudioRecordingStarted(const QString &filename)
{
    emit announcementMask(0x0001);             // disable announcements during recording (only alarm is enabled)
    m_audioRecordingFile = filename;
    m_audioRecordingActive = true;
    onAudioRecordingProgress(0, 0);
    setAudioRecordingUI();
}

void MainWindow::onAudioRecordingStopped()
{    
    m_audioRecordingActive = false;
    setAudioRecordingUI();
    emit announcementMask(m_setupDialog->settings().announcementEna);   // restore announcement settings
}

void MainWindow::onAudioRecordingProgress(size_t bytes, size_t timeSec)
{
    int min = timeSec / 60;
    m_audioRecordingProgressLabel->setText(QString(tr("Audio recording: %1:%2")).arg(min).arg(timeSec - min * 60, 2, 10, QChar('0')));
    m_audioRecordingProgressLabel->setToolTip(QString(tr("Audio recording ongoing (%2 kBytes recorded)\n"
                                                         "File: %1")).arg(m_audioRecordingFile).arg(bytes >> 10));
}

void MainWindow::setAudioRecordingUI()
{
    if (m_audioRecordingActive)
    {
        m_audioRecordingWidget->setVisible(true);
        m_audioRecordingAction->setText(tr("Stop audio recording"));
    }
    else
    {   m_audioRecordingWidget->setVisible(false);
        m_audioRecordingAction->setText(tr("Start audio recording"));
    }
}


void MainWindow::onMetadataUpdated(const ServiceListId & id, MetadataManager::MetadataRole role)
{
    if ((id.sid() == m_SId.value()) && (id.scids() == m_SCIdS))
    {  // current service data
        switch (role)
        {
        case MetadataManager::MetadataRole::SLSLogo:
            ui->slsView_Service->showServiceLogo(m_metadataManager->data(id, MetadataManager::SLSLogo).value<QPixmap>());
            break;
        case MetadataManager::SmallLogo:
        {
            QPixmap logo = m_metadataManager->data(id, MetadataManager::SmallLogo).value<QPixmap>();
            if (!logo.isNull())
            {
                ui->logoLabel->setPixmap(logo);
                ui->logoLabel->setVisible(true);
            }
        }
            break;
        default:
            break;
        }
    }
}

void MainWindow::onEpgEmpty()
{
    m_epgDialog->close();
    m_epgAction->setEnabled(false);
}

void MainWindow::clearEnsembleInformationLabels()
{
    m_timeLabel->setText("");
    ui->ensembleLabel->setText(tr("No ensemble"));
    ui->ensembleLabel->setToolTip(tr("No ensemble tuned"));
    ui->frequencyLabel->setText("");
    ui->ensembleInfoLabel->setText("");
}

void MainWindow::clearServiceInformationLabels()
{
    ui->serviceLabel->setText(tr("No service"));
    ui->favoriteLabel->setChecked(false);
    ui->catSlsLabel->setHidden(true);
    ui->logoLabel->setHidden(true);
    ui->announcementLabel->setHidden(true);
    ui->serviceLabel->setToolTip(tr("No service playing"));
    ui->programTypeLabel->setText("");
    ui->audioEncodingLabel->setText("");
    ui->audioEncodingLabel->setToolTip("");
    ui->stereoLabel->setText("");
    ui->stereoLabel->setToolTip("");
    ui->protectionLabel->setText("");
    ui->protectionLabel->setToolTip("");
    ui->audioBitrateLabel->setText("");
    ui->audioBitrateLabel->setToolTip("");
    ui->announcementLabel->setVisible(false);
    ui->slsWidget->setCurrentIndex(Instance::Service);
    ui->dlPlusWidget->setCurrentIndex(Instance::Service);
    ui->dlWidget->setCurrentIndex(Instance::Service);
    onDLReset_Service();
    onDLReset_Announcement();
}

void MainWindow::onNewAnnouncementSettings()
{
    if (!m_audioRecordingActive)
    {
        emit announcementMask(m_setupDialog->settings().announcementEna);
    }
}

void MainWindow::onNewInputDeviceSettings()
{
    SetupDialog::Settings s = m_setupDialog->settings();
    switch (m_inputDeviceId)
    {
    case InputDeviceId::RTLSDR:
        dynamic_cast<RtlSdrInput*>(m_inputDevice)->setGainMode(s.rtlsdr.gainMode, s.rtlsdr.gainIdx);
        dynamic_cast<RtlSdrInput*>(m_inputDevice)->setBW(s.rtlsdr.bandwidth);
        dynamic_cast<RtlSdrInput*>(m_inputDevice)->setBiasT(s.rtlsdr.biasT);
        dynamic_cast<RtlSdrInput*>(m_inputDevice)->setAgcLevelMax(s.rtlsdr.agcLevelMax);
        break;
    case InputDeviceId::RTLTCP:
        dynamic_cast<RtlTcpInput*>(m_inputDevice)->setGainMode(s.rtltcp.gainMode, s.rtltcp.gainIdx);
        dynamic_cast<RtlTcpInput*>(m_inputDevice)->setAgcLevelMax(s.rtltcp.agcLevelMax);
        break;
    case InputDeviceId::AIRSPY:
#if HAVE_AIRSPY
        dynamic_cast<AirspyInput*>(m_inputDevice)->setGainMode(s.airspy.gain);
        dynamic_cast<AirspyInput*>(m_inputDevice)->setBiasT(s.airspy.biasT);
#endif
        break;
    case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setGainMode(s.soapysdr.gainMode, s.soapysdr.gainIdx);
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setBW(s.soapysdr.bandwidth);
#endif
        break;
    case InputDeviceId::RAWFILE:
        break;
    case InputDeviceId::UNDEFINED:
        break;
    }
}


void MainWindow::changeInputDevice(const InputDeviceId & d)
{
    m_inputDeviceIdRequest = d;
    m_deviceChangeRequested = true;
    if (m_isPlaying)
    { // stop
        stop();
        ui->channelCombo->setDisabled(true);  // enabled when device is ready
        ui->channelDown->setDisabled(true);
        ui->channelUp->setDisabled(true);
    }
    else
    { // device is not playing
        initInputDevice(d);
    }
}

void MainWindow::initInputDevice(const InputDeviceId & d)
{
    m_deviceChangeRequested = false;
    if (nullptr != m_inputDevice)
    {
        delete m_inputDevice;
    }

    // disable band scan - will be enable when it makes sense (RTL-SDR at the moment)
    m_bandScanAction->setDisabled(true);

    // disable file recording
    m_ensembleInfoDialog->enableRecording(false);

    switch (d)
    {
    case InputDeviceId::UNDEFINED:
        // store service list if previous was not RAWFILE or UNDEFINED
        if ((InputDeviceId::RAWFILE != m_inputDeviceId) && (InputDeviceId::UNDEFINED != m_inputDeviceId))
        {   // if switching from live source save current service list
            QSettings * settings;
            if (m_iniFilename.isEmpty())
            {
                settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
            }
            else
            {
                settings = new QSettings(m_iniFilename, QSettings::IniFormat);
            }
            m_serviceList->save(*settings);
            settings->sync();

            delete settings;
        }
        else { /* do nothing if switching from RAW file */ }

        m_inputDeviceId = InputDeviceId::UNDEFINED;
        m_inputDevice = nullptr;
        ui->channelCombo->setDisabled(true);   // it will be enabled when device is ready
        ui->channelDown->setDisabled(true);
        ui->channelUp->setDisabled(true);

        ui->serviceListView->setEnabled(false);
        ui->serviceTreeView->setEnabled(false);
        ui->favoriteLabel->setEnabled(false);

        break;
    case InputDeviceId::RTLSDR:
    {
        m_inputDevice = new RtlSdrInput();

        // signals have to be connected before calling openDevice

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        if (m_inputDevice->openDevice())
        {   // rtl sdr is available
            if ((InputDeviceId::RAWFILE == m_inputDeviceId) || (InputDeviceId::UNDEFINED == m_inputDeviceId))
            {   // if switching from RAW or UNDEFINED load service list

                // clear service list
                m_serviceList->clear();

                QSettings * settings;
                if (m_iniFilename.isEmpty())
                {
                    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
                }
                else
                {
                    settings = new QSettings(m_iniFilename, QSettings::IniFormat);
                }
                m_serviceList->load(*settings);
                delete settings;
            }
            else { /* keep service list as it is */}

            m_inputDeviceId = InputDeviceId::RTLSDR;

            // setup dialog
            m_setupDialog->setGainValues(dynamic_cast<RtlSdrInput*>(m_inputDevice)->getGainList());

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // recorder
            m_inputDeviceRecorder->setDeviceDescription(m_inputDevice->deviceDescription());
            connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_inputDevice, &InputDevice::startStopRecording);
            connect(m_inputDevice, &InputDevice::recordBuffer, m_inputDeviceRecorder, &InputDeviceRecorder::writeBuffer, Qt::DirectConnection);

            // ensemble info dialog
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableRecording(true);

            // metadata & EPG
            EPGTime::getInstance()->setIsLiveBroadcasting(true);

            // apply current settings
            onNewInputDeviceSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
        break;
    case InputDeviceId::RTLTCP:
    {
        m_inputDevice = new RtlTcpInput();

        // signals have to be connected before calling openDevice
        // RTL_TCP is opened immediately and starts receiving data

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // set IP address and port
        dynamic_cast<RtlTcpInput*>(m_inputDevice)->setTcpIp(m_setupDialog->settings().rtltcp.tcpAddress, m_setupDialog->settings().rtltcp.tcpPort);

        if (m_inputDevice->openDevice())
        {  // rtl tcp is available
            if ((InputDeviceId::RAWFILE == m_inputDeviceId) || (InputDeviceId::UNDEFINED == m_inputDeviceId))
            {   // if switching from RAW or UNDEFINED load service list

                // clear service list
                m_serviceList->clear();

                QSettings * settings;
                if (m_iniFilename.isEmpty())
                {
                    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
                }
                else
                {
                    settings = new QSettings(m_iniFilename, QSettings::IniFormat);
                }
                m_serviceList->load(*settings);
                delete settings;
            }
            else { /* keep service list as it is */}

            m_inputDeviceId = InputDeviceId::RTLTCP;

            // setup dialog
            m_setupDialog->setGainValues(dynamic_cast<RtlTcpInput*>(m_inputDevice)->getGainList());

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // recorder
            m_inputDeviceRecorder->setDeviceDescription(m_inputDevice->deviceDescription());
            connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_inputDevice, &InputDevice::startStopRecording);
            connect(m_inputDevice, &InputDevice::recordBuffer, m_inputDeviceRecorder, &InputDeviceRecorder::writeBuffer, Qt::DirectConnection);

            // ensemble info dialog
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableRecording(true);

            // metadata & EPG
            EPGTime::getInstance()->setIsLiveBroadcasting(true);

            // apply current settings
            onNewInputDeviceSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
    break;
    case InputDeviceId::AIRSPY:
    {
#if HAVE_AIRSPY
        m_inputDevice = new AirspyInput(m_setupDialog->settings().airspy.prefer4096kHz);

        // signals have to be connected before calling isAvailable

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        if (m_inputDevice->openDevice())
        {  // airspy is available
            if ((InputDeviceId::RAWFILE == m_inputDeviceId) || (InputDeviceId::UNDEFINED == m_inputDeviceId))
            {   // if switching from RAW or UNDEFINED load service list

                // clear service list
                m_serviceList->clear();

                QSettings * settings;
                if (m_iniFilename.isEmpty())
                {
                    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
                }
                else
                {
                    settings = new QSettings(m_iniFilename, QSettings::IniFormat);
                }
                m_serviceList->load(*settings);
                delete settings;
            }
            else { /* keep service list as it is */}

            m_inputDeviceId = InputDeviceId::AIRSPY;

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            // recorder
            m_inputDeviceRecorder->setDeviceDescription(m_inputDevice->deviceDescription());
            connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_inputDevice, &InputDevice::startStopRecording);
            connect(m_inputDevice, &InputDevice::recordBuffer, m_inputDeviceRecorder, &InputDeviceRecorder::writeBuffer, Qt::DirectConnection);

            // ensemble info dialog
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableRecording(true);

            // metadata & EPG
            EPGTime::getInstance()->setIsLiveBroadcasting(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<AirspyInput*>(m_inputDevice)->setDataPacking(m_setupDialog->settings().airspy.dataPacking);

            // apply current settings
            onNewInputDeviceSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
#endif
    }
    break;
    case InputDeviceId::SOAPYSDR:
    {
#if HAVE_SOAPYSDR
        m_inputDevice = new SoapySdrInput();

        // signals have to be connected before calling isAvailable

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // set connection paramaters
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setDevArgs(m_setupDialog->settings().soapysdr.devArgs);
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setRxChannel(m_setupDialog->settings().soapysdr.channel);
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setAntenna(m_setupDialog->settings().soapysdr.antenna);

        if (m_inputDevice->openDevice())
        {  // SoapySDR is available
            if ((InputDeviceId::RAWFILE == m_inputDeviceId) || (InputDeviceId::UNDEFINED == m_inputDeviceId))
            {   // if switching from RAW or UNDEFINED load service list

                // clear service list
                m_serviceList->clear();

                QSettings * settings;
                if (m_iniFilename.isEmpty())
                {
                    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
                }
                else
                {
                    settings = new QSettings(m_iniFilename, QSettings::IniFormat);
                }
                m_serviceList->load(*settings);
                delete settings;
            }
            else { /* keep service list as it is */}

            m_inputDeviceId = InputDeviceId::SOAPYSDR;

            // setup dialog
            m_setupDialog->setGainValues(dynamic_cast<SoapySdrInput*>(m_inputDevice)->getGainList());

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            // recorder
            m_inputDeviceRecorder->setDeviceDescription(m_inputDevice->deviceDescription());
            connect(m_inputDeviceRecorder, &InputDeviceRecorder::recording, m_inputDevice, &InputDevice::startStopRecording);
            connect(m_inputDevice, &InputDevice::recordBuffer, m_inputDeviceRecorder, &InputDeviceRecorder::writeBuffer, Qt::DirectConnection);

            // ensemble info dialog
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableRecording(true);

            // metadata & EPG
            EPGTime::getInstance()->setIsLiveBroadcasting(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<SoapySdrInput*>(m_inputDevice)->setBW(m_setupDialog->settings().soapysdr.bandwidth);

            // apply current settings
            onNewInputDeviceSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
#endif
    }
    break;

    case InputDeviceId::RAWFILE:
    {
        m_inputDevice = new RawFileInput();

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        RawFileInputFormat format = m_setupDialog->settings().rawfile.format;
        dynamic_cast<RawFileInput*>(m_inputDevice)->setFile(m_setupDialog->settings().rawfile.file, format);

        connect(dynamic_cast<RawFileInput*>(m_inputDevice), &RawFileInput::fileLength, m_setupDialog, &SetupDialog::onFileLength, Qt::QueuedConnection);
        connect(dynamic_cast<RawFileInput*>(m_inputDevice), &RawFileInput::fileProgress, m_setupDialog, &SetupDialog::onFileProgress, Qt::QueuedConnection);

        // we can open device now
        if (m_inputDevice->openDevice())
        {   // raw file is available
            if (InputDeviceId::RAWFILE != m_inputDeviceId)
            {   // if switching from live source save current service list
                QSettings * settings;
                if (m_iniFilename.isEmpty())
                {
                    settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
                }
                else
                {
                    settings = new QSettings(m_iniFilename, QSettings::IniFormat);
                }
                m_serviceList->save(*settings);
                settings->sync();

                delete settings;
            }
            else { /* do nothing if switching from RAW file */ }

            // clear service list
            m_serviceList->clear();

            m_inputDeviceId = InputDeviceId::RAWFILE;

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // show XML header is available
            m_setupDialog->setXmlHeader(m_inputDevice->deviceDescription());

            // metadata & EPG
            EPGTime::getInstance()->setIsLiveBroadcasting(false);

            // apply current settings
            onNewInputDeviceSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
        break;
    }
}

void MainWindow::loadSettings()
{
    QSettings * settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    // load servicelist
    m_serviceList->load(*settings);

    m_audioVolume = settings->value("volume", 100).toInt();
    m_audioVolumeSlider->setValue(m_audioVolume);
    bool mute = settings->value("mute", false).toBool();
    if (mute)
    {   // this sends signal
        m_muteLabel->toggle();
    }
    emit audioOutput(settings->value("audioDevice", "").toByteArray());
    m_keepServiceListOnScan = settings->value("keepServiceListOnScan", false).toBool();

    int inDevice = settings->value("inputDeviceId", int(InputDeviceId::RTLSDR)).toInt();

    SetupDialog::Settings s;

    s.expertModeEna = settings->value("expertMode", false).toBool();
    setExpertMode(s.expertModeEna);

    QSize sz = size();
    QByteArray geometry = settings->value("windowGeometry").toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
        sz = size();
    }
    else
    {   // this should happen only when ini is deleted ot on the first run
        sz = minimumSizeHint();
    }
    // this is workaround to force size when window appears (not clear why it is necessary)
    QTimer::singleShot(10, this, [this, sz](){ resize(sz); } );

    s.applicationStyle = static_cast<ApplicationStyle>(settings->value("style", static_cast<int>(ApplicationStyle::Default)).toInt());
    s.dlPlusEna = settings->value("dlPlus", true).toBool();
    s.lang = QLocale::codeToLanguage(settings->value("language", QString("")).toString());
    s.inputDevice = static_cast<InputDeviceId>(inDevice);
    s.announcementEna = settings->value("announcementEna", 0x07FF).toUInt();
    s.bringWindowToForeground = settings->value("bringWindowToForegroundOnAlarm", true).toBool();
    s.noiseConcealmentLevel = settings->value("noiseConcealment", 0).toInt();
    s.xmlHeaderEna = settings->value("rawFileXmlHeader", true).toBool();
    s.spiAppEna = settings->value("spiAppEna", true).toBool();
    s.useInternet = settings->value("useInternet", true).toBool();
    s.radioDnsEna = settings->value("radioDNS", true).toBool();
    s.audioRecFolder = settings->value("audioRecFolder", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString();
    s.audioRecCaptureOutput = settings->value("audioRecCaptureOutput", false).toBool();
    s.audioRecAutoStopEna = settings->value("audioRecAutoStop", false).toBool();

    s.rtlsdr.gainIdx = settings->value("RTL-SDR/gainIndex", 0).toInt();
    s.rtlsdr.gainMode = static_cast<RtlGainMode>(settings->value("RTL-SDR/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    s.rtlsdr.bandwidth = settings->value("RTL-SDR/bandwidth", 0).toUInt();
    s.rtlsdr.biasT = settings->value("RTL-SDR/bias-T", false).toBool();
    s.rtlsdr.agcLevelMax = settings->value("RTL-SDR/agcLevelMax", 0).toInt();

    s.rtltcp.gainIdx = settings->value("RTL-TCP/gainIndex", 0).toInt();
    s.rtltcp.gainMode = static_cast<RtlGainMode>(settings->value("RTL-TCP/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    s.rtltcp.tcpAddress = settings->value("RTL-TCP/address", QString("127.0.0.1")).toString();
    s.rtltcp.tcpPort = settings->value("RTL-TCP/port", 1234).toInt();
    s.rtltcp.agcLevelMax = settings->value("RTL-TCP/agcLevelMax", 0).toInt();

#if HAVE_AIRSPY
    s.airspy.gain.sensitivityGainIdx = settings->value("AIRSPY/sensitivityGainIdx", 9).toInt();
    s.airspy.gain.lnaGainIdx = settings->value("AIRSPY/lnaGainIdx", 0).toInt();
    s.airspy.gain.mixerGainIdx = settings->value("AIRSPY/mixerGainIdx", 0).toInt();
    s.airspy.gain.ifGainIdx = settings->value("AIRSPY/ifGainIdx", 5).toInt();
    s.airspy.gain.lnaAgcEna = settings->value("AIRSPY/lnaAgcEna", true).toBool();
    s.airspy.gain.mixerAgcEna = settings->value("AIRSPY/mixerAgcEna", true).toBool();
    s.airspy.gain.mode = static_cast<AirpyGainMode>(settings->value("AIRSPY/gainMode", static_cast<int>(AirpyGainMode::Hybrid)).toInt());
    s.airspy.biasT = settings->value("AIRSPY/bias-T", false).toBool();
    s.airspy.dataPacking = settings->value("AIRSPY/dataPacking", true).toBool();
    s.airspy.prefer4096kHz = settings->value("AIRSPY/preferSampleRate4096kHz", true).toBool();
#endif
#if HAVE_SOAPYSDR
    s.soapysdr.gainIdx = settings->value("SOAPYSDR/gainIndex", 0).toInt();
    s.soapysdr.gainMode = static_cast<SoapyGainMode>(settings->value("SOAPYSDR/gainMode", static_cast<int>(SoapyGainMode::Hardware)).toInt());
    s.soapysdr.devArgs = settings->value("SOAPYSDR/devArgs", QString("driver=rtlsdr")).toString();
    s.soapysdr.antenna = settings->value("SOAPYSDR/antenna", QString("RX")).toString();
    s.soapysdr.channel = settings->value("SOAPYSDR/rxChannel", 0).toInt();
    s.soapysdr.bandwidth = settings->value("SOAPYSDR/bandwidth", 0).toUInt();
#endif
    s.rawfile.file = settings->value("RAW-FILE/filename", QVariant(QString(""))).toString();
    s.rawfile.format = RawFileInputFormat(settings->value("RAW-FILE/format", 0).toInt());
    s.rawfile.loopEna = settings->value("RAW-FILE/loop", false).toBool();

    m_setupDialog->setSettings(s);

    // set DAB time locale
    m_timeLocale = QLocale(m_setupDialog->applicationLanguage());
    EPGTime::getInstance()->setTimeLocale(m_timeLocale);

    // need to run here because it expects that settings is up-to-date
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    // theme color looks horible on Linux -> always force color theme
    if (ApplicationStyle::Default != s.applicationStyle)
#endif
    {
        onApplicationStyleChanged(s.applicationStyle);
    }

    m_inputDeviceRecorder->setRecordingPath(settings->value("recordingPath", QVariant(QDir::homePath())).toString());

    if (InputDeviceId::UNDEFINED != static_cast<InputDeviceId>(inDevice))
    {
        initInputDevice(s.inputDevice);

        // if input device has switched to what was stored and it is RTLSDR or RTLTCP or Airspy
        if ((s.inputDevice == m_inputDeviceId)
                && (    (InputDeviceId::RTLSDR == m_inputDeviceId)
                     || (InputDeviceId::AIRSPY == m_inputDeviceId)
                     || (InputDeviceId::SOAPYSDR == m_inputDeviceId)
                     || (InputDeviceId::RTLTCP == m_inputDeviceId)))
        {   // restore channel
            int sid = settings->value("SID", 0).toInt();
            uint8_t scids = settings->value("SCIdS", 0).toInt();
            ServiceListId id(sid, scids);

            // we need to find the item in model and select it
            const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
            QModelIndex index;
            for (int r = 0; r < model->rowCount(); ++r)
            {
                index = model->index(r, 0);
                if (model->id(index) == id)
                {   // found
                    ui->serviceListView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
                    //onServiceListSelection(index);   // this selects service
                    break;
                }
            }
        }
    }

    delete settings;

    if (((InputDeviceId::RTLSDR == m_inputDeviceId)
         || (InputDeviceId::AIRSPY == m_inputDeviceId)
         || (InputDeviceId::SOAPYSDR == m_inputDeviceId))
       && (m_serviceList->numServices() == 0))
    {
        QTimer::singleShot(1, this, [this](){ bandScan(); } );
    }
    if ((InputDeviceId::UNDEFINED == m_inputDeviceId)
        || ((InputDeviceId::RAWFILE == m_inputDeviceId) && (s.rawfile.file.isEmpty())))
    {
        QTimer::singleShot(1, this, [this](){ showSetupDialog(); } );
    }
}

MainWindow::AudioFramework MainWindow::getAudioFramework()
{
#if (HAVE_PORTAUDIO)
    QSettings * settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    int val = settings->value("audioFramework", AudioFramework::Pa).toInt();
    delete settings;

    return static_cast<AudioFramework>(val);
#else
    return AudioFramework::Qt;
#endif
}

void MainWindow::saveSettings()
{
    QSettings * settings;
    if (m_iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(m_iniFilename, QSettings::IniFormat);
    }

    const SetupDialog::Settings s = m_setupDialog->settings();
    settings->setValue("inputDeviceId", int(s.inputDevice));
    settings->setValue("recordingPath", m_inputDeviceRecorder->recordingPath());
    if (nullptr != m_audioDevicesGroup)
    {
        settings->setValue("audioDevice", m_audioDevicesGroup->checkedAction()->data().value<QAudioDevice>().id());
    }
#if (HAVE_PORTAUDIO)
    if (nullptr != dynamic_cast<AudioOutputPa *>(m_audioOutput))
    {
        settings->setValue("audioFramework", static_cast<int>(AudioFramework::Pa));
    }
    else
#endif
    {
        settings->setValue("audioFramework", static_cast<int>(AudioFramework::Qt));
    }
    settings->setValue("volume", m_audioVolume);
    settings->setValue("mute", m_muteLabel->isChecked());
    settings->setValue("keepServiceListOnScan", m_keepServiceListOnScan);
    settings->setValue("windowGeometry", saveGeometry());
    settings->setValue("style", static_cast<int>(s.applicationStyle));
    settings->setValue("announcementEna", s.announcementEna);
    settings->setValue("bringWindowToForegroundOnAlarm", s.bringWindowToForeground);
    settings->setValue("expertMode", s.expertModeEna);
    settings->setValue("dlPlus", s.dlPlusEna);
    settings->setValue("language", QLocale::languageToCode(s.lang));
    settings->setValue("noiseConcealment", s.noiseConcealmentLevel);
    settings->setValue("rawFileXmlHeader", s.xmlHeaderEna);
    settings->setValue("spiAppEna", s.spiAppEna);
    settings->setValue("useInternet", s.useInternet);
    settings->setValue("radioDNS", s.radioDnsEna);
    settings->setValue("audioRecFolder", s.audioRecFolder);
    settings->setValue("audioRecCaptureOutput", s.audioRecCaptureOutput);
    settings->setValue("audioRecAutoStop", s.audioRecAutoStopEna);

    settings->setValue("RTL-SDR/gainIndex", s.rtlsdr.gainIdx);
    settings->setValue("RTL-SDR/gainMode", static_cast<int>(s.rtlsdr.gainMode));
    settings->setValue("RTL-SDR/bandwidth", s.rtlsdr.bandwidth);
    settings->setValue("RTL-SDR/bias-T", s.rtlsdr.biasT);
    settings->setValue("RTL-SDR/agcLevelMax", s.rtlsdr.agcLevelMax);

#if HAVE_AIRSPY
    settings->setValue("AIRSPY/sensitivityGainIdx", s.airspy.gain.sensitivityGainIdx);
    settings->setValue("AIRSPY/lnaGainIdx", s.airspy.gain.lnaGainIdx);
    settings->setValue("AIRSPY/mixerGainIdx", s.airspy.gain.mixerGainIdx);
    settings->setValue("AIRSPY/ifGainIdx", s.airspy.gain.ifGainIdx);
    settings->setValue("AIRSPY/lnaAgcEna", s.airspy.gain.lnaAgcEna);
    settings->setValue("AIRSPY/mixerAgcEna", s.airspy.gain.mixerAgcEna);
    settings->setValue("AIRSPY/gainMode", static_cast<int>(s.airspy.gain.mode));
    settings->setValue("AIRSPY/bias-T", s.airspy.biasT);
    settings->setValue("AIRSPY/dataPacking", s.airspy.dataPacking);
    settings->setValue("AIRSPY/preferSampleRate4096kHz", s.airspy.prefer4096kHz);
#endif

#if HAVE_SOAPYSDR
    settings->setValue("SOAPYSDR/gainIndex", s.soapysdr.gainIdx);
    settings->setValue("SOAPYSDR/gainMode", static_cast<int>(s.soapysdr.gainMode));
    settings->setValue("SOAPYSDR/devArgs", s.soapysdr.devArgs);
    settings->setValue("SOAPYSDR/rxChannel", s.soapysdr.channel);
    settings->setValue("SOAPYSDR/antenna", s.soapysdr.antenna);
    settings->setValue("SOAPYSDR/bandwidth", s.soapysdr.bandwidth);
#endif

    settings->setValue("RTL-TCP/gainIndex", s.rtltcp.gainIdx);
    settings->setValue("RTL-TCP/gainMode", static_cast<int>(s.rtltcp.gainMode));
    settings->setValue("RTL-TCP/address", s.rtltcp.tcpAddress);
    settings->setValue("RTL-TCP/port", s.rtltcp.tcpPort);
    settings->setValue("RTL-TCP/agcLevelMax", s.rtltcp.agcLevelMax);

    settings->setValue("RAW-FILE/filename", s.rawfile.file);
    settings->setValue("RAW-FILE/format", int(s.rawfile.format));
    settings->setValue("RAW-FILE/loop", s.rawfile.loopEna);

    if ((InputDeviceId::RAWFILE != m_inputDeviceId) && (InputDeviceId::UNDEFINED != m_inputDeviceId))
    {   // save current service and service list
        QModelIndex current = ui->serviceListView->currentIndex();
        const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
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
        m_serviceList->save(*settings);
    }
    else { /* RAW file does not store service and service list */ }

    settings->sync();

    delete settings;
}

void MainWindow::onFavoriteToggled(bool checked)
{
    QModelIndex current = ui->serviceListView->currentIndex();
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListId id = model->id(current);
    m_serviceList->setServiceFavorite(id, checked);

    m_slModel->sort(0);

    // find new position of current service and select it
    QModelIndex index;
    for (int r = 0; r < model->rowCount(); ++r)
    {
        index = model->index(r, 0);
        if (model->id(index) == id)
        {   // found
            ui->serviceListView->setCurrentIndex(index);
            ui->serviceListView->setFocus();
            break;
        }
    }
}

void MainWindow::onAudioVolumeSliderChanged(int volume)
{
    if (!m_muteLabel->isChecked())
    {
        m_audioVolume = volume;
        emit audioVolume(volume);
    }
    else { /* we are muted -> no volume change for user is possible */ }
}

void MainWindow::onMuteLabelToggled(bool doMute)
{
    m_audioVolumeSlider->setDisabled(doMute);
    if (doMute)
    {   // store current volume
        m_audioVolume = m_audioVolumeSlider->value();
        m_audioVolumeSlider->setValue(0);  // --> this emits signal
    }
    else
    {
        m_audioVolumeSlider->setValue(m_audioVolume);
    }
    emit audioMute(doMute);
}

void MainWindow::onSwitchSourceClicked()
{
    QModelIndex current = ui->serviceListView->currentIndex();
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListId id = model->id(current);
    ServiceListConstIterator it = m_serviceList->findService(id);
    if (it != m_serviceList->serviceListEnd())
    {
        // swith to next ensemble & get ensemble frequency
        uint32_t newFrequency = (*it)->switchEnsemble(ServiceListId())->frequency();
        if (newFrequency)
        {
            if (newFrequency != m_frequency)
            {
               m_frequency = newFrequency;

                // change combo
                ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(m_frequency));

                // set UI to new channel tuning
                channelSelected();
            }
            m_frequency = newFrequency;
            serviceSelected();
            emit serviceRequest(m_frequency, m_SId.value(), m_SCIdS);

            // synchronize tree view with service selection
            serviceListViewUpdateSelection();
            serviceTreeViewUpdateSelection();
        }
    }
}

void MainWindow::onAnnouncementClicked()
{
    ui->announcementLabel->setEnabled(false);
    emit toggleAnnouncement();
}

void MainWindow::serviceTreeViewUpdateSelection()
{
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(ui->serviceTreeView->model());
    ServiceListId serviceId(m_SId.value(), uint8_t(m_SCIdS));
    ServiceListId ensembleId;
    ServiceListConstIterator it = m_serviceList->findService(serviceId);
    if (m_serviceList->serviceListEnd() != it)
    {  // found
        ensembleId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->id();
    }
    for (int r = 0; r < model->rowCount(); ++r)
    {   // go through ensembles
        QModelIndex ensembleIndex = model->index(r, 0);
        if (model->id(ensembleIndex) == ensembleId)
        {   // found ensemble
            for (int s = 0; s < model->rowCount(ensembleIndex); ++s)
            {   // go through services
                QModelIndex serviceIndex = model->index(s, 0, ensembleIndex);
                if (model->id(serviceIndex) == serviceId)
                {  // found
                    ui->serviceTreeView->setCurrentIndex(serviceIndex);
                    return;
                }
            }
        }
    }
}

void MainWindow::serviceListViewUpdateSelection()
{
    const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
    ServiceListId id(m_SId.value(), uint8_t(m_SCIdS));
    QModelIndex index;
    for (int r = 0; r < model->rowCount(); ++r)
    {
        index = model->index(r, 0);
        if (model->id(index) == id)
        {   // found
            ui->serviceListView->setCurrentIndex(index);
            return;
        }
    }
}

void MainWindow::toggleDLPlus(bool toggle)
{
    ui->dlPlusWidget->setVisible(toggle && m_setupDialog->settings().dlPlusEna);
}

void MainWindow::showEnsembleInfo()
{
    m_ensembleInfoDialog->show();
    m_ensembleInfoDialog->raise();
    m_ensembleInfoDialog->activateWindow();
}

void MainWindow::showEPG()
{
    m_epgDialog->show();
    m_epgDialog->raise();
    m_epgDialog->activateWindow();
}

void MainWindow::showAboutDialog()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::showSetupDialog()
{
    m_setupDialog->show();
    m_setupDialog->raise();
    m_setupDialog->activateWindow();
}

void MainWindow::showLog()
{
    m_logDialog->show();
    m_logDialog->raise();
    m_logDialog->activateWindow();
}

void MainWindow::showCatSLS()
{
    m_catSlsDialog->show();
    m_catSlsDialog->raise();
    m_catSlsDialog->activateWindow();
}

void MainWindow::onExpertModeToggled(bool checked)
{
    setExpertMode(checked);
    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void MainWindow::setExpertMode(bool ena)
{
    ui->channelFrame->setVisible(ena);
    ui->audioFrame->setVisible(ena);
    ui->programTypeLabel->setVisible(ena);

    ui->serviceTreeView->setVisible(ena);
    m_signalQualityWidget->setVisible(ena);
    if (m_timeBasicQualInfoWidget->currentIndex() != 2)
    {   // if not information
        m_timeBasicQualInfoWidget->setCurrentIndex(ena ? 1 : 0);
    }

    ui->audioFrame->setVisible(ena);
    ui->channelFrame->setVisible(ena);
    ui->channelDown->setVisible(ena);
    ui->channelUp->setVisible(ena);
    ui->programTypeLabel->setVisible(ena);
    ui->ensembleInfoLabel->setVisible(ena);

    ui->slsView_Service->setExpertMode(ena);
    ui->slsView_Announcement->setExpertMode(ena);
    m_catSlsDialog->setExpertMode(ena);

    // set tab order
    if (ena)
    {
        setTabOrder(ui->serviceListView, ui->serviceTreeView);
        setTabOrder(ui->serviceTreeView, m_audioVolumeSlider);
        setTabOrder(m_audioVolumeSlider, ui->channelCombo);
    }
    else
    {
        setTabOrder(ui->serviceListView, m_audioVolumeSlider);
    }

    emit expertModeChanged(ena);
}

void MainWindow::bandScan()
{
    BandScanDialog * dialog = new BandScanDialog(this, (m_serviceList->numServices() == 0) || m_keepServiceListOnScan, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    connect(dialog, &BandScanDialog::finished, dialog, &QObject::deleteLater);
    connect(dialog, &BandScanDialog::tuneChannel, this, &MainWindow::onTuneChannel);
    connect(m_radioControl, &RadioControl::signalState, dialog, &BandScanDialog::onSyncStatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, dialog, &BandScanDialog::onEnsembleFound, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, dialog, &BandScanDialog::onTuneDone, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, dialog, &BandScanDialog::onServiceListComplete, Qt::QueuedConnection);
    connect(m_serviceList, &ServiceList::serviceAdded, dialog, &BandScanDialog::onServiceFound);
    connect(dialog, &BandScanDialog::scanStarts, this, &MainWindow::onBandScanStart);
    connect(dialog, &BandScanDialog::finished, this, &MainWindow::onBandScanFinished);

    dialog->open();
}


void MainWindow::onBandScanFinished(int result)
{
    switch (result)
    {
    case BandScanDialogResult::Done:
    case BandScanDialogResult::Interrupted:
    {
        if (m_serviceList->numServices() > 0)
        {   // going to select first service
            const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
            //ui->serviceListView->setSe
            QModelIndex index = model->index(0, 0);
            ui->serviceListView->setCurrentIndex(index);
            ui->serviceListView->setFocus();
        }
    }
        break;
    case BandScanDialogResult::Cancelled:
        // do nothing
        break;
    }
}

void MainWindow::onTuneChannel(uint32_t freq)
{
    // change combo - find combo index
    ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(freq));
}

void MainWindow::stop()
{
    if (m_isPlaying)
    { // stop
        emit stopAudioRecording();
        m_audioRecordingActive = false;

        // tune to 0
        ui->channelCombo->setCurrentIndex(-1);
    }
}

void MainWindow::clearServiceList()
{
    stop();
    m_serviceList->clear();
}

void MainWindow::onApplicationStyleChanged(ApplicationStyle style)
{
    switch(style)
    {
    case ApplicationStyle::Default:
#ifndef Q_OS_LINUX
        qApp->setStyle(QStyleFactory::create(m_defaultStyleName));
        qApp->setPalette(qApp->style()->standardPalette());
        qApp->setStyleSheet("");
        setupDarkMode();
        break;
#endif
    case ApplicationStyle::Light:
    case ApplicationStyle::Dark:
        qApp->setStyle(QStyleFactory::create("Fusion"));

        QSizePolicy sizePolicy = ui->scrollArea->sizePolicy();
        sizePolicy.setVerticalStretch(1);
        ui->scrollArea->setSizePolicy(sizePolicy);

        sizePolicy = ui->slsWidget->sizePolicy();
        sizePolicy.setVerticalStretch(1);
        ui->slsWidget->setSizePolicy(sizePolicy);

        forceDarkStyle(ApplicationStyle::Dark == style);

        break;
    }
}

void MainWindow::initStyle()
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#if defined(Q_OS_WIN)
    // this forces Fusion on Windows
    m_defaultStyleName = "Fusion";
#else
    m_defaultStyleName = qApp->style()->name();
#endif
#else
    m_defaultStyleName = qApp->style()->name();
#endif
    QPalette * palette = &m_palette;

    // standardPalette Qt::ColorScheme::Light
    palette->setColor(QPalette::Active, QPalette::WindowText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::Button, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Active, QPalette::Light, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Active, QPalette::Midlight, QColor(202, 202, 202, 255));
    palette->setColor(QPalette::Active, QPalette::Dark, QColor(159, 159, 159, 255));
    palette->setColor(QPalette::Active, QPalette::Mid, QColor(184, 184, 184, 255));
    palette->setColor(QPalette::Active, QPalette::Text, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::BrightText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Active, QPalette::ButtonText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::Base, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Active, QPalette::Window, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Active, QPalette::Shadow, QColor(118, 118, 118, 255));
    palette->setColor(QPalette::Active, QPalette::Highlight, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Active, QPalette::HighlightedText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Active, QPalette::Link, QColor(0, 0, 255, 255));
    palette->setColor(QPalette::Active, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Active, QPalette::AlternateBase, QColor(247, 247, 247, 255));
    palette->setColor(QPalette::Active, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Active, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::PlaceholderText, QColor(0, 0, 0, 128));
    palette->setColor(QPalette::Disabled, QPalette::WindowText, QColor(190, 190, 190, 255));
    palette->setColor(QPalette::Disabled, QPalette::Button, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Disabled, QPalette::Light, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::Midlight, QColor(202, 202, 202, 255));
    palette->setColor(QPalette::Disabled, QPalette::Dark, QColor(190, 190, 190, 255));
    palette->setColor(QPalette::Disabled, QPalette::Mid, QColor(184, 184, 184, 255));
    palette->setColor(QPalette::Disabled, QPalette::Text, QColor(190, 190, 190, 255));
    palette->setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::ButtonText, QColor(190, 190, 190, 255));
    palette->setColor(QPalette::Disabled, QPalette::Base, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Disabled, QPalette::Window, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Disabled, QPalette::Shadow, QColor(177, 177, 177, 255));
    palette->setColor(QPalette::Disabled, QPalette::Highlight, QColor(145, 145, 145, 255));
    palette->setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::Link, QColor(0, 0, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(247, 247, 247, 255));
    palette->setColor(QPalette::Disabled, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(0, 0, 0, 128));
    palette->setColor(QPalette::Inactive, QPalette::WindowText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::Button, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Inactive, QPalette::Light, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::Midlight, QColor(202, 202, 202, 255));
    palette->setColor(QPalette::Inactive, QPalette::Dark, QColor(159, 159, 159, 255));
    palette->setColor(QPalette::Inactive, QPalette::Mid, QColor(184, 184, 184, 255));
    palette->setColor(QPalette::Inactive, QPalette::Text, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::BrightText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::ButtonText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::Base, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::Window, QColor(239, 239, 239, 255));
    palette->setColor(QPalette::Inactive, QPalette::Shadow, QColor(118, 118, 118, 255));
    palette->setColor(QPalette::Inactive, QPalette::Highlight, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(255, 255, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::Link, QColor(0, 0, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::AlternateBase, QColor(247, 247, 247, 255));
    palette->setColor(QPalette::Inactive, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Inactive, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::PlaceholderText, QColor(0, 0, 0, 128));

    palette = &m_darkPalette;

    // standardPalette Qt::ColorScheme::Dark
    palette->setColor(QPalette::Active, QPalette::WindowText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Active, QPalette::Button, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Active, QPalette::Light, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Active, QPalette::Midlight, QColor(42, 42, 42, 255));
    palette->setColor(QPalette::Active, QPalette::Dark, QColor(33, 33, 33, 255));
    palette->setColor(QPalette::Active, QPalette::Mid, QColor(38, 38, 38, 255));
    palette->setColor(QPalette::Active, QPalette::Text, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Active, QPalette::BrightText, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Active, QPalette::ButtonText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Active, QPalette::Base, QColor(36, 36, 36, 255));
    palette->setColor(QPalette::Active, QPalette::Window, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Active, QPalette::Shadow, QColor(25, 25, 25, 255));
    palette->setColor(QPalette::Active, QPalette::Highlight, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Active, QPalette::HighlightedText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Active, QPalette::Link, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Active, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Active, QPalette::AlternateBase, QColor(43, 43, 43, 255));
    palette->setColor(QPalette::Active, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Active, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Active, QPalette::PlaceholderText, QColor(240, 240, 240, 128));
    palette->setColor(QPalette::Disabled, QPalette::WindowText, QColor(130, 130, 130, 255));
    palette->setColor(QPalette::Disabled, QPalette::Button, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Disabled, QPalette::Light, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Disabled, QPalette::Midlight, QColor(42, 42, 42, 255));
    palette->setColor(QPalette::Disabled, QPalette::Dark, QColor(190, 190, 190, 255));
    palette->setColor(QPalette::Disabled, QPalette::Mid, QColor(38, 38, 38, 255));
    palette->setColor(QPalette::Disabled, QPalette::Text, QColor(130, 130, 130, 255));
    palette->setColor(QPalette::Disabled, QPalette::BrightText, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Disabled, QPalette::ButtonText, QColor(130, 130, 130, 255));
    palette->setColor(QPalette::Disabled, QPalette::Base, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Disabled, QPalette::Window, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Disabled, QPalette::Shadow, QColor(37, 37, 37, 255));
    palette->setColor(QPalette::Disabled, QPalette::Highlight, QColor(145, 145, 145, 255));
    palette->setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Disabled, QPalette::Link, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(43, 43, 43, 255));
    palette->setColor(QPalette::Disabled, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(240, 240, 240, 128));
    palette->setColor(QPalette::Inactive, QPalette::WindowText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Inactive, QPalette::Button, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Inactive, QPalette::Light, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Inactive, QPalette::Midlight, QColor(42, 42, 42, 255));
    palette->setColor(QPalette::Inactive, QPalette::Dark, QColor(33, 33, 33, 255));
    palette->setColor(QPalette::Inactive, QPalette::Mid, QColor(38, 38, 38, 255));
    palette->setColor(QPalette::Inactive, QPalette::Text, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Inactive, QPalette::BrightText, QColor(75, 75, 75, 255));
    palette->setColor(QPalette::Inactive, QPalette::ButtonText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Inactive, QPalette::Base, QColor(36, 36, 36, 255));
    palette->setColor(QPalette::Inactive, QPalette::Window, QColor(50, 50, 50, 255));
    palette->setColor(QPalette::Inactive, QPalette::Shadow, QColor(25, 25, 25, 255));
    palette->setColor(QPalette::Inactive, QPalette::Highlight, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(240, 240, 240, 255));
    palette->setColor(QPalette::Inactive, QPalette::Link, QColor(48, 140, 198, 255));
    palette->setColor(QPalette::Inactive, QPalette::LinkVisited, QColor(255, 0, 255, 255));
    palette->setColor(QPalette::Inactive, QPalette::AlternateBase, QColor(43, 43, 43, 255));
    palette->setColor(QPalette::Inactive, QPalette::NoRole, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(255, 255, 220, 255));
    palette->setColor(QPalette::Inactive, QPalette::ToolTipText, QColor(0, 0, 0, 255));
    palette->setColor(QPalette::Inactive, QPalette::PlaceholderText, QColor(240, 240, 240, 128));

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::onColorSchemeChanged);
#endif
}

void MainWindow::restoreTimeQualWidget()
{
    m_timeBasicQualInfoWidget->setCurrentIndex(m_setupDialog->settings().expertModeEna ? 1 : 0);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
// this is used to detect that system colors where changed (e.g. switching light and dark mode)
void MainWindow::onColorSchemeChanged(Qt::ColorScheme colorScheme)
{
    // this is required here to keep consistency when forced styles have been used before
    // palette should not be touched in case that only automatic dark mode switching is supported
    if (ApplicationStyle::Default == m_setupDialog->settings().applicationStyle)
    {
        qApp->setPalette(qApp->style()->standardPalette());
#ifdef Q_OS_WIN
        // this hack forces correct change for the colors on Windows
        if (colorScheme == Qt::ColorScheme::Dark)
        {
            onApplicationStyleChanged(ApplicationStyle::Dark);
        }
        else
        {
            onApplicationStyleChanged(ApplicationStyle::Light);
        }
        onApplicationStyleChanged(ApplicationStyle::Default);
#endif
    }
    setupDarkMode();

#if 0
    // this is used to get default and dark palette
    // output used to force light and dark theme
    //QPalette palette = qApp->palette();
    QPalette palette = qApp->style()->standardPalette();

    qDebug() << "// standardPalette" << qApp->styleHints()->colorScheme();

    for (int g = QPalette::Active; g <= QPalette::Inactive; ++g) {
        for (int r = QPalette::WindowText; r <= QPalette::PlaceholderText; ++r)
        {
            const QPalette::ColorRole role = static_cast<const QPalette::ColorRole>(r);
            const QPalette::ColorGroup group = static_cast<const QPalette::ColorGroup>(g);

            QString line = QString("palette->setColor(QPalette::%1, QPalette::%2, QColor(%3, %4, %5, %6));")
                               .arg(QVariant::fromValue(group).toString())
                               .arg(QVariant::fromValue(role).toString())
                               .arg(palette.color(group, role).red())
                               .arg(palette.color(group, role).green())
                               .arg(palette.color(group, role).blue())
                               .arg(palette.color(group, role).alpha());

            qDebug("%s", line.toLatin1().data());
        }
    }

#endif
}

#else // QT version < 6.5.0 -> switching is supported only on MacOS
void MainWindow::changeEvent( QEvent* e )
{
#ifdef Q_OS_MACX
    if ( e->type() == QEvent::PaletteChange )
    {
        setupDarkMode();
    }
#endif
    QMainWindow::changeEvent( e );
}
#endif


bool MainWindow::isDarkMode()
{
    if (ApplicationStyle::Dark == m_setupDialog->settings().applicationStyle)
    {
        return true;
    }
    // else
    if (ApplicationStyle::Light == m_setupDialog->settings().applicationStyle)
    {
        return false;
    }
    // else
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return (Qt::ColorScheme::Dark == qApp->styleHints()->colorScheme());
#else // QT < 6.5.0
#ifdef Q_OS_MACX
    return macIsInDarkTheme();
#else
    return false;
#endif
#endif
}

void MainWindow::forceDarkStyle(bool ena)
{
    if (ena)
    {
        qApp->setPalette(m_darkPalette);
        qApp->setStyleSheet("QToolTip { color: rgba(230, 230, 230, 240); "
                            "background-color: rgba(100, 100, 100, 160); "
                            "border: 1px rgba(0, 0, 0, 128); }");    }
    else
    {
        qApp->setPalette(m_palette);
        qApp->setStyleSheet("QToolTip { color: rgba(50, 50, 50, 240); "
                            "background-color: rgba(255, 255, 255, 160); "
                            "border: 1px rgba(0, 0, 0, 128); }");
    }
    setupDarkMode();
}

void MainWindow::setupDarkMode()
{
    if (isDarkMode())
    {
        m_menuLabel->setIcon(":/resources/menu_dark.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty_dark.png", false);
        ui->favoriteLabel->setIcon(":/resources/star_dark.png", true);

        m_muteLabel->setIcon(":/resources/volume_on_dark.png", false);
        m_muteLabel->setIcon(":/resources/volume_off_dark.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls_dark.png");

        ui->announcementLabel->setIcon(":/resources/announcement_active_dark.png", true);
        ui->announcementLabel->setIcon(":/resources/announcement_suspended_dark.png", false);

        ui->switchSourceLabel->setIcon(":/resources/broadcast_dark.png");

        ui->channelDown->setIcon(":/resources/chevron-left_dark.png");
        ui->channelUp->setIcon(":/resources/chevron-right_dark.png");

        m_audioRecordingLabel->setIcon(":/resources/record.png");

        ui->slsView_Service->setupDarkMode(true);
        ui->slsView_Announcement->setupDarkMode(true);
        m_logDialog->setupDarkMode(true);
        m_epgDialog->setupDarkMode(true);
    }
    else
    {
        m_menuLabel->setIcon(":/resources/menu.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty.png", false);
        ui->favoriteLabel->setIcon(":/resources/star.png", true);

        m_muteLabel->setIcon(":/resources/volume_on.png", false);
        m_muteLabel->setIcon(":/resources/volume_off.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls.png");

        ui->announcementLabel->setIcon(":/resources/announcement_active.png", true);
        ui->announcementLabel->setIcon(":/resources/announcement_suspended.png", false);

        ui->switchSourceLabel->setIcon(":/resources/broadcast.png");

        ui->channelDown->setIcon(":/resources/chevron-left.png");
        ui->channelUp->setIcon(":/resources/chevron-right.png");

        m_audioRecordingLabel->setIcon(":/resources/record.png");

        ui->slsView_Service->setupDarkMode(false);
        ui->slsView_Announcement->setupDarkMode(false);
        m_logDialog->setupDarkMode(false);
        m_epgDialog->setupDarkMode(false);
    }
}

void MainWindow::onDLPlusObjReceived_Service(const DLPlusObject & object)
{
    onDLPlusObjReceived(object, Instance::Service);
}

void MainWindow::onDLPlusObjReceived_Announcement(const DLPlusObject & object)
{
    onDLPlusObjReceived(object, Instance::Announcement);
}

void MainWindow::onDLPlusObjReceived(const DLPlusObject & object, Instance inst)
{
    QMap<DLPlusContentType, DLPlusObjectUI*> * dlObjCachePtr = &m_dlObjCache[inst];
    QWidget * dlPlusFrame = ui->dlPlusFrame_Service;
    if (Instance::Service != inst)
    {
        dlPlusFrame = ui->dlPlusFrame_Announcement;
    }
    else
    { /* Service - already initialized */ }

    if (DLPlusContentType::DUMMY == object.getType())
    {   // dummy object = do nothing
        // or
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        return;
    }
    else
    { /* no dummy object */ }

    toggleDLPlus(true);

    if (object.isDelete())
    {   // [ETSI TS 102 980 V2.1.2] 5.3.2 Deleting objects
        // Objects are deleted by transmitting a "delete" object
        const auto it = dlObjCachePtr->constFind(object.getType());
        if (it != dlObjCachePtr->cend())
        {   // object type found in cache
            DLPlusObjectUI* uiObj = dlObjCachePtr->take(object.getType());
            delete uiObj;
        }
        else
        { /* not in cache, do nothing */ }
    }
    else
    {
        auto it = dlObjCachePtr->find(object.getType());
        if (it != dlObjCachePtr->end())
        {   // object type found in cahe
            (*it)->update(object);
        }
        else
        {   // not in cache yet -> insert
            DLPlusObjectUI * uiObj = new DLPlusObjectUI(object);
            if (nullptr != uiObj->getLayout())
            {
                it = dlObjCachePtr->insert(object.getType(), uiObj);

                // we want it sorted -> need to find the position
                int index = std::distance(dlObjCachePtr->begin(), it);

                QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(dlPlusFrame->layout());
                layout->insertLayout(index, uiObj->getLayout());
                //dlPlusFrame->setMaximumHeight(dlPlusFrame->minimumHeight() > 120 ? dlPlusFrame->minimumHeight() : 120);
            }
            else
            { /* objects that we do not display: Descriptors, PROGRAMME_FREQUENCY, INFO_DATE_TIME, PROGRAMME_SUBCHANNEL*/ }
        }
    }
}

void MainWindow::onDLPlusItemToggle_Service()
{
    onDLPlusItemToggle(Instance::Service);
}

void MainWindow::onDLPlusItemToggle_Announcement()
{
    onDLPlusItemToggle(Instance::Announcement);
}

void MainWindow::onDLPlusItemToggle(Instance inst)
{
    QMap<DLPlusContentType, DLPlusObjectUI*> * dlObjCachePtr = &m_dlObjCache[inst];

    // delete all ITEMS.*
    auto it = dlObjCachePtr->cbegin();
    while (it != dlObjCachePtr->cend())
    {
        if (it.key() < DLPlusContentType::INFO_NEWS)
        {
            DLPlusObjectUI* uiObj = it.value();
            delete uiObj;
            it = dlObjCachePtr->erase(it);
        }
        else
        {   // no more items ot ITEM type in cache
            break;
        }
    }
}

void MainWindow::onDLPlusItemRunning_Service(bool isRunning)
{
    onDLPlusItemRunning(isRunning, Instance::Service);
}

void MainWindow::onDLPlusItemRunning_Announcement(bool isRunning)
{
    onDLPlusItemRunning(isRunning, Instance::Announcement);
}

void MainWindow::onDLPlusItemRunning(bool isRunning, Instance inst)
{
    QMap<DLPlusContentType, DLPlusObjectUI*> * dlObjCachePtr = &m_dlObjCache[inst];

    auto it = dlObjCachePtr->cbegin();
    while (it != dlObjCachePtr->cend())
    {
        if (it.key() < DLPlusContentType::INFO_NEWS)
        {
            it.value()->setVisible(isRunning);
            ++it;
        }
        else
        {   // no more items ot ITEM type in cache
            break;
        }
    }
}

void MainWindow::onDLReset_Service()
{
    ui->dynamicLabel_Service->clear();
    toggleDLPlus(false);

    for (auto objPtr : m_dlObjCache[Instance::Service])
    {
        delete objPtr;
    }
    m_dlObjCache[Instance::Service].clear();
}

void MainWindow::onDLReset_Announcement()
{
    ui->dynamicLabel_Announcement->clear();
    toggleDLPlus(false);

    for (auto objPtr : m_dlObjCache[Instance::Announcement])
    {
        delete objPtr;
    }
    m_dlObjCache[Instance::Announcement].clear();
}

DLPlusObjectUI::DLPlusObjectUI(const DLPlusObject &obj) : m_dlPlusObject(obj)
{
    m_layout = new QHBoxLayout();
    QString label = getLabel(obj.getType());
    if (label.isEmpty())
    {
        m_layout = nullptr;
    }
    else
    {
        m_tagLabel = new QLabel(QString("<b>%1:  </b>").arg(label));
        m_tagLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
        m_tagLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_layout->addWidget(m_tagLabel);


        switch (obj.getType())
        {
        case DLPlusContentType::PROGRAMME_HOMEPAGE:
        case DLPlusContentType::INFO_URL:
            m_tagText = new QLabel(QString("<a href=\"%1\">%1</a>").arg(obj.getTag()));
            m_tagText->setToolTip(QString(QObject::tr("Open link")));
            QObject::connect(
                        m_tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        case DLPlusContentType::EMAIL_HOTLINE:
        case DLPlusContentType::EMAIL_STUDIO:
        case DLPlusContentType::EMAIL_OTHER:
            m_tagText = new QLabel(QString("<a href=\"mailto:%1\">%1</a>").arg(obj.getTag()));
            m_tagText->setToolTip(QString(QObject::tr("Open link")));
            QObject::connect(
                        m_tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        default:
            m_tagText = new QLabel(obj.getTag());
        }

        //m_tagText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        m_tagText->setWordWrap(true);
        m_tagText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_layout->addWidget(m_tagText);
    }
}

DLPlusObjectUI::~DLPlusObjectUI()
{

    if (nullptr != m_layout)
    {
        delete m_tagLabel;
        delete m_tagText;
        delete m_layout;
    }
}

QHBoxLayout *DLPlusObjectUI::getLayout() const
{
    return m_layout;
}

void DLPlusObjectUI::update(const DLPlusObject &obj)
{
    if (obj != m_dlPlusObject)
    {
        m_dlPlusObject = obj;
        m_tagText->setText(obj.getTag());
    }
}

void DLPlusObjectUI::setVisible(bool visible)
{
    m_tagLabel->setVisible(visible);
    m_tagText->setVisible(visible);
}

const DLPlusObject &DLPlusObjectUI::getDlPlusObject() const
{
    return m_dlPlusObject;
}

QString DLPlusObjectUI::getLabel(DLPlusContentType type) const
{
    QString label;

    switch (type)
    {
    //case DLPlusContentType::DUMMY: label = "DUMMY"; break;
    case DLPlusContentType::ITEM_TITLE: label = QObject::tr("Title"); break;
    case DLPlusContentType::ITEM_ALBUM: label = QObject::tr("Album"); break;
    case DLPlusContentType::ITEM_TRACKNUMBER: label = QObject::tr("Track Number"); break;
    case DLPlusContentType::ITEM_ARTIST: label = QObject::tr("Artist"); break;
    case DLPlusContentType::ITEM_COMPOSITION: label = QObject::tr("Composition"); break;
    case DLPlusContentType::ITEM_MOVEMENT: label = QObject::tr("Movement"); break;
    case DLPlusContentType::ITEM_CONDUCTOR: label = QObject::tr("Conductor"); break;
    case DLPlusContentType::ITEM_COMPOSER: label = QObject::tr("Composer"); break;
    case DLPlusContentType::ITEM_BAND: label = QObject::tr("Band"); break;
    case DLPlusContentType::ITEM_COMMENT: label = QObject::tr("Comment"); break;
    case DLPlusContentType::ITEM_GENRE: label = QObject::tr("Genre"); break;
    case DLPlusContentType::INFO_NEWS: label = QObject::tr("News"); break;
    case DLPlusContentType::INFO_NEWS_LOCAL: label = QObject::tr("News (local)"); break;
    case DLPlusContentType::INFO_STOCKMARKET: label = QObject::tr("Stock Market"); break;
    case DLPlusContentType::INFO_SPORT: label = QObject::tr("Sport"); break;
    case DLPlusContentType::INFO_LOTTERY: label = QObject::tr("Lottery"); break;
    case DLPlusContentType::INFO_HOROSCOPE: label = QObject::tr("Horoscope"); break;
    case DLPlusContentType::INFO_DAILY_DIVERSION: label = QObject::tr("Daily Diversion"); break;
    case DLPlusContentType::INFO_HEALTH: label = QObject::tr("Health"); break;
    case DLPlusContentType::INFO_EVENT: label = QObject::tr("Event"); break;
    case DLPlusContentType::INFO_SCENE: label = QObject::tr("Scene"); break;
    case DLPlusContentType::INFO_CINEMA: label = QObject::tr("Cinema"); break;
    case DLPlusContentType::INFO_TV: label = QObject::tr("TV"); break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::INFO_DATE_TIME: label = "INFO_DATE_TIME"; break;
    case DLPlusContentType::INFO_WEATHER: label = QObject::tr("Weather"); break;
    case DLPlusContentType::INFO_TRAFFIC: label = QObject::tr("Traffic"); break;
    case DLPlusContentType::INFO_ALARM: label = QObject::tr("Alarm"); break;
    case DLPlusContentType::INFO_ADVERTISEMENT: label = QObject::tr("Advertisment"); break;
    case DLPlusContentType::INFO_URL: label = QObject::tr("URL"); break;
    case DLPlusContentType::INFO_OTHER: label = QObject::tr("Other"); break;
    case DLPlusContentType::STATIONNAME_SHORT: label = QObject::tr("Station (short)"); break;
    case DLPlusContentType::STATIONNAME_LONG: label = QObject::tr("Station"); break;
    case DLPlusContentType::PROGRAMME_NOW: label = QObject::tr("Now"); break;
    case DLPlusContentType::PROGRAMME_NEXT: label = QObject::tr("Next"); break;
    case DLPlusContentType::PROGRAMME_PART: label = QObject::tr("Programme Part"); break;
    case DLPlusContentType::PROGRAMME_HOST: label = QObject::tr("Host"); break;
    case DLPlusContentType::PROGRAMME_EDITORIAL_STAFF: label = QObject::tr("Editorial"); break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::PROGRAMME_FREQUENCY: label = "Frequency"; break;
    case DLPlusContentType::PROGRAMME_HOMEPAGE: label = QObject::tr("Homepage"); break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::PROGRAMME_SUBCHANNEL: label = "Subchannel"; break;
    case DLPlusContentType::PHONE_HOTLINE: label = QObject::tr("Phone (Hotline)"); break;
    case DLPlusContentType::PHONE_STUDIO: label = QObject::tr("Phone (Studio)"); break;
    case DLPlusContentType::PHONE_OTHER: label = QObject::tr("Phone (Other)"); break;
    case DLPlusContentType::SMS_STUDIO: label = QObject::tr("SMS (Studio)"); break;
    case DLPlusContentType::SMS_OTHER: label = QObject::tr("SMS (Other)"); break;
    case DLPlusContentType::EMAIL_HOTLINE: label = QObject::tr("E-mail (Hotline)"); break;
    case DLPlusContentType::EMAIL_STUDIO: label = QObject::tr("E-mail (Studio)"); break;
    case DLPlusContentType::EMAIL_OTHER: label = QObject::tr("E-mail (Other)"); break;
    case DLPlusContentType::MMS_OTHER: label = QObject::tr("MMS"); break;
    case DLPlusContentType::CHAT: label = QObject::tr("Chat Message"); break;
    case DLPlusContentType::CHAT_CENTER: label = QObject::tr("Chat"); break;
    case DLPlusContentType::VOTE_QUESTION: label = QObject::tr("Vote Question"); break;
    case DLPlusContentType::VOTE_CENTRE: label = QObject::tr("Vote Here"); break;
        // rfu = 54
        // rfu = 55
    case DLPlusContentType::PRIVATE_1: label = QObject::tr("Private 1"); break;
    case DLPlusContentType::PRIVATE_2: label = QObject::tr("Private 2"); break;
    case DLPlusContentType::PRIVATE_3: label = QObject::tr("Private 3"); break;
//    case DLPlusContentType::DESCRIPTOR_PLACE: label = "DESCRIPTOR_PLACE"; break;
//    case DLPlusContentType::DESCRIPTOR_APPOINTMENT: label = "DESCRIPTOR_APPOINTMENT"; break;
//    case DLPlusContentType::DESCRIPTOR_IDENTIFIER: label = "DESCRIPTOR_IDENTIFIER"; break;
//    case DLPlusContentType::DESCRIPTOR_PURCHASE: label = "DESCRIPTOR_PURCHASE"; break;
//    case DLPlusContentType::DESCRIPTOR_GET_DATA: label = "DESCRIPTOR_GET_DATA"; break;
    default:
        break;

    }

    return label;
}
