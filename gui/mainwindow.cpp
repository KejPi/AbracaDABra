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
#include "./ui_mainwindow.h"
#include "dabtables.h"
#include "radiocontrol.h"
#include "bandscandialog.h"
#include "config.h"

#ifdef Q_OS_MACX
#include "mac.h"
#endif

const QString appName("AbracaDABra");
const QStringList syncLevelLabels = {"No signal", "Signal found", "Sync"};
const QStringList syncLevelTooltip = {"DAB signal not detected<br>Looking for signal...",
                                      "Found DAB signal,<br>trying to synchronize...",
                                      "Synchronized to DAB signal"};
const QStringList snrProgressStylesheet = {
    QString::fromUtf8("QProgressBar::chunk {background-color: #ff4b4b; }"),  // red
    QString::fromUtf8("QProgressBar::chunk {background-color: #ffb527; }"),  // yellow
    QString::fromUtf8("QProgressBar::chunk {background-color: #5bc214; }")   // green
};

enum class SNR10Threhold
{
    SNR_BAD = 60,
    SNR_GOOD = 90
};

MainWindow::MainWindow(const QString &iniFile, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , iniFilename(iniFile)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();
    dlDecoder = new DLDecoder();

    ui->setupUi(this);
    connect(ui->channelCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onChannelChange);

    // set UI
    setWindowTitle("Abraca DAB Radio");

    // favorites control
    ui->favoriteLabel->setCheckable(true);
    ui->favoriteLabel->setTooltip("Add service to favorites", false);
    ui->favoriteLabel->setTooltip("Remove service from favorites", true);
    ui->favoriteLabel->setChecked(false);

    setupDialog = new SetupDialog(this);
    connect(setupDialog, &SetupDialog::inputDeviceChanged, this, &MainWindow::changeInputDevice);
    connect(this, &MainWindow::expertModeChanged, setupDialog, &SetupDialog::onExpertMode);
    connect(setupDialog, &SetupDialog::newSettings, this, &MainWindow::onNewSettings);

    ensembleInfoDialog = new EnsembleInfoDialog(this);

    // status bar
    QWidget * widget = new QWidget();
    timeLabel = new QLabel("");
    timeLabel->setToolTip("DAB time");

    basicSignalQualityLabel = new QLabel("");
    basicSignalQualityLabel->setToolTip("DAB signal quality");

    timeBasicQualWidget = new QStackedWidget;
    timeBasicQualWidget->addWidget(basicSignalQualityLabel);
    timeBasicQualWidget->addWidget(timeLabel);

    syncLabel = new QLabel();
    syncLabel->setAlignment(Qt::AlignRight);

    snrProgress = new QProgressBar();
    snrProgress->setMaximum(30*10);
    snrProgress->setMinimum(0);
    snrProgress->setFixedWidth(80);
    snrProgress->setFixedHeight(syncLabel->fontInfo().pixelSize());
    snrProgress->setTextVisible(false);
    snrProgress->setToolTip(QString("DAB signal SNR"));

    snrLabel = new QLabel();
//#ifdef __APPLE__
    int width = snrLabel->fontMetrics().boundingRect("100.0 dB").width();
    snrLabel->setFixedWidth(width);
//#endif
    snrLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    snrLabel->setToolTip(QString("DAB signal SNR"));

    QHBoxLayout * signalQualityLayout = new QHBoxLayout();
    signalQualityLayout->addWidget(syncLabel);
    signalQualityLayout->addWidget(snrProgress);
    signalQualityLayout->setAlignment(snrProgress, Qt::AlignCenter);
    signalQualityLayout->setStretch(0, 100);
    signalQualityLayout->setAlignment(syncLabel, Qt::AlignCenter);
    signalQualityLayout->addWidget(snrLabel);
    signalQualityLayout->setSpacing(10);
    signalQualityLayout->setContentsMargins(0,0,0,0);
    signalQualityWidget = new QWidget();
    signalQualityWidget->setLayout(signalQualityLayout);

    updateSnrLevel(0);
    updateSyncStatus(uint8_t(DabSyncLevel::NoSync));

    setupAct = new QAction("Setup...", this);
    //setupAct->setStatusTip("Application settings");       // this is shown in status bar
    connect(setupAct, &QAction::triggered, this, &MainWindow::showSetupDialog);

    clearServiceListAct = new QAction("Clear service list", this);
    connect(clearServiceListAct, &QAction::triggered, this, &MainWindow::clearServiceList);

    bandScanAct = new QAction("Band scan...", this);
    //scanAct->setStatusTip("Seach for available service"); // this is shown in status bar
    connect(bandScanAct, &QAction::triggered, this, &MainWindow::bandScan);

    switchModeAct = new QAction("Expert mode", this);
    connect(switchModeAct, &QAction::triggered, this, &MainWindow::switchMode);

    ensembleInfoAct = new QAction("Ensemble information", this);
    connect(ensembleInfoAct, &QAction::triggered, this, &MainWindow::showEnsembleInfo);

    aboutAct = new QAction("About", this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::showAboutDialog);

    menu = new QMenu(this);
    menu->addAction(setupAct);
    menu->addAction(bandScanAct);
    menu->addAction(switchModeAct);
    menu->addAction(ensembleInfoAct);
    menu->addAction(clearServiceListAct);
    menu->addAction(aboutAct);

    settingsLabel = new ClickableLabel(this);
    settingsLabel->setToolTip("Open menu");
    settingsLabel->setMenu(menu);

    muteLabel = new ClickableLabel(this);
    muteLabel->setCheckable(true);
    muteLabel->setTooltip("Mute audio", false);
    muteLabel->setTooltip("Unmute audio", true);
    muteLabel->setChecked(false);

#if !(defined HAVE_PORTAUDIO) || (AUDIOOUTPUT_PORTAUDIO_VOLUME_ENA)
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setMinimum(0);
    volumeSlider->setMaximum(100);
    volumeSlider->setSingleStep(10);
    volumeSlider->setToolTip("Audio volume");
#ifdef Q_OS_MACX
    //volumeSlider->setTickPosition(QSlider::TicksAbove);
#endif
#ifdef Q_OS_WIN
    volumeSlider->setMaximumHeight(15);
#endif
    connect(muteLabel, &ClickableLabel::toggled, volumeSlider, &QSlider::setDisabled);

    QHBoxLayout * volumeLayout = new QHBoxLayout();
    volumeLayout->addWidget(muteLabel);
#if 0
    QHBoxLayout * dummySliderLayout = new QHBoxLayout();
    dummySliderLayout->addWidget(volumeSlider);
    dummySliderLayout->setAlignment(volumeSlider, Qt::AlignCenter);
    dummySliderLayout->setContentsMargins(0,6,0,0);
    volumeLayout->addLayout(dummySliderLayout);
#else
    volumeLayout->addWidget(volumeSlider);
    volumeLayout->setAlignment(volumeSlider, Qt::AlignCenter);
#endif
    volumeLayout->setStretch(0, 100);
    volumeLayout->setAlignment(muteLabel, Qt::AlignCenter);
    volumeLayout->setSpacing(10);
    volumeLayout->setContentsMargins(10,0,0,0);
    QWidget * volumeWidget = new QWidget();
    volumeWidget->setLayout(volumeLayout);
#endif

    //DL+
    ui->dlPlusLabel->setCheckable(true);
    ui->dlPlusLabel->setTooltip("Show DL+", false);
    ui->dlPlusLabel->setTooltip("Hide DL+", true);
    ui->dlPlusLabel->setChecked(false);
    ui->dlPlusFrame->setVisible(false);

    QSizePolicy sp_retain = ui->dlPlusLabel->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    ui->dlPlusLabel->setSizePolicy(sp_retain);

    ui->dlPlusLabel->setHidden(true);
    connect(ui->dlPlusLabel, &ClickableLabel::toggled, this, &MainWindow::onDLPlusToggle);

    QGridLayout * layout = new QGridLayout(widget);
    layout->addWidget(timeBasicQualWidget, 0, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(signalQualityWidget, 0, 1, Qt::AlignVCenter | Qt::AlignRight);
#if !(defined HAVE_PORTAUDIO) || (AUDIOOUTPUT_PORTAUDIO_VOLUME_ENA)
    layout->addWidget(volumeWidget, 0, 2, Qt::AlignVCenter | Qt::AlignRight);
#else
    layout->addWidget(muteLabel, 0, 2, Qt::AlignVCenter | Qt::AlignRight);
#endif
    layout->addWidget(settingsLabel, 0, 3, Qt::AlignVCenter | Qt::AlignRight);

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
    serviceList = new ServiceList;

    slModel = new SLModel(serviceList, this);
    connect(serviceList, &ServiceList::serviceAdded, slModel, &SLModel::addService);
    connect(serviceList, &ServiceList::serviceUpdated, slModel, &SLModel::updateService);
    connect(serviceList, &ServiceList::serviceRemoved, slModel, &SLModel::removeService);
    connect(serviceList, &ServiceList::empty, slModel, &SLModel::clear);

    ui->serviceListView->setModel(slModel);
    ui->serviceListView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceListView->installEventFilter(this);
    connect(ui->serviceListView, &QListView::clicked, this, &MainWindow::serviceListClicked);

    slTreeModel = new SLTreeModel(serviceList, this);
    connect(serviceList, &ServiceList::serviceAddedToEnsemble, slTreeModel, &SLTreeModel::addEnsembleService);
    connect(serviceList, &ServiceList::serviceUpdatedInEnsemble, slTreeModel, &SLTreeModel::updateEnsembleService);
    connect(serviceList, &ServiceList::serviceRemovedFromEnsemble, slTreeModel, &SLTreeModel::removeEnsembleService);
    connect(serviceList, &ServiceList::ensembleRemoved, slTreeModel, &SLTreeModel::removeEnsemble);
    ui->serviceTreeView->setModel(slTreeModel);
    ui->serviceTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceTreeView->setHeaderHidden(true);
    ui->serviceTreeView->installEventFilter(this);
    connect(ui->serviceTreeView, &QTreeView::clicked, this, &MainWindow::serviceListTreeClicked);
    connect(serviceList, &ServiceList::empty, slTreeModel, &SLTreeModel::clear);

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
    ui->dynamicLabel->setText("");

    ui->audioEncodingLabel->setToolTip("Audio coding");

    ui->slsView->setMinimumSize(QSize(322, 242));
    ui->slsView->reset();

    ui->catSlsLabel->setToolTip("Browse categorized slides");
    ui->catSlsLabel->setHidden(true);

    ui->switchSourceLabel->setToolTip("Change service source (ensemble)");
    ui->switchSourceLabel->setHidden(true);

    ui->serviceTreeView->setVisible(false);

    setIcons();

    resize(minimumSizeHint());

    // threads
    radioControl = new RadioControl();
    radioControlThr = new QThread(this);
    radioControlThr->setObjectName("radioControlThr");
    radioControl->moveToThread(radioControlThr);
    connect(radioControlThr, &QThread::finished, radioControl, &QObject::deleteLater);
    radioControlThr->start();

    // initialize radio control
    if (!radioControl->init())
    {        
        qDebug() << "RadioControl() init failed";
        close();
        qApp->quit();
    }

    audioDecoder = new AudioDecoder(&audioBuffer, nullptr);
    audioDecoderThr = new QThread(this);
    audioDecoderThr->setObjectName("audioDecoderThr");
    audioDecoder->moveToThread(audioDecoderThr);
    connect(audioDecoderThr, &QThread::finished, audioDecoder, &QObject::deleteLater);
    audioDecoderThr->start();

    audioOutput = new AudioOutput(&audioBuffer);
#if (!defined HAVE_PORTAUDIO)
    audioOutThr = new QThread(this);
    audioOutThr->setObjectName("audioOutThr");
    audioOutput->moveToThread(audioOutThr);
    connect(audioOutThr, &QThread::finished, audioOutput, &QObject::deleteLater);
    audioOutThr->start();
#endif
#if (!defined HAVE_PORTAUDIO) || (AUDIOOUTPUT_PORTAUDIO_VOLUME_ENA)
    connect(volumeSlider, &QSlider::valueChanged, audioOutput, &AudioOutput::setVolume, Qt::QueuedConnection);
#endif

    // Connect signals
    connect(muteLabel, &ClickableLabel::toggled, audioOutput, &AudioOutput::mute, Qt::QueuedConnection);
    connect(ui->favoriteLabel, &ClickableLabel::toggled, this, &MainWindow::favoriteToggled);
    connect(ui->switchSourceLabel, &ClickableLabel::clicked, this, &MainWindow::switchServiceSource);
    connect(ui->catSlsLabel, &ClickableLabel::clicked, this, &MainWindow::showCatSLS);

    connect(radioControl, &RadioControl::ensembleInformation, this, &MainWindow::updateEnsembleInfo, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::ensembleReconfiguration, this, &MainWindow::onEnsembleReconfiguration, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::ensembleComplete, this, &MainWindow::onEnsembleComplete, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::syncStatus, this, &MainWindow::updateSyncStatus, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::snrLevel, this, &MainWindow::updateSnrLevel, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::dabTime, this, &MainWindow::updateDabTime, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::serviceListEntry, this, &MainWindow::updateServiceList, Qt::BlockingQueuedConnection);

    connect(ensembleInfoDialog, &EnsembleInfoDialog::requestEnsembleConfiguration, radioControl, &RadioControl::getEnsembleConfiguration, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::snrLevel, ensembleInfoDialog, &EnsembleInfoDialog::updateSnr, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::freqOffset, ensembleInfoDialog, &EnsembleInfoDialog::updateFreqOffset, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::ensembleConfiguration, ensembleInfoDialog, &EnsembleInfoDialog::refreshEnsembleConfiguration, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::tuneDone, ensembleInfoDialog, &EnsembleInfoDialog::newFrequency, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::fibCounter, ensembleInfoDialog, &EnsembleInfoDialog::updateFIBstatus, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::mscCounter, ensembleInfoDialog, &EnsembleInfoDialog::updateMSCstatus, Qt::QueuedConnection);    
    connect(radioControl, &RadioControl::audioServiceSelection, ensembleInfoDialog, &EnsembleInfoDialog::serviceChanged, Qt::QueuedConnection);

    connect(radioControl, &RadioControl::dlDataGroup, dlDecoder, &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::audioServiceSelection, this, &MainWindow::serviceChanged, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::audioServiceSelection, dlDecoder, &DLDecoder::reset, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::audioData, audioDecoder, &AudioDecoder::inputData, Qt::QueuedConnection);

    // service stopped
    connect(radioControl, &RadioControl::ensembleRemoved, this, &MainWindow::onEnsembleRemoved, Qt::QueuedConnection);

    // reconfiguration
    connect(radioControl, &RadioControl::audioServiceReconfiguration, this, &MainWindow::audioServiceReconfiguration, Qt::QueuedConnection);
    connect(this, &MainWindow::getAudioInfo, audioDecoder, &AudioDecoder::getAudioParameters, Qt::QueuedConnection);    

    // DL(+)       
    connect(dlDecoder, &DLDecoder::dlComplete, this, &MainWindow::updateDL);
    connect(dlDecoder, &DLDecoder::dlPlusObject, this, &MainWindow::onDLPlusObjReceived);
    connect(dlDecoder, &DLDecoder::dlItemToggle, this, &MainWindow::onDLPlusItemToggle);
    connect(dlDecoder, &DLDecoder::dlItemRunning, this, &MainWindow::onDLPlusItemRunning);
    connect(dlDecoder, &DLDecoder::resetTerminal, this, &MainWindow::onDLReset);

    connect(audioDecoder, &AudioDecoder::audioParametersInfo, this, &MainWindow::updateAudioInfo, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::audioServiceSelection, audioDecoder, &AudioDecoder::start, Qt::QueuedConnection);

    // audio output is controlled by signals from decoder
    connect(this, &MainWindow::stopUserApps, audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
    connect(audioDecoder, &AudioDecoder::startAudio, audioOutput, &AudioOutput::start, Qt::QueuedConnection);
    connect(audioDecoder, &AudioDecoder::stopAudio, audioOutput, &AudioOutput::stop, Qt::QueuedConnection);

    // tune procedure:
    // 1. mainwindow tune -> radiocontrol tune (this stops DAB SDR - tune to 0)
    // 2. radiocontrol tuneInputDevice -> inputdevice tune (reset of input bufer and tune FE)
    // 3. inputDevice tuned -> radiocontrol start (this starts DAB SDR)
    // 4. notification to HMI
    connect(this, &MainWindow::serviceRequest, radioControl, &RadioControl::tuneService, Qt::QueuedConnection);

    // these two signals have to be connected in initInputDevice() - left here as comment
    // connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
    // connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::tuneDone, this, &MainWindow::tuneFinished, Qt::QueuedConnection);

    connect(this, &MainWindow::exit, radioControl, &RadioControl::exit, Qt::QueuedConnection);

    // user applications

    // slide show application is created by default
    // ETSI TS 101 499 V3.1.1  [5.1.1]
    // The application should be automatically started when a SlideShow service is discovered for the current radio service
    slideShowApp = new SlideShowApp(radioControl);
    slideShowApp->moveToThread(radioControlThr);
    connect(radioControlThr, &QThread::finished, slideShowApp, &QObject::deleteLater);       
    connect(radioControl, &RadioControl::audioServiceSelection, slideShowApp, &SlideShowApp::start);
    connect(slideShowApp, &SlideShowApp::currentSlide, ui->slsView, &SLSView::showSlide, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::resetTerminal, ui->slsView, &SLSView::reset, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::catSlsAvailable, ui->catSlsLabel, &ClickableLabel::setVisible, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, slideShowApp, &SlideShowApp::stop, Qt::QueuedConnection);

    catSlsDialog = new CatSLSDialog(this);
    connect(slideShowApp, &SlideShowApp::categoryUpdate, catSlsDialog, &CatSLSDialog::onCategoryUpdate, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::catSlide, catSlsDialog, &CatSLSDialog::onCatSlide, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::resetTerminal, catSlsDialog, &CatSLSDialog::reset, Qt::QueuedConnection);
    connect(catSlsDialog, &CatSLSDialog::getCurrentCatSlide, slideShowApp, &SlideShowApp::getCurrentCatSlide, Qt::QueuedConnection);
    connect(catSlsDialog, &CatSLSDialog::getNextCatSlide, slideShowApp, &SlideShowApp::getNextCatSlide, Qt::QueuedConnection);    

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
    //qDebug() << Q_FUNC_INFO;

    delete inputDevice;

    radioControlThr->quit();  // this deletes radioControl
    radioControlThr->wait();
    delete radioControlThr;

    audioDecoderThr->quit();  // this deletes audiodecoder
    audioDecoderThr->wait();
    delete audioDecoderThr;

#if (defined HAVE_PORTAUDIO)
    delete audioOutput;
#else
    audioOutThr->quit();  // this deletes audiodecoder
    audioOutThr->wait();
    delete audioOutThr;
#endif

    delete dlDecoder;

    delete serviceList;

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
                serviceListClicked(ui->serviceListView->currentIndex());
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
                serviceListTreeClicked(ui->serviceTreeView->currentIndex());
                return true;
            }
        }
        return QObject::eventFilter(o, e);
    }
    return QObject::eventFilter(o, e);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (0 == frequency)
    {  // in idle
        //qDebug() << Q_FUNC_INFO << "requesting exit";

        saveSettings();

        emit exit();
        event->accept();
    }
    else
    {
        //qDebug() << Q_FUNC_INFO << "going to IDLE";
        exitRequested = true;
        emit stopUserApps();
        emit serviceRequest(0,0,0);
        event->ignore();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (ui->dynamicLabel->width() < ui->dynamicLabel->fontMetrics().boundingRect(ui->dynamicLabel->text()).width())
    {
        ui->dynamicLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        ui->dynamicLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
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

void MainWindow::inputDeviceReady()
{
    ui->channelCombo->setEnabled(true);    
}

void MainWindow::updateEnsembleInfo(const RadioControlEnsemble &ens)
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

    serviceList->beginEnsembleUpdate(ens);
}

void MainWindow::onEnsembleReconfiguration(const RadioControlEnsemble &ens) const
{
    serviceList->beginEnsembleUpdate(ens);
}

void MainWindow::onEnsembleComplete(const RadioControlEnsemble &ens)
{
    serviceList->endEnsembleUpdate(ens);

    serviceListViewUpdateSelection();
    serviceTreeViewUpdateSelection();
}

void MainWindow::onEnsembleRemoved(const RadioControlEnsemble &ens)
{
    emit stopUserApps();

    clearServiceInformationLabels();
    dlDecoder->reset();
    ui->favoriteLabel->setEnabled(false);

    serviceList->removeEnsemble(ens);
}

void MainWindow::updateSyncStatus(uint8_t sync)
{
    if (DabSyncLevel::FullSync > DabSyncLevel(sync))
    {   // hide time when no sync
        timeLabel->setText("");

        // set no signal quality when no sync
        if (isDarkMode())
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal0_dark.png"));
        }
        else
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal0.png"));
        }
    }
    syncLabel->setText(syncLevelLabels[sync]);
    syncLabel->setToolTip(syncLevelTooltip[sync]);
}

void MainWindow::updateSnrLevel(float snr)
{
    // limit SNR to progressbar maximum
    int snr10 = qRound(snr*10);
    if (snr10 > snrProgress->maximum())
    {
        snr10 = snrProgress->maximum();
    }
    snrProgress->setValue(snr10);

    snrLabel->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
    // progressbar styling -> it does not look good on Apple
    if (static_cast<int>(SNR10Threhold::SNR_BAD) > snr10)
    {   // bad SNR
#ifndef __APPLE__
        snrProgress->setStyleSheet(snrProgressStylesheet[0]);
#endif
        if (isDarkMode())
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal1_dark.png"));
        }
        else
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal1.png"));
        }
    }
    else if (static_cast<int>(SNR10Threhold::SNR_GOOD) > snr10)
    {   // medium SNR
#ifndef __APPLE__
        snrProgress->setStyleSheet(snrProgressStylesheet[1]);
#endif
        if (isDarkMode())
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal2_dark.png"));
        }
        else
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal2.png"));
        }
    }
    else
    {   // good SNR
#ifndef __APPLE__
        snrProgress->setStyleSheet(snrProgressStylesheet[2]);
#endif
        if (isDarkMode())
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal3_dark.png"));
        }
        else
        {
            basicSignalQualityLabel->setPixmap(QPixmap(":/resources/signal3.png"));
        }
    }

}

void MainWindow::updateServiceList(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry)
{
    if (slEntry.TMId != DabTMId::StreamAudio)
    {  // do nothing - data services not supported
        return;
    }

    // add to service list
    serviceList->addService(ens, slEntry);
}

void MainWindow::updateDL(const QString & dl)
{
    QString label = dl;

    label.replace(QRegularExpression(QString(QChar(0x0A))), "<br/>");
    if (label.indexOf(QChar(0x0B)) >= 0)
    {
        label.prepend("<b>");
        label.replace(label.indexOf(QChar(0x0B)), 1, "</b>");
    }
    label.remove(QChar(0x1F));

    ui->dynamicLabel->setText(label);
    if (ui->dynamicLabel->width() < ui->dynamicLabel->fontMetrics().boundingRect(label).width())
    {
        ui->dynamicLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        ui->dynamicLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    }
}

void MainWindow::updateDabTime(const QDateTime & d)
{
    timeLabel->setText(d.toString(QString("dddd, dd.MM.yyyy, hh:mm")));
}

void MainWindow::updateAudioInfo(const struct AudioParameters & params)
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

void MainWindow::onChannelSelection()
{
    //qDebug() << Q_FUNC_INFO << ui->serviceListView->hasFocus() << ui->serviceTreeView->hasFocus();

    hasListViewFocus = ui->serviceListView->hasFocus();
    hasTreeViewFocus = ui->serviceTreeView->hasFocus();
    if (InputDeviceId::RAWFILE != inputDeviceId)
    {
        ui->serviceListView->setEnabled(false);
        ui->serviceTreeView->setEnabled(false);
    }


    clearEnsembleInformationLabels();
    ui->frequencyLabel->setText("Tuning...  ");

    updateSyncStatus(uint8_t(DabSyncLevel::NoSync));
    updateSnrLevel(0);

    // hide switch to avoid conflict with tuning -> will be enabled when tune is finished
    ui->switchSourceLabel->setHidden(true);
    onServiceSelection();
}

void MainWindow::onServiceSelection()
{
    emit stopUserApps();

    clearServiceInformationLabels();
    dlDecoder->reset();
    ui->favoriteLabel->setEnabled(false);
}

void MainWindow::onChannelChange(int index)
{
    if (frequency != ui->channelCombo->itemData(index).toUInt())
    {
        // no service is selected
        SId.set(0);

        // reset UI
        ui->serviceListView->clearSelection();
        ui->serviceTreeView->clearSelection();
        onChannelSelection();

        emit serviceRequest(ui->channelCombo->itemData(index).toUInt(), 0, 0);
    }
}

void MainWindow::tuneFinished(uint32_t freq)
{  // this slot is called when tune is complete
    //qDebug() << Q_FUNC_INFO << freq;

    frequency = freq;
    if (freq != 0)
    {
        ui->serviceListView->setEnabled(true);
        ui->serviceTreeView->setEnabled(true);
        if (hasListViewFocus)
        {
            ui->serviceListView->setFocus();
        }
        else if (hasTreeViewFocus)
        {
            ui->serviceTreeView->setFocus();
        }

        ui->frequencyLabel->setText(QString("%1 MHz").arg(freq/1000.0, 3, 'f', 3, QChar('0')));
        isPlaying = true;

        // if current service has alternatives show icon immediately to avoid UI blocking when audio does not work
        if (serviceList->numEnsembles(ServiceListId(SId.value(), SCIdS)) > 1)
        {
            ui->switchSourceLabel->setVisible(true);
        }
    }
    else        
    {
        // this can only happen when device is changed, or ehen exit is requested
        if (exitRequested)
        {   // processing in IDLE, close window
            close();
            return;
        }

        ui->frequencyLabel->setText("");
        isPlaying = false;
        if (deviceChangeRequested)
        {
            initInputDevice(inputDeviceId);
        }
    }
}

void MainWindow::onInputDeviceError(const InputDeviceErrorCode errCode)
{
    qDebug() << Q_FUNC_INFO;

    switch (errCode)
    {
    case InputDeviceErrorCode::EndOfFile:
        // tune to 0
        if (!setupDialog->settings().rawfile.loopEna)
        {
            ui->channelCombo->setCurrentIndex(-1);
        }
        timeLabel->setText("End of file");
        break;
    case InputDeviceErrorCode::DeviceDisconnected:
        timeLabel->setText("Input device disconnected");
        // force no device
        setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    case InputDeviceErrorCode::NoDataAvailable:
        timeLabel->setText("No input data");
        // force no device
        setupDialog->resetInputDevice();
        changeInputDevice(InputDeviceId::UNDEFINED);
        break;
    default:
        qDebug() << Q_FUNC_INFO << "InputDevice error" << int(errCode);
    }
}

void MainWindow::serviceListClicked(const QModelIndex &index)
{
    const SLModel * model = reinterpret_cast<const SLModel*>(index.model());

    //qDebug() << current << model->getId(current) << previous << model->getId(previous);
    if (model->id(index) == ServiceListId(SId.value(), SCIdS))
    {
        return;
    }

    //qDebug() << Q_FUNC_INFO << "isService ID =" << model->getId(current);
    ServiceListConstIterator it = serviceList->findService(model->id(index));
    if (serviceList->serviceListEnd() != it)
    {
        SId = (*it)->SId();
        SCIdS = (*it)->SCIdS();
        uint32_t newFrequency = (*it)->getEnsemble()->frequency();

        if (newFrequency != frequency)
        {
           frequency = newFrequency;

            // change combo
            ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(frequency));

            // set UI to new channel tuning
            onChannelSelection();
        }
        else
        {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
            ui->switchSourceLabel->setVisible(serviceList->numEnsembles(ServiceListId(SId.value(), SCIdS)) > 1);
        }
        onServiceSelection();
        emit serviceRequest(frequency, SId.value(), SCIdS);

        // synchronize tree view with service selection
        serviceTreeViewUpdateSelection();
    }
}

void MainWindow::serviceListTreeClicked(const QModelIndex &index)
{
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(index.model());

    if (index.parent().isValid())
    {   // service, not ensemle selected
        // if both service ID and enseble ID are the same then return
        ServiceListId currentServiceId(SId.value(), SCIdS);
        ServiceListId currentEnsId(0);
        ServiceListConstIterator it = serviceList->findService(currentServiceId);
        if (serviceList->serviceListEnd() != it)
        {  // found
            currentEnsId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->id();
        }

        if ((model->id(index) == currentServiceId) && (model->id(index.parent()) == currentEnsId))
        {
            return;
        }

        it = serviceList->findService(model->id(index));
        if (serviceList->serviceListEnd() != it)
        {
            SId = (*it)->SId();
            SCIdS = (*it)->SCIdS();

            uint32_t newFrequency = (*it)->switchEnsemble(model->id(index.parent()))->frequency();
            if (newFrequency != frequency)
            {
                frequency = newFrequency;

                // change combo
                ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(frequency));

                // set UI to new channel tuning
                onChannelSelection();
            }
            else
            {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
                ui->switchSourceLabel->setVisible(serviceList->numEnsembles(ServiceListId(SId.value(), SCIdS)) > 1);
            }
            onServiceSelection();
            emit serviceRequest(frequency, SId.value(), SCIdS);

            // we need to find the item in model and select it
            serviceListViewUpdateSelection();
        }
    }
}

void MainWindow::serviceChanged(const RadioControlServiceComponent &s)
{
    if (s.isAudioService())
    {
        if (s.SId.value() != SId.value())
        {   // this can happen when service is selected while still acquiring ensemble infomation
            SId = s.SId;
            SCIdS = s.SCIdS;
        }

        if (s.label.isEmpty())
        {   // service component not valid -> shoudl not happen
            return;
        }
        // set service name in UI until information arrives from decoder

        ui->favoriteLabel->setEnabled(true);

        ServiceListId id(s);
        ServiceListConstIterator it = serviceList->findService(id);
        if (it != serviceList->serviceListEnd())
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
        SId.set(0);

        ui->serviceListView->clearSelection();
        ui->serviceTreeView->clearSelection();
        //ui->serviceListView->selectionModel()->select(ui->serviceListView->selectionModel()->selection(), QItemSelectionModel::Deselect);
    }
}

void MainWindow::audioServiceReconfiguration(const RadioControlServiceComponent &s)
{
    qDebug() << Q_FUNC_INFO << s.SId.value() << s.SId.isValid();
    if (s.SId.isValid() && s.isAudioService() && (s.SId.value() == SId.value()))
    {   // set UI
        serviceChanged(s);
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
        dlDecoder->reset();

        ui->favoriteLabel->setEnabled(false);
    }
}

void MainWindow::clearEnsembleInformationLabels()
{
    timeLabel->setText("");    
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
}

void MainWindow::onNewSettings()
{
    //qDebug() << Q_FUNC_INFO;
    SetupDialog::Settings s = setupDialog->settings();

    switch (s.inputDevice)
    {
    case InputDeviceId::RTLSDR:
        dynamic_cast<RtlSdrInput*>(inputDevice)->setGainMode(s.rtlsdr.gainMode, s.rtlsdr.gainIdx);
        break;
    case InputDeviceId::RTLTCP:
        dynamic_cast<RtlTcpInput*>(inputDevice)->setGainMode(s.rtltcp.gainMode, s.rtltcp.gainIdx);
        break;
    case InputDeviceId::RARTTCP:
        break;
    case InputDeviceId::AIRSPY:
#ifdef HAVE_AIRSPY
        dynamic_cast<AirspyInput*>(inputDevice)->setGainMode(s.airspy.gain);
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
    qDebug() << Q_FUNC_INFO << int(d);

    deviceChangeRequested = true;
    inputDeviceId = d;
    if (isPlaying)
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
    deviceChangeRequested = false;
    if (nullptr != inputDevice)
    {
        delete inputDevice;
    }

    // disable band scan - will be enable when it makes sense (RTL-SDR at the moment)
    bandScanAct->setDisabled(true);

    // disable file dumping
    ensembleInfoDialog->enableDumpToFile(false);

    switch (d)
    {
    case InputDeviceId::UNDEFINED:
        // do nothing
        inputDeviceId = InputDeviceId::UNDEFINED;
        inputDevice = nullptr;
        ui->channelCombo->setDisabled(true);   // it will be enabled when device is ready

        ui->serviceListView->setEnabled(false);
        ui->serviceTreeView->setEnabled(false);
        ui->favoriteLabel->setEnabled(false);

        break;
    case InputDeviceId::RTLSDR:
    {
        inputDevice = new RtlSdrInput();

        // signals have to be connected before calling openDevice

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // setup dialog
        connect(dynamic_cast<RtlSdrInput*>(inputDevice), &RtlSdrInput::gainListAvailable, setupDialog, &SetupDialog::setGainValues);

        if (inputDevice->openDevice())
        {  // rtl sdr is available
            inputDeviceId = InputDeviceId::RTLSDR;

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
            connect(inputDevice, &InputDevice::agcGain, ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            ensembleInfoDialog->enableDumpToFile(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<RtlSdrInput*>(inputDevice)->setBW(setupDialog->settings().rtlsdr.bandwidth);
            dynamic_cast<RtlSdrInput*>(inputDevice)->setBiasT(setupDialog->settings().rtlsdr.biasT);

            // apply current settings
            onNewSettings();
        }
        else
        {
            setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
        break;
    case InputDeviceId::RTLTCP:
    {
        inputDevice = new RtlTcpInput();

        // signals have to be connected before calling openDevice
        // RTL_TCP is opened immediately and starts receiving data

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // setup dialog
        connect(dynamic_cast<RtlTcpInput*>(inputDevice), &RtlTcpInput::gainListAvailable, setupDialog, &SetupDialog::setGainValues);

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // set IP address and port
        dynamic_cast<RtlTcpInput*>(inputDevice)->setTcpIp(setupDialog->settings().rtltcp.tcpAddress, setupDialog->settings().rtltcp.tcpPort);

        if (inputDevice->openDevice())
        {  // rtl tcp is available
            inputDeviceId = InputDeviceId::RTLTCP;

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
            connect(inputDevice, &InputDevice::agcGain, ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            ensembleInfoDialog->enableDumpToFile(true);

            // apply current settings
            onNewSettings();
        }
        else
        {
            setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
    break;
    case InputDeviceId::RARTTCP:
    {
#ifdef HAVE_RARTTCP
        inputDevice = new RartTcpInput();

        // signals have to be connected before calling isAvailable
        // RTL_TCP is opened immediately and starts receiving data

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // setup dialog
        // nothing at the moment

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
#ifdef HAVE_AIRSPY
        inputDevice = new AirspyInput(setupDialog->settings().airspy.prefer4096kHz);

        // signals have to be connected before calling isAvailable

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        if (inputDevice->openDevice())
        {  // airspy is available
            inputDeviceId = InputDeviceId::AIRSPY;

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
            connect(inputDevice, &InputDevice::agcGain, ensembleInfoDialog, &EnsembleInfoDialog::updateAgcGain);
            ensembleInfoDialog->enableDumpToFile(true);

            // these are settings that are configures in ini file manually
            // they are only set when device is initialized
            dynamic_cast<AirspyInput*>(inputDevice)->setBiasT(setupDialog->settings().airspy.biasT);
            dynamic_cast<AirspyInput*>(inputDevice)->setDataPacking(setupDialog->settings().airspy.dataPacking);

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
    case InputDeviceId::RAWFILE:
    {
        inputDevice = new RawFileInput();
        inputDeviceId = InputDeviceId::RAWFILE;

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        RawFileInputFormat format = setupDialog->settings().rawfile.format;
        dynamic_cast<RawFileInput*>(inputDevice)->setFile(setupDialog->settings().rawfile.file, format);

        // we can open device now
        if (inputDevice->openDevice())
        {  // raw file is available
            // clear service list
            serviceList->clear();

            // enable service list
            ui->serviceListView->setEnabled(true);
            ui->serviceTreeView->setEnabled(true);
            ui->favoriteLabel->setEnabled(true);

            // apply current settings
            onNewSettings();
        }
        else
        {
            setupDialog->resetInputDevice();
            initInputDevice(InputDeviceId::UNDEFINED);
        }
    }
        break;
    }
}

void MainWindow::loadSettings()
{
    QSettings * settings;
    if (iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(iniFilename, QSettings::IniFormat);
    }

    // load servicelist
    serviceList->load(*settings);
    restoreGeometry(settings->value("windowGeometry").toByteArray());
    expertMode = settings->value("ExpertMode").toBool();
    setExpertMode(expertMode);
#if (!defined HAVE_PORTAUDIO) || (AUDIOOUTPUT_PORTAUDIO_VOLUME_ENA)
    volumeSlider->setValue(settings->value("volume", 100).toInt());
#endif

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

#ifdef HAVE_RARTTCP
    s.rarttcp.tcpAddress = settings->value("RART-TCP/address", QString("127.0.0.1")).toString();
    s.rarttcp.tcpPort = settings->value("RART-TCP/port", 1235).toInt();
#endif
#ifdef HAVE_AIRSPY
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
    s.rawfile.file = settings->value("RAW-FILE/filename", QVariant(QString(""))).toString();
    s.rawfile.format = RawFileInputFormat(settings->value("RAW-FILE/format", 0).toInt());
    s.rawfile.loopEna = settings->value("RAW-FILE/loop", false).toBool();

    setupDialog->setSettings(s);

    ensembleInfoDialog->setDumpPath(settings->value("dumpPath", QVariant(QDir::homePath())).toString());

    if (InputDeviceId::UNDEFINED != static_cast<InputDeviceId>(inDevice))
    {
        initInputDevice(s.inputDevice);

        // if input device has switched to what was stored and it is RTLSDR or RTLTCP
        if ((s.inputDevice == inputDeviceId)
                && (    (InputDeviceId::RTLSDR == inputDeviceId)
                     || (InputDeviceId::AIRSPY == inputDeviceId)
                     || (InputDeviceId::RTLTCP == inputDeviceId)
                     || (InputDeviceId::RARTTCP == inputDeviceId)))
        {   // channel is only restored for RTL SDR and RTL/RART TCP at the moment
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
                    serviceListClicked(index);   // this selects service
                    break;
                }
            }
        }
    }

    delete settings;

    if (((InputDeviceId::RTLSDR == inputDeviceId) || (InputDeviceId::AIRSPY == inputDeviceId))
       && (serviceList->numServices() == 0))
    {
        QTimer::singleShot(1, this, [this](){ bandScan(); } );
    }
    if ((InputDeviceId::UNDEFINED == inputDeviceId)
        || ((InputDeviceId::RAWFILE == inputDeviceId) && (s.rawfile.file.isEmpty())))
    {
        QTimer::singleShot(1, this, [this](){ showSetupDialog(); } );
    }        
}

void MainWindow::saveSettings()
{    
    QSettings * settings;
    if (iniFilename.isEmpty())
    {
        settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    }
    else
    {
        settings = new QSettings(iniFilename, QSettings::IniFormat);
    }

    const SetupDialog::Settings s = setupDialog->settings();
    settings->setValue("inputDeviceId", int(s.inputDevice));
    settings->setValue("dumpPath", ensembleInfoDialog->getDumpPath());
#if (!defined HAVE_PORTAUDIO) || (AUDIOOUTPUT_PORTAUDIO_VOLUME_ENA)
    settings->setValue("volume", volumeSlider->value());
#else
    settings->setValue("volume", 100);
#endif
    settings->setValue("windowGeometry", saveGeometry());

    settings->setValue("RTL-SDR/gainIndex", s.rtlsdr.gainIdx);
    settings->setValue("RTL-SDR/gainMode", static_cast<int>(s.rtlsdr.gainMode));
    settings->setValue("RTL-SDR/bandwidth", s.rtlsdr.bandwidth);
    settings->setValue("RTL-SDR/bias-T", s.rtlsdr.biasT);

#ifdef HAVE_AIRSPY
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

    settings->setValue("RTL-TCP/gainIndex", s.rtltcp.gainIdx);
    settings->setValue("RTL-TCP/gainMode", static_cast<int>(s.rtltcp.gainMode));
    settings->setValue("RTL-TCP/address", s.rtltcp.tcpAddress);
    settings->setValue("RTL-TCP/port", s.rtltcp.tcpPort);

#ifdef HAVE_RARTTCP
    settings->setValue("RART-TCP/address", s.rarttcp.tcpAddress);
    settings->setValue("RART-TCP/port", s.rarttcp.tcpPort);
#endif
    settings->setValue("RAW-FILE/filename", s.rawfile.file);
    settings->setValue("RAW-FILE/format", int(s.rawfile.format));
    settings->setValue("RAW-FILE/loop", s.rawfile.loopEna);

    QModelIndex current = ui->serviceListView->currentIndex();
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListConstIterator it = serviceList->findService(model->id(current));
    if (serviceList->serviceListEnd() != it)
    {
        SId = (*it)->SId();
        SCIdS = (*it)->SCIdS();
        frequency = (*it)->getEnsemble()->frequency();

        settings->setValue("SID", SId.value());
        settings->setValue("SCIdS", SCIdS);
        settings->setValue("Frequency", frequency);
    }
    settings->setValue("ExpertMode", expertMode);

    serviceList->save(*settings);

    settings->sync();

    delete settings;
}


void MainWindow::favoriteToggled(bool checked)
{
    QModelIndex current = ui->serviceListView->currentIndex();            
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListId id = model->id(current);
    serviceList->setServiceFavorite(id, checked);

    slModel->sort(0);

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

void MainWindow::switchServiceSource()
{
    QModelIndex current = ui->serviceListView->currentIndex();
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListId id = model->id(current);
    ServiceListConstIterator it = serviceList->findService(id);
    if (it != serviceList->serviceListEnd())
    {
        // swith to next ensemble & get ensemble frequency
        uint32_t newFrequency = (*it)->switchEnsemble(ServiceListId())->frequency();
        if (newFrequency)
        {
            if (newFrequency != frequency)
            {
               frequency = newFrequency;

                // change combo
                ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(frequency));

                // set UI to new channel tuning
                onChannelSelection();
            }
            frequency = newFrequency;
            onServiceSelection();
            emit serviceRequest(frequency, SId.value(), SCIdS);

            // synchronize tree view with service selection
            serviceTreeViewUpdateSelection();
        }
    }
}

void MainWindow::serviceTreeViewUpdateSelection()
{
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(ui->serviceTreeView->model());
    ServiceListId serviceId(SId.value(), uint8_t(SCIdS));
    ServiceListId ensembleId;
    ServiceListConstIterator it = serviceList->findService(serviceId);
    if (serviceList->serviceListEnd() != it)
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
    ServiceListId id(SId.value(), uint8_t(SCIdS));
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

void MainWindow::onDLPlusToggle(bool toggle)
{
    int h = 0;
    if (height() > minimumSizeHint().height())
    {   // user changed window height
        h = height();
    }
    ui->dlPlusFrame->setVisible(toggle);

    //QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
    QTimer::singleShot(10, this, [this, h](){
        resize(width(), (h > minimumSizeHint().height()) ? h : minimumSizeHint().height());
    } );
}

void MainWindow::switchMode()
{   // toggle expert mode
    expertMode = !expertMode;
    setExpertMode(expertMode);
}

void MainWindow::showEnsembleInfo()
{
    ensembleInfoDialog->show();
    ensembleInfoDialog->raise();
    ensembleInfoDialog->activateWindow();
}

void MainWindow::showAboutDialog()
{
    //QMessageBox::aboutQt(this, tr("About QT"));
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::showSetupDialog()
{
    setupDialog->show();
    setupDialog->raise();
    setupDialog->activateWindow();
}

void MainWindow::showCatSLS()
{
    catSlsDialog->show();
    catSlsDialog->raise();
    catSlsDialog->activateWindow();
}

void MainWindow::setExpertMode(bool ena)
{
    bool doResize = false;
    if (ena)
    {
        switchModeAct->setText("Basic mode");
    }
    else
    {
       switchModeAct->setText("Expert mode");

        // going to basic mode -> windows get resized if user did not resize before
        // this always keeps minimum window size
        doResize = (size() == minimumSizeHint());
    }
    ui->channelFrame->setVisible(ena);
    ui->audioFrame->setVisible(ena);
    ui->programTypeLabel->setVisible(ena);
    ui->ensembleInfoLabel->setVisible(ena);

    ui->serviceTreeView->setVisible(ena);
    signalQualityWidget->setVisible(ena);
    timeBasicQualWidget->setCurrentIndex(ena ? 1 : 0);

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
    BandScanDialog * dialog = new BandScanDialog(this, serviceList->numServices() == 0, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    connect(dialog, &BandScanDialog::finished, dialog, &QObject::deleteLater);
    connect(dialog, &BandScanDialog::tuneChannel, this, &MainWindow::tuneChannel);
    connect(radioControl, &RadioControl::syncStatus, dialog, &BandScanDialog::onSyncStatus, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::ensembleInformation, dialog, &BandScanDialog::onEnsembleFound, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::tuneDone, dialog, &BandScanDialog::tuneFinished, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::ensembleComplete, dialog, &BandScanDialog::onEnsembleComplete, Qt::QueuedConnection);
    connect(serviceList, &ServiceList::serviceAdded, dialog, &BandScanDialog::onServiceFound);
    connect(dialog, &BandScanDialog::scanStarts, this, &MainWindow::clearServiceList);
    connect(dialog, &BandScanDialog::finished, this, &MainWindow::bandScanFinished);

    dialog->open();
}

void MainWindow::bandScanFinished(int result)
{
    qDebug() << Q_FUNC_INFO;
    switch (result)
    {
    case BandScanDialogResult::Done:
    case BandScanDialogResult::Interrupted:
    {
        if (serviceList->numServices() > 0)
        {   // going to select first service
            const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
            QModelIndex index = model->index(0, 0);
            ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
            serviceListClicked(index);   // this selects service
            ui->serviceListView->setFocus();
        }
    }
        break;
    case BandScanDialogResult::Cancelled:
        // do nothing
        break;
    }
}

void MainWindow::tuneChannel(uint32_t freq)
{
    // change combo - find combo index
    ui->channelCombo->setCurrentIndex(ui->channelCombo->findData(freq));
}

void MainWindow::stop()
{
    if (isPlaying)
    { // stop
        // tune to 0
        ui->channelCombo->setCurrentIndex(-1);
    }
}

void MainWindow::clearServiceList()
{
    stop();
    serviceList->clear();
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
        settingsLabel->setIcon(":/resources/menu_dark.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty_dark.png", false);
        ui->favoriteLabel->setIcon(":/resources/star_dark.png", true);

        muteLabel->setIcon(":/resources/volume_on_dark.png", false);
        muteLabel->setIcon(":/resources/volume_off_dark.png", true);

        ui->dlPlusLabel->setIcon(":/resources/down_dark.png", false);
        ui->dlPlusLabel->setIcon(":/resources/up_dark.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls_dark.png");

        ui->switchSourceLabel->setIcon(":/resources/broadcast_dark.png");
    }
    else
    {
        settingsLabel->setIcon(":/resources/menu.png");

        ui->favoriteLabel->setIcon(":/resources/starEmpty.png", false);
        ui->favoriteLabel->setIcon(":/resources/star.png", true);

        muteLabel->setIcon(":/resources/volume_on.png", false);
        muteLabel->setIcon(":/resources/volume_off.png", true);

        ui->dlPlusLabel->setIcon(":/resources/down.png", false);
        ui->dlPlusLabel->setIcon(":/resources/up.png", true);

        ui->catSlsLabel->setIcon(":/resources/catSls.png");

        ui->switchSourceLabel->setIcon(":/resources/broadcast.png");
    }
    ui->slsView->setDarkMode(isDarkMode());
}

void MainWindow::onDLPlusObjReceived(const DLPlusObject & object)
{
    //qDebug() << Q_FUNC_INFO << object.getType() << object.getTag();

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
        qDebug() << "DL+ delete object" << object.getType();

        const auto it = dlObjCache.constFind(object.getType());
        if (it != dlObjCache.cend())
        {   // object type found in cache
            DLPlusObjectUI* uiObj = dlObjCache.take(object.getType());
            delete uiObj;
        }
        else
        { /* not in cache, do nothing */ }
    }
    else
    {
        auto it = dlObjCache.find(object.getType());
        if (it != dlObjCache.end())
        {   // object type found in cahe
            (*it)->update(object);
        }
        else
        {   // not in cache yet -> insert
            DLPlusObjectUI * uiObj = new DLPlusObjectUI(object);
            if (nullptr != uiObj->getLayout())
            {
                it = dlObjCache.insert(object.getType(), uiObj);

                // we want it sorted -> need to find the position
                int index = std::distance(dlObjCache.begin(), it);

                QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->dlPlusFrame->layout());
                layout->insertLayout(index, uiObj->getLayout());
                ui->dlPlusFrame->setMaximumHeight(ui->dlPlusFrame->minimumHeight() > 120 ? ui->dlPlusFrame->minimumHeight() : 120);
            }
            else
            { /* object the we do not display: Descriptors, PROGRAMME_FREQUENCY, INFO_DATE_TIME, PROGRAMME_SUBCHANNEL*/ }
        }
    }
}

void MainWindow::onDLPlusItemToggle()
{
    //qDebug() << Q_FUNC_INFO;
    // delete all ITEMS.*
    auto it = dlObjCache.cbegin();
    while (it != dlObjCache.cend())
    {
        if (it.key() < DLPlusContentType::INFO_NEWS)
        {
            DLPlusObjectUI* uiObj = it.value();
            delete uiObj;
            it = dlObjCache.erase(it);
        }
        else
        {   // no more items ot ITEM type in cache
            break;
        }
    }
}

void MainWindow::onDLPlusItemRunning(bool isRunning)
{
    //qDebug() << Q_FUNC_INFO << isRunning;
    auto it = dlObjCache.cbegin();
    while (it != dlObjCache.cend())
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

void MainWindow::onDLReset()
{
    ui->dynamicLabel->setText("");
    ui->dlPlusLabel->setVisible(false);
    ui->dlPlusLabel->setChecked(false);
    onDLPlusToggle(false);

    for (auto objPtr : dlObjCache)
    {
        delete objPtr;
    }
    dlObjCache.clear();
}

DLPlusObjectUI::DLPlusObjectUI(const DLPlusObject &obj) : dlPlusObject(obj)
{
    //qDebug() << Q_FUNC_INFO;

    layout = new QHBoxLayout();
    QString label = getLabel(obj.getType());
    if (label.isEmpty())
    {
        layout = nullptr;
    }
    else
    {
        tagLabel = new QLabel(QString("<b>%1:  </b>").arg(label));
        tagLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
        tagLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        layout->addWidget(tagLabel);


        switch (obj.getType())
        {
        case DLPlusContentType::PROGRAMME_HOMEPAGE:
        case DLPlusContentType::INFO_URL:           
            tagText = new QLabel(QString("<a href=\"%1\">%1</a>").arg(obj.getTag()));
            QObject::connect(
                        tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        case DLPlusContentType::EMAIL_HOTLINE:
        case DLPlusContentType::EMAIL_STUDIO:
        case DLPlusContentType::EMAIL_OTHER:
            tagText = new QLabel(QString("<a href=\"mailto:%1\">%1</a>").arg(obj.getTag()));
            QObject::connect(
                        tagText, &QLabel::linkActivated,
                        [=]( const QString & link ) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
                    );
            break;
        default:
            tagText = new QLabel(obj.getTag());
        }

        tagText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        tagText->setWordWrap(true);
        tagText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        layout->addWidget(tagText);
    }
}

DLPlusObjectUI::~DLPlusObjectUI()
{

    if (nullptr != layout)
    {
        delete tagLabel;
        delete tagText;
        delete layout;
    }
}

QHBoxLayout *DLPlusObjectUI::getLayout() const
{
    return layout;
}

void DLPlusObjectUI::update(const DLPlusObject &obj)
{
    if (obj != dlPlusObject)
    {
        dlPlusObject = obj;
        tagText->setText(obj.getTag());
    }
}

void DLPlusObjectUI::setVisible(bool visible)
{
    tagLabel->setVisible(visible);
    tagText->setVisible(visible);
}

const DLPlusObject &DLPlusObjectUI::getDlPlusObject() const
{
    return dlPlusObject;
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
