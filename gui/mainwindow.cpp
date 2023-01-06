#include <QString>
#include <QFileDialog>
#include <QDir>
#include <QMap>
#include <QDebug>
#include <QGraphicsScene>
#include <QToolButton>
#include <QFormLayout>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

#include "mainwindow.h"
#include "slsview.h"
#include "./ui_mainwindow.h"
#include "dabtables.h"
#include "radiocontrol.h"
#include "bandscandialog.h"
#include "config.h"
#include "aboutdialog.h"

// Input devices
#include "rawfileinput.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#if HAVE_RARTTCP
#include "rarttcpinput.h"
#endif
#if HAVE_AIRSPY
#include "airspyinput.h"
#endif
#if HAVE_SOAPYSDR
#include "soapysdrinput.h"
#endif

#ifdef Q_OS_MACX
#include "mac.h"
#endif

const QString MainWindow::appName("AbracaDABra");
const QStringList MainWindow::syncLevelLabels = {"No signal", "Signal found", "Sync"};
const QStringList MainWindow::syncLevelTooltip = {"DAB signal not detected<br>Looking for signal...",
                                      "Found DAB signal,<br>trying to synchronize...",
                                      "Synchronized to DAB signal"};
const QStringList MainWindow::snrProgressStylesheet = {
    QString::fromUtf8("QProgressBar::chunk {background-color: #ff4b4b; }"),  // red
    QString::fromUtf8("QProgressBar::chunk {background-color: #ffb527; }"),  // yellow
    QString::fromUtf8("QProgressBar::chunk {background-color: #5bc214; }")   // green
};

enum class SNR10Threhold
{
    SNR_BAD = 60,
    SNR_GOOD = 90
};

MainWindow::MainWindow(const QString &iniFilename, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_iniFilename(iniFilename)
{
    m_dlDecoder = new DLDecoder[InstanceIdx::NumInstances];

    ui->setupUi(this);
    connect(ui->channelCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onChannelChange);

    // set UI
    setWindowTitle("Abraca DAB Radio");

    // favorites control
    ui->favoriteLabel->setCheckable(true);
    ui->favoriteLabel->setTooltip("Add service to favorites", false);
    ui->favoriteLabel->setTooltip("Remove service from favorites", true);
    ui->favoriteLabel->setChecked(false);

    m_setupDialog = new SetupDialog(this);
    connect(m_setupDialog, &SetupDialog::inputDeviceChanged, this, &MainWindow::changeInputDevice);
    connect(this, &MainWindow::expertModeChanged, m_setupDialog, &SetupDialog::onExpertMode);
    connect(m_setupDialog, &SetupDialog::newSettings, this, &MainWindow::onNewSettings);

    m_ensembleInfoDialog = new EnsembleInfoDialog(this);

    // status bar
    QWidget * widget = new QWidget();
    m_timeLabel = new QLabel("");
    m_timeLabel->setToolTip("DAB time");

    m_basicSignalQualityLabel = new QLabel("");
    m_basicSignalQualityLabel->setToolTip("DAB signal quality");

    m_timeBasicQualWidget = new QStackedWidget;
    m_timeBasicQualWidget->addWidget(m_basicSignalQualityLabel);
    m_timeBasicQualWidget->addWidget(m_timeLabel);

    m_syncLabel = new QLabel();
    m_syncLabel->setAlignment(Qt::AlignRight);

    m_snrProgressbar = new QProgressBar();
    m_snrProgressbar->setMaximum(30*10);
    m_snrProgressbar->setMinimum(0);
    m_snrProgressbar->setFixedWidth(80);
    m_snrProgressbar->setFixedHeight(m_syncLabel->fontInfo().pixelSize());
    m_snrProgressbar->setTextVisible(false);
    m_snrProgressbar->setToolTip(QString("DAB signal SNR"));

    m_snrLabel = new QLabel();
//#ifdef __APPLE__
    int width = m_snrLabel->fontMetrics().boundingRect("100.0 dB").width();
    m_snrLabel->setFixedWidth(width);
//#endif
    m_snrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_snrLabel->setToolTip(QString("DAB signal SNR"));

    QHBoxLayout * signalQualityLayout = new QHBoxLayout();
    signalQualityLayout->addWidget(m_syncLabel);
    signalQualityLayout->addWidget(m_snrProgressbar);
    signalQualityLayout->setAlignment(m_snrProgressbar, Qt::AlignCenter);
    signalQualityLayout->setStretch(0, 100);
    signalQualityLayout->setAlignment(m_syncLabel, Qt::AlignCenter);
    signalQualityLayout->addWidget(m_snrLabel);
    signalQualityLayout->setSpacing(10);
    signalQualityLayout->setContentsMargins(0,0,0,0);
    m_signalQualityWidget = new QWidget();
    m_signalQualityWidget->setLayout(signalQualityLayout);

    onSnrLevel(0);
    onSyncStatus(uint8_t(DabSyncLevel::NoSync));

    m_setupAction = new QAction("Setup...", this);
    //setupAct->setStatusTip("Application settings");       // this is shown in status bar
    connect(m_setupAction, &QAction::triggered, this, &MainWindow::showSetupDialog);

    m_clearServiceListAction = new QAction("Clear service list", this);
    connect(m_clearServiceListAction, &QAction::triggered, this, &MainWindow::clearServiceList);

    m_bandScanAction = new QAction("Band scan...", this);
    //scanAct->setStatusTip("Seach for available service"); // this is shown in status bar
    connect(m_bandScanAction, &QAction::triggered, this, &MainWindow::bandScan);

    m_switchModeAction = new QAction("Expert mode", this);
    connect(m_switchModeAction, &QAction::triggered, this, &MainWindow::switchMode);

    m_ensembleInfoAction = new QAction("Ensemble information", this);
    connect(m_ensembleInfoAction, &QAction::triggered, this, &MainWindow::showEnsembleInfo);

    m_aboutAction = new QAction("About", this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    m_menu = new QMenu(this);
    m_menu->addAction(m_setupAction);
    m_menu->addAction(m_bandScanAction);
    m_menu->addAction(m_switchModeAction);
    m_menu->addAction(m_ensembleInfoAction);
    m_menu->addAction(m_clearServiceListAction);
    m_menu->addAction(m_aboutAction);

    m_settingsLabel = new ClickableLabel(this);
    m_settingsLabel->setToolTip("Open menu");
    m_settingsLabel->setMenu(m_menu);

    m_muteLabel = new ClickableLabel(this);
    m_muteLabel->setCheckable(true);
    m_muteLabel->setTooltip("Mute audio", false);
    m_muteLabel->setTooltip("Unmute audio", true);
    m_muteLabel->setChecked(false);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setSingleStep(10);
    m_volumeSlider->setToolTip("Audio volume");
#ifdef Q_OS_WIN
    m_volumeSlider->setMaximumHeight(15);
#endif
    connect(m_muteLabel, &ClickableLabel::toggled, m_volumeSlider, &QSlider::setDisabled);

    QHBoxLayout * volumeLayout = new QHBoxLayout();
    volumeLayout->addWidget(m_muteLabel);
    volumeLayout->addWidget(m_volumeSlider);
    volumeLayout->setAlignment(m_volumeSlider, Qt::AlignCenter);
    volumeLayout->setStretch(0, 100);
    volumeLayout->setAlignment(m_muteLabel, Qt::AlignCenter);
    volumeLayout->setSpacing(10);
    volumeLayout->setContentsMargins(10,0,0,0);
    QWidget * volumeWidget = new QWidget();
    volumeWidget->setLayout(volumeLayout);

    //DL+
    ui->dlPlusLabel->setCheckable(true);
    ui->dlPlusLabel->setTooltip("Show DL+", false);
    ui->dlPlusLabel->setTooltip("Hide DL+", true);
    ui->dlPlusLabel->setChecked(false);
    ui->dlPlusWidget->setVisible(false);

    QSizePolicy sp_retain = ui->dlPlusLabel->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    ui->dlPlusLabel->setSizePolicy(sp_retain);

    ui->dlPlusLabel->setHidden(true);
    connect(ui->dlPlusLabel, &ClickableLabel::toggled, this, &MainWindow::onDLPlusToggled);

    QGridLayout * layout = new QGridLayout(widget);
    layout->addWidget(m_timeBasicQualWidget, 0, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(m_signalQualityWidget, 0, 1, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(volumeWidget, 0, 2, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(m_settingsLabel, 0, 3, Qt::AlignVCenter | Qt::AlignRight);

    layout->setColumnStretch(0, 100);
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

    m_slModel = new SLModel(m_serviceList, this);
    connect(m_serviceList, &ServiceList::serviceAdded, m_slModel, &SLModel::addService);
    connect(m_serviceList, &ServiceList::serviceUpdated, m_slModel, &SLModel::updateService);
    connect(m_serviceList, &ServiceList::serviceRemoved, m_slModel, &SLModel::removeService);
    connect(m_serviceList, &ServiceList::empty, m_slModel, &SLModel::clear);

    ui->serviceListView->setModel(m_slModel);
    ui->serviceListView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceListView->installEventFilter(this);
    connect(ui->serviceListView, &QListView::clicked, this, &MainWindow::onServiceListClicked);

    m_slTreeModel = new SLTreeModel(m_serviceList, this);
    connect(m_serviceList, &ServiceList::serviceAddedToEnsemble, m_slTreeModel, &SLTreeModel::addEnsembleService);
    connect(m_serviceList, &ServiceList::serviceUpdatedInEnsemble, m_slTreeModel, &SLTreeModel::updateEnsembleService);
    connect(m_serviceList, &ServiceList::serviceRemovedFromEnsemble, m_slTreeModel, &SLTreeModel::removeEnsembleService);
    connect(m_serviceList, &ServiceList::ensembleRemoved, m_slTreeModel, &SLTreeModel::removeEnsemble);
    ui->serviceTreeView->setModel(m_slTreeModel);
    ui->serviceTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceTreeView->setHeaderHidden(true);
    ui->serviceTreeView->installEventFilter(this);
    connect(ui->serviceTreeView, &QTreeView::clicked, this, &MainWindow::onServiceListTreeClicked);
    connect(m_serviceList, &ServiceList::empty, m_slTreeModel, &SLTreeModel::clear);

    // fill channel list
    dabChannelList_t::const_iterator i = DabTables::channelList.constBegin();
    while (i != DabTables::channelList.constEnd()) {
        ui->channelCombo->addItem(i.value(), i.key());
        ++i;
    }
    ui->channelCombo->setCurrentIndex(-1);
    ui->channelCombo->setDisabled(true);

    // disable service list - it is enabled when some valid device is selected1
    ui->serviceListView->setEnabled(false);
    ui->serviceTreeView->setEnabled(false);
    ui->favoriteLabel->setEnabled(false);


    clearEnsembleInformationLabels();
    clearServiceInformationLabels();
    ui->dynamicLabel_Service->clear();
    ui->dynamicLabel_Announcement->clear();

    ui->audioEncodingLabel->setToolTip("Audio coding");

    ui->slsView->setMinimumSize(QSize(322, 242));
    ui->slsView->reset();

    ui->announcementLabel->setToolTip("Ongoing announcement");
    ui->announcementLabel->setHidden(true);

    ui->catSlsLabel->setToolTip("Browse categorized slides");
    ui->catSlsLabel->setHidden(true);

    ui->switchSourceLabel->setToolTip("Change service source (ensemble)");
    ui->switchSourceLabel->setHidden(true);

    ui->serviceTreeView->setVisible(false);

    setIcons();

    resize(minimumSizeHint());

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
        qDebug() << "RadioControl() init failed";
        close();
        qApp->quit();
    }

    m_audioDecoder = new AudioDecoder(nullptr);
    m_audioDecoderThread = new QThread(this);
    m_audioDecoderThread->setObjectName("audioDecoderThr");
    m_audioDecoder->moveToThread(m_audioDecoderThread);
    connect(m_audioDecoderThread, &QThread::finished, m_audioDecoder, &QObject::deleteLater);
    m_audioDecoderThread->start();

    m_audioOutput = new AudioOutput();
#if (!HAVE_PORTAUDIO)
    m_audioOutputThread = new QThread(this);
    m_audioOutputThread->setObjectName("audioOutThr");
    m_audioOutputThread->moveToThread(m_audioOutputThread);
    connect(m_audioOutputThread, &QThread::finished, m_audioOutput, &QObject::deleteLater);
    m_audioOutputThread->start();
#endif
    connect(m_volumeSlider, &QSlider::valueChanged, m_audioOutput, &AudioOutput::setVolume, Qt::QueuedConnection);

    // Connect signals
    connect(m_muteLabel, &ClickableLabel::toggled, m_audioOutput, &AudioOutput::mute, Qt::QueuedConnection);
    connect(ui->favoriteLabel, &ClickableLabel::toggled, this, &MainWindow::onFavoriteToggled);
    connect(ui->switchSourceLabel, &ClickableLabel::clicked, this, &MainWindow::onSwitchSourceClicked);
    connect(ui->catSlsLabel, &ClickableLabel::clicked, this, &MainWindow::showCatSLS);

    connect(m_radioControl, &RadioControl::ensembleInformation, this, &MainWindow::onEnsembleInfo, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleReconfiguration, this, &MainWindow::onEnsembleReconfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, this, &MainWindow::onServiceListComplete, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::syncStatus, this, &MainWindow::onSyncStatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::snrLevel, this, &MainWindow::onSnrLevel, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dabTime, this, &MainWindow::onDabTime, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListEntry, this, &MainWindow::onServiceListEntry, Qt::BlockingQueuedConnection);
    connect(m_radioControl, &RadioControl::announcement, this, &MainWindow::onAnnouncement, Qt::QueuedConnection);
    connect(m_audioOutput, &AudioOutput::audioOutputRestart, m_radioControl, &RadioControl::onAudioOutputRestart, Qt::QueuedConnection);

    connect(m_ensembleInfoDialog, &EnsembleInfoDialog::requestEnsembleConfiguration, m_radioControl, &RadioControl::getEnsembleConfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::snrLevel, m_ensembleInfoDialog, &EnsembleInfoDialog::updateSnr, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::freqOffset, m_ensembleInfoDialog, &EnsembleInfoDialog::updateFreqOffset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleConfiguration, m_ensembleInfoDialog, &EnsembleInfoDialog::refreshEnsembleConfiguration, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, m_ensembleInfoDialog, &EnsembleInfoDialog::newFrequency, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::fibCounter, m_ensembleInfoDialog, &EnsembleInfoDialog::updateFIBstatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::mscCounter, m_ensembleInfoDialog, &EnsembleInfoDialog::updateMSCstatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_ensembleInfoDialog, &EnsembleInfoDialog::serviceChanged, Qt::QueuedConnection);

    connect(m_radioControl, &RadioControl::dlDataGroup_Service, &m_dlDecoder[InstanceIdx::Service], &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::dlDataGroup_Announcement, &m_dlDecoder[InstanceIdx::Announcement], &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, this, &MainWindow::onAudioServiceSelection, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, &m_dlDecoder[InstanceIdx::Service], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioServiceSelection, &m_dlDecoder[InstanceIdx::Announcement], &DLDecoder::reset, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::audioData, m_audioDecoder, &AudioDecoder::decodeData, Qt::QueuedConnection);

    // service stopped
    connect(m_radioControl, &RadioControl::ensembleRemoved, this, &MainWindow::onEnsembleRemoved, Qt::QueuedConnection);

    // reconfiguration
    connect(m_radioControl, &RadioControl::audioServiceReconfiguration, this, &MainWindow::onAudioServiceReconfiguration, Qt::QueuedConnection);
    connect(this, &MainWindow::getAudioInfo, m_audioDecoder, &AudioDecoder::getAudioParameters, Qt::QueuedConnection);

    // DL(+)
    // normal service
    connect(&m_dlDecoder[InstanceIdx::Service], &DLDecoder::dlComplete, this, &MainWindow::onDLComplete_Service);
    connect(&m_dlDecoder[InstanceIdx::Service], &DLDecoder::dlPlusObject, this, &MainWindow::onDLPlusObjReceived);
    connect(&m_dlDecoder[InstanceIdx::Service], &DLDecoder::dlItemToggle, this, &MainWindow::onDLPlusItemToggle);
    connect(&m_dlDecoder[InstanceIdx::Service], &DLDecoder::dlItemRunning, this, &MainWindow::onDLPlusItemRunning);
    connect(&m_dlDecoder[InstanceIdx::Service], &DLDecoder::resetTerminal, this, &MainWindow::onDLReset_Service);
    // announcement
    connect(&m_dlDecoder[InstanceIdx::Announcement], &DLDecoder::dlComplete, this, &MainWindow::onDLComplete_Announcement);
    connect(&m_dlDecoder[InstanceIdx::Announcement], &DLDecoder::resetTerminal, this, &MainWindow::onDLReset_Announcement);

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
    m_slideShowApp = new SlideShowApp(m_radioControl);
    m_slideShowApp->moveToThread(m_radioControlThread);
    connect(m_radioControlThread, &QThread::finished, m_slideShowApp, &QObject::deleteLater);
    connect(m_radioControl, &RadioControl::audioServiceSelection, m_slideShowApp, &SlideShowApp::start);
    connect(m_slideShowApp, &SlideShowApp::currentSlide, ui->slsView, &SLSView::showSlide, Qt::QueuedConnection);
    connect(m_slideShowApp, &SlideShowApp::resetTerminal, ui->slsView, &SLSView::reset, Qt::QueuedConnection);
    connect(m_slideShowApp, &SlideShowApp::catSlsAvailable, ui->catSlsLabel, &ClickableLabel::setVisible, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, m_slideShowApp, &SlideShowApp::stop, Qt::QueuedConnection);

    m_catSlsDialog = new CatSLSDialog(this);
    connect(m_slideShowApp, &SlideShowApp::categoryUpdate, m_catSlsDialog, &CatSLSDialog::onCategoryUpdate, Qt::QueuedConnection);
    connect(m_slideShowApp, &SlideShowApp::catSlide, m_catSlsDialog, &CatSLSDialog::onCatSlide, Qt::QueuedConnection);
    connect(m_slideShowApp, &SlideShowApp::resetTerminal, m_catSlsDialog, &CatSLSDialog::reset, Qt::QueuedConnection);
    connect(m_catSlsDialog, &CatSLSDialog::getCurrentCatSlide, m_slideShowApp, &SlideShowApp::getCurrentCatSlide, Qt::QueuedConnection);
    connect(m_catSlsDialog, &CatSLSDialog::getNextCatSlide, m_slideShowApp, &SlideShowApp::getNextCatSlide, Qt::QueuedConnection);

#if RADIO_CONTROL_SPI_ENABLE
#warning "Remove automatic creation of SPI app"
    // slide show application is created by default
    // ETSI TS 101 499 V3.1.1  [5.1.1]
    // The application should be automatically started when a SlideShow service is discovered for the current radio service
    spiApp = new SPIApp(radioControl);
    spiApp->moveToThread(radioControlThr);
    connect(radioControlThr, &QThread::finished, spiApp, &QObject::deleteLater);
    connect(radioControl, &RadioControl::newServiceSelection, spiApp, &SPIApp::start);
    //connect(spiApp, &SPIApp::resetTerminal, ui->slsView, &SLSView::reset, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, spiApp, &SPIApp::stop, Qt::QueuedConnection);
#endif

    // input device connections
    initInputDevice(InputDeviceId::UNDEFINED);

    loadSettings();

    QTimer::singleShot(1, this, [this](){ ui->serviceListView->setFocus(); } );
}

MainWindow::~MainWindow()
{
    delete m_inputDevice;

    m_radioControlThread->quit();  // this deletes radioControl
    m_radioControlThread->wait();
    delete m_radioControlThread;

    m_audioDecoderThread->quit();  // this deletes audiodecoder
    m_audioDecoderThread->wait();
    delete m_audioDecoderThread;

#if HAVE_PORTAUDIO
    delete m_audioOutput;
#else
    m_audioOutputThread->quit();  // this deletes audiodecoder
    m_audioOutputThread->wait();
    delete m_audioOutputThread;
#endif

    delete [] m_dlDecoder;

    delete m_serviceList;

    delete ui;
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o == ui->serviceListView)
    {
        if(e->type() == QEvent::KeyPress)
        {
            QKeyEvent * event = static_cast<QKeyEvent *>(e);
            if (Qt::Key_Return == event->key())
            {
                onServiceListClicked(ui->serviceListView->currentIndex());
                return true;
            }
            if (Qt::Key_Space == event->key())
            {
                ui->favoriteLabel->toggle();
                return true;
            }
        }
        return QObject::eventFilter(o, e);
    }
    if (o == ui->serviceTreeView)
    {
        if(e->type() == QEvent::KeyPress)
        {
            QKeyEvent * event = static_cast<QKeyEvent *>(e);
            if (Qt::Key_Return == event->key())
            {
                onServiceListTreeClicked(ui->serviceTreeView->currentIndex());
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
    {
        m_exitRequested = true;
        emit stopUserApps();
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

void MainWindow::changeEvent( QEvent* e )
{
#ifdef Q_OS_MACX
    if ( e->type() == QEvent::PaletteChange )
    {
        setIcons();
    }
#endif
    QMainWindow::changeEvent( e );
}

void MainWindow::onInputDeviceReady()
{
    ui->channelCombo->setEnabled(true);
}

void MainWindow::onEnsembleInfo(const RadioControlEnsemble &ens)
{
    ui->ensembleLabel->setText(ens.label);
    ui->ensembleLabel->setToolTip(QString("<b>Ensemble:</b> %1<br>"
                                          "<b>Short label:</b> %2<br>"
                                          "<b>ECC:</b> 0x%3<br>"
                                          "<b>EID:</b> 0x%4<br>"
                                          "<b>Country:</b> %5")
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
    emit stopUserApps();

    clearServiceInformationLabels();
    m_dlDecoder[InstanceIdx::Service].reset();
    m_dlDecoder[InstanceIdx::Announcement].reset();
    ui->favoriteLabel->setEnabled(false);

    m_serviceList->removeEnsemble(ens);
}

void MainWindow::onSyncStatus(uint8_t sync)
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
    m_syncLabel->setText(syncLevelLabels[sync]);
    m_syncLabel->setToolTip(syncLevelTooltip[sync]);
}

void MainWindow::onSnrLevel(float snr)
{
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
    onDLComplete(ui->dynamicLabel_Service, dl);
}

void MainWindow::onDLComplete_Announcement(const QString & dl)
{
    onDLComplete(ui->dynamicLabel_Announcement, dl);
}

void MainWindow::onDLComplete(QLabel * dlLabel, const QString & dl)
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
    m_timeLabel->setText(d.toString(QString("dddd, dd.MM.yyyy, hh:mm")));
}

void MainWindow::onAudioParametersInfo(const struct AudioParameters & params)
{
    switch (params.coding)
    {
    case AudioCoding::MP2:
        ui->audioEncodingLabel->setText("MP2");
        ui->audioEncodingLabel->setToolTip(QString("<b>DAB audio encoding</b><br>%1").arg("MPEG-1 layer 2"));
        break;
    case AudioCoding::AACLC:
        ui->audioEncodingLabel->setText("AAC-LC");
        ui->audioEncodingLabel->setToolTip(QString("<b>DAB+ audio encoding</b><br>%1").arg("MPEG-4 Low Complexity AAC"));
        break;
    case AudioCoding::HEAAC:
        ui->audioEncodingLabel->setText("HE-AAC");
        ui->audioEncodingLabel->setToolTip(QString("<b>DAB+ audio encoding</b><br>%1").arg("MPEG-4 High Efficiency AAC"));
        break;
    case AudioCoding::HEAACv2:
        ui->audioEncodingLabel->setText("HE-AACv2");
        ui->audioEncodingLabel->setToolTip(QString("<b>DAB+ audio encoding</b><br>%1").arg("MPEG-4 High Efficiency AAC v2"));
        break;
    }

    if (params.stereo)
    {
        ui->stereoLabel->setText("STEREO");
        if (AudioCoding::MP2 == params.coding)
        {
            ui->stereoLabel->setToolTip(QString("<b>Audio signal</b><br>%1Stereo<br>Sample rate: %2 kHz")
                    .arg(params.sbr ? "Joint " : "")
                    .arg(params.sampleRateKHz)
                );
        }
        else
        {
            ui->stereoLabel->setToolTip(QString("<b>Audio signal</b><br>Stereo (PS %1)<br>Sample rate: %2 kHz (SBR %3)")
                    .arg(params.parametricStereo? "on" : "off")
                    .arg(params.sampleRateKHz)
                    .arg(params.sbr ? "on" : "off")
                );
        }
    }
    else
    {
        ui->stereoLabel->setText("MONO");
        if (AudioCoding::MP2 == params.coding)
        {
            ui->stereoLabel->setToolTip(QString("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz")
                        .arg(params.sampleRateKHz)
                    );
        }
        else
        {
            ui->stereoLabel->setToolTip(QString("<b>Audio signal</b><br>Mono<br>Sample rate: %1 kHz (SBR: %2)")
                        .arg(params.sampleRateKHz)
                        .arg(params.sbr ? "on" : "off")
                    );
        }
    }
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
    ui->frequencyLabel->setText("Tuning...  ");

    onSyncStatus(uint8_t(DabSyncLevel::NoSync));
    onSnrLevel(0);

    // hide switch to avoid conflict with tuning -> will be enabled when tune is finished
    ui->switchSourceLabel->setHidden(true);
    serviceSelected();
}

void MainWindow::serviceSelected()
{
    emit stopUserApps();

    clearServiceInformationLabels();
    m_dlDecoder[InstanceIdx::Service].reset();
    m_dlDecoder[InstanceIdx::Announcement].reset();
    ui->favoriteLabel->setEnabled(false);
}

void MainWindow::onChannelChange(int index)
{
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
        }

        emit serviceRequest(ui->channelCombo->itemData(index).toUInt(), 0, 0);
    }
}

void MainWindow::onTuneDone(uint32_t freq)
{   // this slot is called when tune is complete
    m_frequency = freq;
    if (freq != 0)
    {
        ui->channelCombo->setEnabled(true);
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

        ui->frequencyLabel->setText(QString("%1 MHz").arg(freq/1000.0, 3, 'f', 3, QChar('0')));
        m_isPlaying = true;

        // if current service has alternatives show icon immediately to avoid UI blocking when audio does not work
        if (m_serviceList->numEnsembles(ServiceListId(m_SId.value(), m_SCIdS)) > 1)
        {
            ui->switchSourceLabel->setVisible(true);
        }
    }
    else
    {
        // this can only happen when device is changed, or ehen exit is requested
        if (m_exitRequested)
        {   // processing in IDLE, close window
            close();
            return;
        }

        ui->frequencyLabel->setText("");
        m_isPlaying = false;
        if (m_deviceChangeRequested)
        {
            initInputDevice(m_inputDeviceId);
        }
    }
}

void MainWindow::onInputDeviceError(const InputDeviceErrorCode errCode)
{
    switch (errCode)
    {
    case InputDeviceErrorCode::EndOfFile:
        // tune to 0
        if (!m_setupDialog->settings().rawfile.loopEna)
        {
            ui->channelCombo->setCurrentIndex(-1);
        }
        m_timeLabel->setText("End of file");
        break;
    case InputDeviceErrorCode::DeviceDisconnected:
        m_timeLabel->setText("Input device disconnected");
        // force no device
        m_setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    case InputDeviceErrorCode::NoDataAvailable:
        m_timeLabel->setText("No input data");
        // force no device
        m_setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    default:
        qDebug() << "InputDevice error" << int(errCode);
    }
}

void MainWindow::onServiceListClicked(const QModelIndex &index)
{
    const SLModel * model = reinterpret_cast<const SLModel*>(index.model());
    if (model->id(index) == ServiceListId(m_SId.value(), m_SCIdS))
    {
        return;
    }

    ServiceListConstIterator it = m_serviceList->findService(model->id(index));
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

void MainWindow::onServiceListTreeClicked(const QModelIndex &index)
{
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(index.model());

    if (index.parent().isValid())
    {   // service, not ensemble selected
        // if both service ID and ensemble ID are the same then return
        ServiceListId currentServiceId(m_SId.value(), m_SCIdS);
        ServiceListId currentEnsId(0);
        ServiceListConstIterator it = m_serviceList->findService(currentServiceId);
        if (m_serviceList->serviceListEnd() != it)
        {  // found
            currentEnsId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->id();
        }

        if ((model->id(index) == currentServiceId) && (model->id(index.parent()) == currentEnsId))
        {
            return;
        }

        it = m_serviceList->findService(model->id(index));
        if (m_serviceList->serviceListEnd() != it)
        {
            m_SId = (*it)->SId();
            m_SCIdS = (*it)->SCIdS();

            uint32_t newFrequency = (*it)->switchEnsemble(model->id(index.parent()))->frequency();
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
            ui->favoriteLabel->setChecked((*it)->isFavorite());
            int numEns = (*it)->numEnsembles();
            if (numEns > 1)
            {
                ui->switchSourceLabel->setVisible(true);
                int current = (*it)->currentEnsembleIdx();
                const EnsembleListItem * ens = (*it)->getEnsemble(current+1);  // get next ensemble
                ui->switchSourceLabel->setToolTip(QString("<b>Ensemble %1/%2</b><br>Click for switching to:<br><i>%3</i>")
                                                  .arg(current+1)
                                                  .arg(numEns)
                                                  .arg(ens->label()));
            }
        }
        ui->serviceLabel->setText(s.label);
        ui->serviceLabel->setToolTip(QString("<b>Service:</b> %1<br>"
                                             "<b>Short label:</b> %2<br>"
                                             "<b>SId:</b> 0x%3<br>"
                                             "<b>SCIdS:</b> %4<br>"
                                             "<b>Language:</b> %5<br>"
                                             "<b>Country:</b> %6")
                                     .arg(s.label)
                                     .arg(s.labelShort)
                                     .arg(QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper() )
                                     .arg(s.SCIdS)
                                     .arg(DabTables::getLangName(s.lang))
                                     .arg(DabTables::getCountryName(s.SId.value())));

        if ((s.pty.d != 0) && (s.pty.d != s.pty.s))
        {   // dynamic PTy available != static PTy
            ui->programTypeLabel->setText(DabTables::getPtyName(s.pty.d));
            ui->programTypeLabel->setToolTip(QString("<b>Programme Type</b><br>"
                                                     "%1 (dynamic)<br>"
                                                     "%2 (static)")
                                             .arg(DabTables::getPtyName(s.pty.d))
                                             .arg(DabTables::getPtyName(s.pty.s)));

        }
        else
        {
            ui->programTypeLabel->setText(DabTables::getPtyName(s.pty.s));
            ui->programTypeLabel->setToolTip(QString("<b>Programme Type</b><br>"
                                                     "%1")
                                             .arg(DabTables::getPtyName(s.pty.s)));

        }

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
            toolTip = QString("<B>Error protection</b><br>"
                              "%1<br>Coderate: %2/%3<br>"
                              "Capacity units: %4 CU")
                                            .arg(label)
                                            .arg(s.protection.codeRateUpper)
                                            .arg(s.protection.codeRateLower)
                                            .arg(s.SubChSize);
        }
        else
        {  // UEP
            label = QString("UEP #%1").arg(s.protection.uepIndex);
            toolTip = QString("<B>Error protection</b><br>"
                              "%1<br>Protection level: %2<br>"
                              "Capacity units: %3 CU")
                                            .arg(label)
                                            .arg(int(s.protection.level))
                                            .arg(s.SubChSize);
        }
        ui->protectionLabel->setText(label);
        ui->protectionLabel->setToolTip(toolTip);

        QString br = QString("%1 kbps").arg(s.streamAudioData.bitRate);
        ui->audioBitrateLabel->setText(br);
        ui->audioBitrateLabel->setToolTip(QString("<b>Service bitrate</b><br>Audio & data: %1").arg(br));

        //ui->serviceListView->selectionModel()->setCurrentIndex(item->index(), QItemSelectionModel::Select | QItemSelectionModel::Current);
        //ui->serviceListView->setFocus();
    }
    else
    {   // sid it not equal to selected sid -> this should not happen
        m_SId.set(0);

        ui->serviceListView->clearSelection();
        ui->serviceTreeView->clearSelection();
        //ui->serviceListView->selectionModel()->select(ui->serviceListView->selectionModel()->selection(), QItemSelectionModel::Deselect);
    }
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
        ui->serviceLabel->setText(serviceLabel);
        ui->programTypeLabel->setText("Service currently unavailable");
        ui->programTypeLabel->setToolTip("Service was removed from ensemble");

        emit stopUserApps();
        m_dlDecoder[InstanceIdx::Service].reset();
        m_dlDecoder[InstanceIdx::Announcement].reset();

        ui->favoriteLabel->setEnabled(false);
    }
}

void MainWindow::onAnnouncement(DabAnnouncement id, bool serviceSwitch)
{
    bool ongoing = (DabAnnouncement::Undefined != id);
    if (ongoing)
    {
        ui->announcementLabel->setToolTip(QString("Ongoing announcement:<br><b>%1</b>")
                                              .arg(DabTables::getAnnouncementName(id)));
        ui->dlWidget->setCurrentIndex(InstanceIdx::Announcement);
        ui->dlPlusWidget->setCurrentIndex(InstanceIdx::Announcement);
        qDebug() << "Announcement START | service switch" << serviceSwitch;
    }
    else
    {
        ui->dlWidget->setCurrentIndex(InstanceIdx::Service);
        ui->dynamicLabel_Announcement->clear();   // clear for next announcment

        ui->dlPlusWidget->setCurrentIndex(InstanceIdx::Service);
        qDebug() << "Announcement STOP | service switch" << serviceSwitch;
    }
    ui->announcementLabel->setVisible(ongoing);
}

void MainWindow::clearEnsembleInformationLabels()
{
    m_timeLabel->setText("");
    ui->ensembleLabel->setText("No ensemble");
    ui->ensembleLabel->setToolTip("No ensemble tuned");
    ui->ensembleInfoLabel->setText("");
    ui->frequencyLabel->setText("");
}

void MainWindow::clearServiceInformationLabels()
{
    ui->serviceLabel->setText("No service");
    ui->favoriteLabel->setChecked(false);
    ui->catSlsLabel->setHidden(true);
    ui->announcementLabel->setHidden(true);
    ui->serviceLabel->setToolTip("No service playing");
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
}

void MainWindow::onNewSettings()
{
    SetupDialog::Settings s = m_setupDialog->settings();
    switch (m_inputDeviceId)
    {
    case InputDeviceId::RTLSDR:
        dynamic_cast<RtlSdrInput*>(m_inputDevice)->setGainMode(s.rtlsdr.gainMode, s.rtlsdr.gainIdx);
        break;
    case InputDeviceId::RTLTCP:
        dynamic_cast<RtlTcpInput*>(m_inputDevice)->setGainMode(s.rtltcp.gainMode, s.rtltcp.gainIdx);
        break;
    case InputDeviceId::RARTTCP:
        break;
    case InputDeviceId::AIRSPY:
#if HAVE_AIRSPY
        dynamic_cast<AirspyInput*>(m_inputDevice)->setGainMode(s.airspy.gain);
#endif
        break;
    case InputDeviceId::SOAPYSDR:
#if HAVE_SOAPYSDR
        dynamic_cast<SoapySdrInput*>(m_inputDevice)->setGainMode(s.soapysdr.gainMode, s.soapysdr.gainIdx);
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
    m_deviceChangeRequested = true;
    m_inputDeviceId = d;
    if (m_isPlaying)
    { // stop
        stop();
        ui->channelCombo->setDisabled(true);  // enabled when device is ready
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

    // disable file dumping
    m_ensembleInfoDialog->enableDumpToFile(false);

    switch (d)
    {
    case InputDeviceId::UNDEFINED:
        // do nothing
        m_inputDeviceId = InputDeviceId::UNDEFINED;
        m_inputDevice = nullptr;
        ui->channelCombo->setDisabled(true);   // it will be enabled when device is ready

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
        {  // rtl sdr is available
            m_inputDeviceId = InputDeviceId::RTLSDR;

            // setup dialog
            m_setupDialog->setGainValues(dynamic_cast<RtlSdrInput*>(m_inputDevice)->getGainList());

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStart, m_inputDevice, &InputDevice::startDumpToFile);
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStop, m_inputDevice, &InputDevice::stopDumpToFile);
            connect(m_inputDevice, &InputDevice::dumpingToFile, m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStateToggle);
            connect(m_inputDevice, &InputDevice::dumpedBytes, m_ensembleInfoDialog, &EnsembleInfoDialog::updateDumpStatus);
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableDumpToFile(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<RtlSdrInput*>(m_inputDevice)->setBW(m_setupDialog->settings().rtlsdr.bandwidth);
            dynamic_cast<RtlSdrInput*>(m_inputDevice)->setBiasT(m_setupDialog->settings().rtlsdr.biasT);

            // apply current settings
            onNewSettings();
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
            m_inputDeviceId = InputDeviceId::RTLTCP;

            // setup dialog
            m_setupDialog->setGainValues(dynamic_cast<RtlTcpInput*>(m_inputDevice)->getGainList());

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStart, m_inputDevice, &InputDevice::startDumpToFile);
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStop, m_inputDevice, &InputDevice::stopDumpToFile);
            connect(m_inputDevice, &InputDevice::dumpingToFile, m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStateToggle);
            connect(m_inputDevice, &InputDevice::dumpedBytes, m_ensembleInfoDialog, &EnsembleInfoDialog::updateDumpStatus);
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableDumpToFile(true);

            // apply current settings
            onNewSettings();
        }
        else
        {
            m_setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
    break;
    case InputDeviceId::RARTTCP:
    {
#if HAVE_RARTTCP
        inputDevice = new RartTcpInput();

        // signals have to be connected before calling isAvailable
        // RTL_TCP is opened immediately and starts receiving data

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // set IP address and port
        dynamic_cast<RartTcpInput*>(inputDevice)->setTcpIp(setupDialog->settings().rarttcp.tcpAddress, setupDialog->settings().rarttcp.tcpPort);

        if (inputDevice->openDevice())
        {  // rtl tcp is available
            inputDeviceId = InputDeviceId::RARTTCP;

            // enable band scan
            bandScanAct->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            connect(ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStart, inputDevice, &InputDevice::startDumpToFile);
            connect(ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStop, inputDevice, &InputDevice::stopDumpToFile);
            connect(inputDevice, &InputDevice::dumpingToFile, ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStateToggle);
            connect(inputDevice, &InputDevice::dumpedBytes, ensembleInfoDialog, &EnsembleInfoDialog::updateDumpStatus);
            ensembleInfoDialog->enableDumpToFile(true);

            // apply current settings
            onNewSettings();
        }
        else
        {
            setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
#endif
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
            m_inputDeviceId = InputDeviceId::AIRSPY;

            // enable band scan
            m_bandScanAction->setEnabled(true);

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // ensemble info dialog
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStart, m_inputDevice, &InputDevice::startDumpToFile);
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStop, m_inputDevice, &InputDevice::stopDumpToFile);
            connect(m_inputDevice, &InputDevice::dumpingToFile, m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStateToggle);
            connect(m_inputDevice, &InputDevice::dumpedBytes, m_ensembleInfoDialog, &EnsembleInfoDialog::updateDumpStatus);
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableDumpToFile(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<AirspyInput*>(m_inputDevice)->setBiasT(m_setupDialog->settings().airspy.biasT);
            dynamic_cast<AirspyInput*>(m_inputDevice)->setDataPacking(m_setupDialog->settings().airspy.dataPacking);

            // apply current settings
            onNewSettings();
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
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStart, m_inputDevice, &InputDevice::startDumpToFile);
            connect(m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStop, m_inputDevice, &InputDevice::stopDumpToFile);
            connect(m_inputDevice, &InputDevice::dumpingToFile, m_ensembleInfoDialog, &EnsembleInfoDialog::dumpToFileStateToggle);
            connect(m_inputDevice, &InputDevice::dumpedBytes, m_ensembleInfoDialog, &EnsembleInfoDialog::updateDumpStatus);
            connect(m_inputDevice, &InputDevice::agcGain, m_ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            m_ensembleInfoDialog->enableDumpToFile(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<SoapySdrInput*>(m_inputDevice)->setBW(m_setupDialog->settings().soapysdr.bandwidth);

            // apply current settings
            onNewSettings();
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
        m_inputDeviceId = InputDeviceId::RAWFILE;

        // tuning procedure
        connect(m_radioControl, &RadioControl::tuneInputDevice, m_inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::tuned, m_radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(m_inputDevice, &InputDevice::deviceReady, this, &MainWindow::onInputDeviceReady, Qt::QueuedConnection);
        connect(m_inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        RawFileInputFormat format = m_setupDialog->settings().rawfile.format;
        dynamic_cast<RawFileInput*>(m_inputDevice)->setFile(m_setupDialog->settings().rawfile.file, format);

        // we can open device now
        if (m_inputDevice->openDevice())
        {  // raw file is available
            // clear service list
            m_serviceList->clear();

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // apply current settings
            onNewSettings();
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
    restoreGeometry(settings->value("windowGeometry").toByteArray());
    m_expertMode = settings->value("ExpertMode").toBool();
    setExpertMode(m_expertMode);
    m_volumeSlider->setValue(settings->value("volume", 100).toInt());

    int inDevice = settings->value("inputDeviceId", int(InputDeviceId::RTLSDR)).toInt();

    SetupDialog::Settings s;
    s.inputDevice = static_cast<InputDeviceId>(inDevice);

    s.rtlsdr.gainIdx = settings->value("RTL-SDR/gainIndex", 0).toInt();
    s.rtlsdr.gainMode = static_cast<RtlGainMode>(settings->value("RTL-SDR/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    s.rtlsdr.bandwidth = settings->value("RTL-SDR/bandwidth", 0).toInt();
    s.rtlsdr.biasT = settings->value("RTL-SDR/bias-T", false).toBool();

    s.rtltcp.gainIdx = settings->value("RTL-TCP/gainIndex", 0).toInt();
    s.rtltcp.gainMode = static_cast<RtlGainMode>(settings->value("RTL-TCP/gainMode", static_cast<int>(RtlGainMode::Software)).toInt());
    s.rtltcp.tcpAddress = settings->value("RTL-TCP/address", QString("127.0.0.1")).toString();
    s.rtltcp.tcpPort = settings->value("RTL-TCP/port", 1234).toInt();

#if HAVE_RARTTCP
    s.rarttcp.tcpAddress = settings->value("RART-TCP/address", QString("127.0.0.1")).toString();
    s.rarttcp.tcpPort = settings->value("RART-TCP/port", 1235).toInt();
#endif
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
    s.soapysdr.bandwidth = settings->value("SOAPYSDR/bandwidth", 0).toInt();
#endif
    s.rawfile.file = settings->value("RAW-FILE/filename", QVariant(QString(""))).toString();
    s.rawfile.format = RawFileInputFormat(settings->value("RAW-FILE/format", 0).toInt());
    s.rawfile.loopEna = settings->value("RAW-FILE/loop", false).toBool();

    m_setupDialog->setSettings(s);

    m_ensembleInfoDialog->setDumpPath(settings->value("dumpPath", QVariant(QDir::homePath())).toString());

    if (InputDeviceId::UNDEFINED != static_cast<InputDeviceId>(inDevice))
    {
        initInputDevice(s.inputDevice);

        // if input device has switched to what was stored and it is RTLSDR or RTLTCP or Airspy
        if ((s.inputDevice == m_inputDeviceId)
                && (    (InputDeviceId::RTLSDR == m_inputDeviceId)
                     || (InputDeviceId::AIRSPY == m_inputDeviceId)
                     || (InputDeviceId::SOAPYSDR == m_inputDeviceId)
                     || (InputDeviceId::RTLTCP == m_inputDeviceId)
                     || (InputDeviceId::RARTTCP == m_inputDeviceId)))
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
                    ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
                    onServiceListClicked(index);   // this selects service
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
    settings->setValue("dumpPath", m_ensembleInfoDialog->getDumpPath());
    settings->setValue("volume", m_volumeSlider->value());
    settings->setValue("windowGeometry", saveGeometry());

    settings->setValue("RTL-SDR/gainIndex", s.rtlsdr.gainIdx);
    settings->setValue("RTL-SDR/gainMode", static_cast<int>(s.rtlsdr.gainMode));
    settings->setValue("RTL-SDR/bandwidth", s.rtlsdr.bandwidth);
    settings->setValue("RTL-SDR/bias-T", s.rtlsdr.biasT);

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

#if HAVE_RARTTCP
    settings->setValue("RART-TCP/address", s.rarttcp.tcpAddress);
    settings->setValue("RART-TCP/port", s.rarttcp.tcpPort);
#endif
    settings->setValue("RAW-FILE/filename", s.rawfile.file);
    settings->setValue("RAW-FILE/format", int(s.rawfile.format));
    settings->setValue("RAW-FILE/loop", s.rawfile.loopEna);

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
    settings->setValue("ExpertMode", m_expertMode);

    m_serviceList->save(*settings);

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
            ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
            ui->serviceListView->setFocus();
            break;
        }
    }
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
            serviceTreeViewUpdateSelection();
        }
    }
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
                    ui->serviceTreeView->selectionModel()->setCurrentIndex(serviceIndex,
                           QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);
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
            ui->serviceListView->selectionModel()->setCurrentIndex(index,
                           QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);
            return;
        }
    }
}

void MainWindow::onDLPlusToggled(bool toggle)
{
    int h = 0;
    if (height() > minimumSizeHint().height())
    {   // user changed window height
        h = height();
    }
    ui->dlPlusWidget->setVisible(toggle);

    //QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
    QTimer::singleShot(10, this, [this, h](){
        resize(width(), (h > minimumSizeHint().height()) ? h : minimumSizeHint().height());
    } );
}

void MainWindow::switchMode()
{   // toggle expert mode
    m_expertMode = !m_expertMode;
    setExpertMode(m_expertMode);
}

void MainWindow::showEnsembleInfo()
{
    m_ensembleInfoDialog->show();
    m_ensembleInfoDialog->raise();
    m_ensembleInfoDialog->activateWindow();
}

void MainWindow::showAboutDialog()
{
    //QMessageBox::aboutQt(this, tr("About QT"));
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::showSetupDialog()
{
    m_setupDialog->show();
    m_setupDialog->raise();
    m_setupDialog->activateWindow();
}

void MainWindow::showCatSLS()
{
    m_catSlsDialog->show();
    m_catSlsDialog->raise();
    m_catSlsDialog->activateWindow();
}

void MainWindow::setExpertMode(bool ena)
{
    bool doResize = false;
    if (ena)
    {
        m_switchModeAction->setText("Basic mode");
    }
    else
    {
       m_switchModeAction->setText("Expert mode");

        // going to basic mode -> windows get resized if user did not resize before
        // this always keeps minimum window size
        doResize = (size() == minimumSizeHint());
    }
    ui->channelFrame->setVisible(ena);
    ui->audioFrame->setVisible(ena);
    ui->programTypeLabel->setVisible(ena);
    ui->ensembleInfoLabel->setVisible(ena);

    ui->serviceTreeView->setVisible(ena);
    m_signalQualityWidget->setVisible(ena);
    m_timeBasicQualWidget->setCurrentIndex(ena ? 1 : 0);

    ui->audioFrame->setVisible(ena);
    ui->channelFrame->setVisible(ena);
    ui->ensembleInfoLabel->setVisible(ena);
    ui->programTypeLabel->setVisible(ena);

    emit expertModeChanged(ena);

    if (doResize)
    {
        QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
    }
}

void MainWindow::bandScan()
{
    BandScanDialog * dialog = new BandScanDialog(this, m_serviceList->numServices() == 0, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    connect(dialog, &BandScanDialog::finished, dialog, &QObject::deleteLater);
    connect(dialog, &BandScanDialog::tuneChannel, this, &MainWindow::onTuneChannel);
    connect(m_radioControl, &RadioControl::syncStatus, dialog, &BandScanDialog::onSyncStatus, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::ensembleInformation, dialog, &BandScanDialog::onEnsembleFound, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::tuneDone, dialog, &BandScanDialog::onTuneDone, Qt::QueuedConnection);
    connect(m_radioControl, &RadioControl::serviceListComplete, dialog, &BandScanDialog::onServiceListComplete, Qt::QueuedConnection);
    connect(m_serviceList, &ServiceList::serviceAdded, dialog, &BandScanDialog::onServiceFound);
    connect(dialog, &BandScanDialog::scanStarts, this, &MainWindow::clearServiceList);
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
            QModelIndex index = model->index(0, 0);
            ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
            onServiceListClicked(index);   // this selects service
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
        // tune to 0
        ui->channelCombo->setCurrentIndex(-1);
    }
}

void MainWindow::clearServiceList()
{
    stop();
    m_serviceList->clear();
}

bool MainWindow::isDarkMode()
{
#ifdef Q_OS_MACX
    return macIsInDarkTheme();
#else
    //return windowsIsInDarkTheme();
    return false;
#endif
}

void MainWindow::setIcons()
{
    if (isDarkMode())
    {
        m_settingsLabel->setIcon(":/resources/menu_dark.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty_dark.png", false);
        ui->favoriteLabel->setIcon(":/resources/star_dark.png", true);

        m_muteLabel->setIcon(":/resources/volume_on_dark.png", false);
        m_muteLabel->setIcon(":/resources/volume_off_dark.png", true);

        ui->dlPlusLabel->setIcon(":/resources/down_dark.png", false);
        ui->dlPlusLabel->setIcon(":/resources/up_dark.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls_dark.png");

        ui->announcementLabel->setIcon(":/resources/announcement_dark.png");

        ui->switchSourceLabel->setIcon(":/resources/broadcast_dark.png");
    }
    else
    {
        m_settingsLabel->setIcon(":/resources/menu.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty.png", false);
        ui->favoriteLabel->setIcon(":/resources/star.png", true);

        m_muteLabel->setIcon(":/resources/volume_on.png", false);
        m_muteLabel->setIcon(":/resources/volume_off.png", true);

        ui->dlPlusLabel->setIcon(":/resources/down.png", false);
        ui->dlPlusLabel->setIcon(":/resources/up.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls.png");

        ui->announcementLabel->setIcon(":/resources/announcement.png");

        ui->switchSourceLabel->setIcon(":/resources/broadcast.png");
    }
    ui->slsView->setDarkMode(isDarkMode());
}

void MainWindow::onDLPlusObjReceived(const DLPlusObject & object)
{
    ui->dlPlusLabel->setVisible(true);
    if (DLPlusContentType::DUMMY == object.getType())
    {   // dummy object = do nothing
        // or
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        return;
    }
    else
    { /* no dummy object */ }

    if (object.isDelete())
    {   // [ETSI TS 102 980 V2.1.2] 5.3.2 Deleting objects
        // Objects are deleted by transmitting a "delete" object
        const auto it = m_dlObjCache.constFind(object.getType());
        if (it != m_dlObjCache.cend())
        {   // object type found in cache
            DLPlusObjectUI* uiObj = m_dlObjCache.take(object.getType());
            delete uiObj;
        }
        else
        { /* not in cache, do nothing */ }
    }
    else
    {
        auto it = m_dlObjCache.find(object.getType());
        if (it != m_dlObjCache.end())
        {   // object type found in cahe
            (*it)->update(object);
        }
        else
        {   // not in cache yet -> insert
            DLPlusObjectUI * uiObj = new DLPlusObjectUI(object);
            if (nullptr != uiObj->getLayout())
            {
                it = m_dlObjCache.insert(object.getType(), uiObj);

                // we want it sorted -> need to find the position
                int index = std::distance(m_dlObjCache.begin(), it);

                QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->dlPlusFrame_Service->layout());
                layout->insertLayout(index, uiObj->getLayout());
                ui->dlPlusFrame_Service->setMaximumHeight(ui->dlPlusFrame_Service->minimumHeight() > 120 ? ui->dlPlusFrame_Service->minimumHeight() : 120);
            }
            else
            { /* objects that we do not display: Descriptors, PROGRAMME_FREQUENCY, INFO_DATE_TIME, PROGRAMME_SUBCHANNEL*/ }
        }
    }
}

void MainWindow::onDLPlusItemToggle()
{
    // delete all ITEMS.*
    auto it = m_dlObjCache.cbegin();
    while (it != m_dlObjCache.cend())
    {
        if (it.key() < DLPlusContentType::INFO_NEWS)
        {
            DLPlusObjectUI* uiObj = it.value();
            delete uiObj;
            it = m_dlObjCache.erase(it);
        }
        else
        {   // no more items ot ITEM type in cache
            break;
        }
    }
}

void MainWindow::onDLPlusItemRunning(bool isRunning)
{
    auto it = m_dlObjCache.cbegin();
    while (it != m_dlObjCache.cend())
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
    ui->dynamicLabel_Service->setText("");
    ui->dlPlusLabel->setVisible(false);
    ui->dlPlusLabel->setChecked(false);
    onDLPlusToggled(false);

    for (auto objPtr : m_dlObjCache)
    {
        delete objPtr;
    }
    m_dlObjCache.clear();
}

void MainWindow::onDLReset_Announcement()
{
    ui->dynamicLabel_Announcement->setText("");
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
            QObject::connect(
                        m_tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        case DLPlusContentType::EMAIL_HOTLINE:
        case DLPlusContentType::EMAIL_STUDIO:
        case DLPlusContentType::EMAIL_OTHER:
            m_tagText = new QLabel(QString("<a href=\"mailto:%1\">%1</a>").arg(obj.getTag()));
            QObject::connect(
                        m_tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        default:
            m_tagText = new QLabel(obj.getTag());
        }

        m_tagText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
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
    case DLPlusContentType::ITEM_TITLE: label = "Title"; break;
    case DLPlusContentType::ITEM_ALBUM: label = "Album"; break;
    case DLPlusContentType::ITEM_TRACKNUMBER: label = "Track Number:"; break;
    case DLPlusContentType::ITEM_ARTIST: label = "Artist"; break;
    case DLPlusContentType::ITEM_COMPOSITION: label = "Composition"; break;
    case DLPlusContentType::ITEM_MOVEMENT: label = "Movement"; break;
    case DLPlusContentType::ITEM_CONDUCTOR: label = "Conductor"; break;
    case DLPlusContentType::ITEM_COMPOSER: label = "Composer"; break;
    case DLPlusContentType::ITEM_BAND: label = "Band"; break;
    case DLPlusContentType::ITEM_COMMENT: label = "Comment"; break;
    case DLPlusContentType::ITEM_GENRE: label = "Genre"; break;
    case DLPlusContentType::INFO_NEWS: label = "News"; break;
    case DLPlusContentType::INFO_NEWS_LOCAL: label = "News (local)"; break;
    case DLPlusContentType::INFO_STOCKMARKET: label = "Stock Market"; break;
    case DLPlusContentType::INFO_SPORT: label = "Sport"; break;
    case DLPlusContentType::INFO_LOTTERY: label = "Lottery"; break;
    case DLPlusContentType::INFO_HOROSCOPE: label = "Horoscope"; break;
    case DLPlusContentType::INFO_DAILY_DIVERSION: label = "Daily Diversion"; break;
    case DLPlusContentType::INFO_HEALTH: label = "Health"; break;
    case DLPlusContentType::INFO_EVENT: label = "Event"; break;
    case DLPlusContentType::INFO_SCENE: label = "Scene"; break;
    case DLPlusContentType::INFO_CINEMA: label = "Cinema"; break;
    case DLPlusContentType::INFO_TV: label = "TV"; break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::INFO_DATE_TIME: label = "INFO_DATE_TIME"; break;
    case DLPlusContentType::INFO_WEATHER: label = "Weather"; break;
    case DLPlusContentType::INFO_TRAFFIC: label = "Traffic"; break;
    case DLPlusContentType::INFO_ALARM: label = "Alarm"; break;
    case DLPlusContentType::INFO_ADVERTISEMENT: label = "Advertisment"; break;
    case DLPlusContentType::INFO_URL: label = "URL"; break;
    case DLPlusContentType::INFO_OTHER: label = "Other"; break;
    case DLPlusContentType::STATIONNAME_SHORT: label = "Station (short)"; break;
    case DLPlusContentType::STATIONNAME_LONG: label = "Station"; break;
    case DLPlusContentType::PROGRAMME_NOW: label = "Now"; break;
    case DLPlusContentType::PROGRAMME_NEXT: label = "Next"; break;
    case DLPlusContentType::PROGRAMME_PART: label = "Programme Part"; break;
    case DLPlusContentType::PROGRAMME_HOST: label = "Host"; break;
    case DLPlusContentType::PROGRAMME_EDITORIAL_STAFF: label = "Editorial"; break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::PROGRAMME_FREQUENCY: label = "Frequency"; break;
    case DLPlusContentType::PROGRAMME_HOMEPAGE: label = "Homepage"; break;
        // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
        // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
        //case DLPlusContentType::PROGRAMME_SUBCHANNEL: label = "Subchannel"; break;
    case DLPlusContentType::PHONE_HOTLINE: label = "Phone (Hotline)"; break;
    case DLPlusContentType::PHONE_STUDIO: label = "Phone (Studio)"; break;
    case DLPlusContentType::PHONE_OTHER: label = "Phone (Other)"; break;
    case DLPlusContentType::SMS_STUDIO: label = "SMS (Studio)"; break;
    case DLPlusContentType::SMS_OTHER: label = "SMS (Other)"; break;
    case DLPlusContentType::EMAIL_HOTLINE: label = "E-mail (Hotline)"; break;
    case DLPlusContentType::EMAIL_STUDIO: label = "E-mail (Studio)"; break;
    case DLPlusContentType::EMAIL_OTHER: label = "E-mail (Other)"; break;
    case DLPlusContentType::MMS_OTHER: label = "MMS"; break;
    case DLPlusContentType::CHAT: label = "Chat Message"; break;
    case DLPlusContentType::CHAT_CENTER: label = "Chat"; break;
    case DLPlusContentType::VOTE_QUESTION: label = "Vote Question"; break;
    case DLPlusContentType::VOTE_CENTRE: label = "Vote Here"; break;
        // rfu = 54
        // rfu = 55
    case DLPlusContentType::PRIVATE_1: label = "Private 1"; break;
    case DLPlusContentType::PRIVATE_2: label = "Private 2"; break;
    case DLPlusContentType::PRIVATE_3: label = "Private 3"; break;
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
