#include <QString>
#include <QFileDialog>
#include <QDir>
#include <QMap>
#include <QDebug>
#include <QGraphicsScene>
#include <QToolButton>
#include <QFormLayout>
#include <QSettings>

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "dabtables.h"
#include "radiocontrol.h"
#include "bandscandialog.h"

#define USE_SLS_FRAME 1

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    SId.value = 0;
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId();
    dlDecoder = new DLDecoder();

    ui->setupUi(this);

    // set UI
    setWindowTitle("Abraca DAB Radio");

    // favorites control
    ui->favoriteLabel->setCheckable(true);
    ui->favoriteLabel->setIcon(":/resources/starEmpty20.png", false);
    ui->favoriteLabel->setIcon(":/resources/star20.png", true);
    ui->favoriteLabel->setTooltip("Click to add service to favourites", false);
    ui->favoriteLabel->setTooltip("Click to remove service from favourites", true);
    ui->favoriteLabel->setChecked(false);


    setupDialog = new SetupDialog(this);
    connect(setupDialog, &SetupDialog::inputDeviceChanged, this, &MainWindow::changeInputDevice);
    connect(setupDialog, &SetupDialog::fileLoopingEnabled, this, &MainWindow::enableFileLooping);
    connect(setupDialog, &SetupDialog::rawFileStop, this, &MainWindow::onRawFileStop);

    ensembleInfoDialog = new EnsembleInfoDialog(this);

    // status bar
    QWidget * widget = new QWidget();
    timeLabel = new QLabel("");
    timeLabel->setToolTip("DAB time");

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

    updateSnrLevel(0);
    updateSyncStatus(uint8_t(DabSyncLevel::NoSync));

    setupAct = new QAction("Setup...", this);
    //setupAct->setStatusTip("Application settings");       // this is shown in status bar
    connect(setupAct, &QAction::triggered, setupDialog, &SetupDialog::show);

    clearServiceListAct = new QAction("Clear service list", this);
    connect(clearServiceListAct, &QAction::triggered, this, &MainWindow::clearServiceList);

    bandScanAct = new QAction("Band scan...", this);
    //scanAct->setStatusTip("Seach for available service"); // this is shown in status bar
    connect(bandScanAct, &QAction::triggered, this, &MainWindow::bandScan);

    switchModeAct = new QAction("Expert mode", this);
    connect(switchModeAct, &QAction::triggered, this, &MainWindow::switchMode);

    ensembleInfoAct = new QAction("Ensemble Info", this);
    connect(ensembleInfoAct, &QAction::triggered, this, &MainWindow::showEnsembleInfo);

    catSlsAct = new QAction("Categorized SLS", this);
    connect(catSlsAct, &QAction::triggered, this, &MainWindow::showCatSLS);

    menu = new QMenu(this);
    menu->addAction(setupAct);
    menu->addAction(bandScanAct);
    menu->addAction(switchModeAct);
    menu->addAction(ensembleInfoAct);
    menu->addAction(clearServiceListAct);
    menu->addAction(catSlsAct);

    QPixmap pic;
    ClickableLabel * settingsLabel = new ClickableLabel(this);
    settingsLabel->setIcon(":/resources/menu.png");
    settingsLabel->setToolTip("Open menu");
    settingsLabel->setMenu(menu);

    ClickableLabel * muteLabel = new ClickableLabel(this);
    muteLabel->setCheckable(true);
    muteLabel->setIcon(":/resources/volume_on.png", false);
    muteLabel->setIcon(":/resources/volume_off.png", true);
    muteLabel->setTooltip("Mute audio", false);
    muteLabel->setTooltip("Unmute audio", true);
    muteLabel->setChecked(false);


    QGridLayout * layout = new QGridLayout(widget);
    layout->addWidget(timeLabel, 0, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addLayout(signalQualityLayout, 0, 1, Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(muteLabel, 0, 2, Qt::AlignVCenter | Qt::AlignRight);
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
    connect(serviceList, &ServiceList::empty, slModel, &SLModel::clear);

    ui->serviceListView->setModel(slModel);
    ui->serviceListView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(ui->serviceListView, &QListView::clicked, this, &MainWindow::serviceListClicked);

    slTreeModel = new SLTreeModel(serviceList, this);
    connect(serviceList, &ServiceList::serviceAddedToEnsemble, slTreeModel, &SLTreeModel::addEnsembleService);
    ui->serviceTreeView->setModel(slTreeModel);
    ui->serviceTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->serviceTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->serviceTreeView->setHeaderHidden(true);
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

#if USE_SLS_FRAME
    ui->slsView->setMinimumSize(QSize(322, 242));
#else
    ui->slsView->setMinimumSize(QSize(320, 240));
    ui->slsView->setFrameStyle(QFrame::NoFrame);
#endif    

    if (pic.load(":/resources/sls_logo.png"))
    {
        QGraphicsScene * scene = ui->slsView->scene();
        if (nullptr == scene)
        {
            scene = new QGraphicsScene(this);
            slsPixmapItem = scene->addPixmap(pic);

            ui->slsView->setScene(scene);
        }
        else
        {
            slsPixmapItem->setPixmap(pic);
        }
        scene->setSceneRect(pic.rect());
    }
    else
    {
        qDebug() << "Unable to load :/resources/sls_logo.png";
    }

    if (pic.load(":/resources/broadcast.png"))
    {
        ui->switchSourceLabel->setPixmap(pic);
    }
    else
    {
        qDebug() << "Unable to load :/resources/broadcast.png";
    }
    ui->switchSourceLabel->setToolTip("Click to change service source (ensemble)");
    ui->switchSourceLabel->setHidden(true);

    ui->serviceTreeView->setVisible(false);
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
#if (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    audioOutThr = new QThread(this);    
    audioOutThr->setObjectName("audioOutThr");
    audioOutput->moveToThread(audioOutThr);
    connect(audioOutThr, &QThread::finished, audioOutput, &QObject::deleteLater);
    audioOutThr->start();
    audioOutThr->setPriority(QThread::HighestPriority);
#endif

    // Connect signals
    connect(muteLabel, &ClickableLabel::toggled, audioOutput, &AudioOutput::mute, Qt::QueuedConnection);
    connect(ui->favoriteLabel, &ClickableLabel::toggled, this, &MainWindow::favoriteToggled);
    connect(ui->switchSourceLabel, &ClickableLabel::clicked, this, &MainWindow::switchServiceSource);    

    connect(radioControl, &RadioControl::ensembleInformation, this, &MainWindow::updateEnsembleInfo, Qt::QueuedConnection);
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
    connect(radioControl, &RadioControl::serviceChanged, ensembleInfoDialog, &EnsembleInfoDialog::resetMscStat, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::newServiceSelection, ensembleInfoDialog, &EnsembleInfoDialog::serviceChanged, Qt::QueuedConnection);

    connect(radioControl, &RadioControl::dlDataGroup, dlDecoder, &DLDecoder::newDataGroup, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::newServiceSelection, this, &MainWindow::serviceChanged, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::serviceChanged, dlDecoder, &DLDecoder::reset, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::audioData, audioDecoder, &AudioDecoder::inputData, Qt::QueuedConnection);
    connect(dlDecoder, &DLDecoder::dlComplete, this, &MainWindow::updateDL, Qt::QueuedConnection);

    connect(audioDecoder, &AudioDecoder::audioParametersInfo, this, &MainWindow::updateAudioInfo, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::newServiceSelection, audioDecoder, &AudioDecoder::start, Qt::QueuedConnection);

    // audio output is controlled by signals from decoder
    connect(this, &MainWindow::serviceRequest, audioDecoder, &AudioDecoder::stop, Qt::QueuedConnection);
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
    connect(radioControl, &RadioControl::newServiceSelection, slideShowApp, &SlideShowApp::start);
    connect(radioControl, &RadioControl::serviceChanged, slideShowApp, &SlideShowApp::start);
    connect(slideShowApp, &SlideShowApp::currentSlide, this, &MainWindow::updateSLS, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::resetTerminal, this, &MainWindow::resetSLS, Qt::QueuedConnection);
    connect(this, &MainWindow::stopUserApps, slideShowApp, &SlideShowApp::stop, Qt::QueuedConnection);

    catSlsDialog = new CatSLSDialog(this);
    connect(slideShowApp, &SlideShowApp::categoryUpdate, catSlsDialog, &CatSLSDialog::onCategoryUpdate, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::catSlide, catSlsDialog, &CatSLSDialog::onCatSlide, Qt::QueuedConnection);
    connect(slideShowApp, &SlideShowApp::resetTerminal, catSlsDialog, &CatSLSDialog::reset, Qt::QueuedConnection);
    connect(catSlsDialog, &CatSLSDialog::getCurrentCatSlide, slideShowApp, &SlideShowApp::getCurrentCatSlide, Qt::QueuedConnection);
    connect(catSlsDialog, &CatSLSDialog::getNextCatSlide, slideShowApp, &SlideShowApp::getNextCatSlide, Qt::QueuedConnection);    

    // input device connections
    initInputDevice(InputDeviceId::UNDEFINED);

    loadSettings();
}

MainWindow::~MainWindow()
{
    qDebug() << Q_FUNC_INFO;

    delete inputDevice;

    radioControlThr->quit();  // this deletes radioControl
    radioControlThr->wait();
    delete radioControlThr;

    audioDecoderThr->quit();  // this deletes audiodecoder
    audioDecoderThr->wait();
    delete audioDecoderThr;

#if (defined AUDIOOUTPUT_USE_RTAUDIO) || (defined AUDIOOUTPUT_USE_PORTAUDIO)
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (0 == frequency)
    {  // in idle
        qDebug() << Q_FUNC_INFO << "requesting exit";

        saveSettings();

        emit exit();
        event->accept();
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "going to IDLE";
        exitRequested = true;
        emit serviceRequest(0,0,0);
        event->ignore();
    }

/*
    saveSettings();

    qDebug() << Q_FUNC_INFO << "stopping all processing";

    emit serviceRequest(0,0,0);
    //QThread::sleep(1);
    emit exit();
    event->accept();
*/
}


void MainWindow::showEvent(QShowEvent *event)
{
    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr != scene)
    {
        ui->slsView->fitInViewTight(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    QMainWindow::showEvent(event);
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

    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr != scene)
    {
        ui->slsView->fitInViewTight(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    QMainWindow::resizeEvent(event);
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
                                  .arg(QString("%1").arg(ens.ecc, 2, 16, QChar('0')).toUpper())
                                  .arg(QString("%1").arg(ens.eid, 4, 16, QChar('0')).toUpper())
                                  .arg(DabTables::getCountryName(ens.ueid)));
}

void MainWindow::updateSyncStatus(uint8_t sync)
{
    if (DabSyncLevel::FullSync > DabSyncLevel(sync))
    {   // hide time when no sync
        timeLabel->setText("");
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
#ifndef __APPLE__
    // progressbar styling -> it does not look good on Apple
    if (static_cast<int>(SNR10Threhold::SNR_BAD) > snr10)
    {   // bad SNR
        snrProgress->setStyleSheet(snrProgressStylesheet[0]);
    }
    else if (static_cast<int>(SNR10Threhold::SNR_GOOD) > snr10)
    {   // medium SNR
        snrProgress->setStyleSheet(snrProgressStylesheet[1]);
    }
    else
    {   // good SNR
        snrProgress->setStyleSheet(snrProgressStylesheet[2]);
    }
#endif
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
    ui->dynamicLabel->setText(dl);
    if (ui->dynamicLabel->width() < ui->dynamicLabel->fontMetrics().boundingRect(dl).width())
    {
        ui->dynamicLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else
    {
        ui->dynamicLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    }
}

void MainWindow::updateSLS(const Slide &slide)
{
    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr == scene)
    {
        //qDebug() << Q_FUNC_INFO << "New graphisc scene";
        scene = new QGraphicsScene(this);
        slsPixmapItem = scene->addPixmap(slide.getPixmap());

        ui->slsView->setScene(scene);
    }
    else
    {
        slsPixmapItem->setPixmap(slide.getPixmap());
    }

    scene->setSceneRect(slide.getPixmap().rect());
    scene->setBackgroundBrush(Qt::black);
    ui->slsView->fitInViewTight(slide.getPixmap().rect(), Qt::KeepAspectRatio);

    // update tool tip
    QString toolTip = QString("<b>ContentName:</b> %1").arg(slide.getContentName());
    if (0 != slide.getCategoryID())
    {
        toolTip += QString("<br><b>Category:</b> %1 (%2)").arg(slide.getCategoryTitle())
                                                          .arg(slide.getCategoryID());
    }
    else
    { /* no catSLS */ }
    if (!slide.getClickThroughURL().isEmpty())
    {
        toolTip += QString("<br><b>ClickThroughURL:</b> %1").arg(slide.getClickThroughURL());
    }
    else
    { /* not present */ }
    ui->slsView->setToolTip(toolTip);
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
    clearServiceInformationLabels();
    dlDecoder->reset();
    ui->dynamicLabel->setText("");    

    emit stopUserApps();
}

void MainWindow::resetSLS()
{
    QPixmap pic;
    if (pic.load(":/resources/sls_logo.png"))
    {
        QGraphicsScene * scene = ui->slsView->scene();
        if (nullptr == scene)
        {
            //qDebug() << Q_FUNC_INFO << "New graphisc scene";
            scene = new QGraphicsScene(this);
            slsPixmapItem = scene->addPixmap(pic);

            ui->slsView->setScene(scene);
        }
        else
        {
            slsPixmapItem->setPixmap(pic);
        }
        scene->setSceneRect(pic.rect());
        scene->setBackgroundBrush(Qt::white);
        ui->slsView->fitInViewTight(pic.rect(), Qt::KeepAspectRatio);
    }
    else
    {
        qDebug() << "Unable to load :/resources/sls_logo.png";
    }

    ui->slsView->setToolTip("");
}

void MainWindow::on_channelCombo_currentIndexChanged(int index)
{
    if (frequency != ui->channelCombo->itemData(index).toUInt())
    {
        ui->channelLabel->setText(ui->channelCombo->currentText());

        // no service is selected
        SId.value = 0;

        // reset UI
        onChannelSelection();

        emit serviceRequest(ui->channelCombo->itemData(index).toUInt(), 0, 0);
    }
}

void MainWindow::tuneFinished(uint32_t freq)
{  // this slot is called when tune is complete
    qDebug() << Q_FUNC_INFO << freq;

    frequency = freq;
    if (freq != 0)
    {
        ui->frequencyLabel->setText(QString("%1 MHz").arg(freq/1000.0, 3, 'f', 3, QChar('0')));
        isPlaying = true;
        setupDialog->enableFileSelection(false);

        // if current service has alternatives show icon immediately to avoid UI blocking when audio does not work
        if (serviceList->numEnsembles(ServiceListItem::getId(SId.value, SCIdS)) > 1)
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
        setupDialog->enableFileSelection(true);
        if (deviceChangeRequested)
        {
            initInputDevice(inputDeviceId);
        }
    }
}

void MainWindow::onRawFileStop()
{
    timeLabel->setText("End of file");
    // tune to 0
    ui->channelCombo->setCurrentIndex(-1);
}

void MainWindow::onInputDeviceError(const InputDeviceErrorCode errCode)
{
    qDebug() << Q_FUNC_INFO;

    switch (errCode)
    {
    case InputDeviceErrorCode::EndOfFile:
        timeLabel->setText("End of file");
        // tune to 0
        if (!fileLooping)
        {
            ui->channelCombo->setCurrentIndex(-1);
            setupDialog->enableFileSelection(true);
        }
        break;
    case InputDeviceErrorCode::DeviceDisconnected:
        timeLabel->setText("Input device disconnected");
        // force no device
        setupDialog->setInputDevice(InputDeviceId::UNDEFINED);
        break;
    case InputDeviceErrorCode::NoDataAvailable:
        timeLabel->setText("No input data");
        // force no device
        setupDialog->setInputDevice(InputDeviceId::UNDEFINED);
        break;
    default:
        qDebug() << Q_FUNC_INFO << "InputDevice error" << int(errCode);
    }
}

void MainWindow::serviceListClicked(const QModelIndex &index)
{
    const SLModel * model = reinterpret_cast<const SLModel*>(index.model());

    //qDebug() << current << model->getId(current) << previous << model->getId(previous);
    if (model->getId(index) == ServiceListItem::getId(SId.value, SCIdS))
    {
        return;
    }

    //qDebug() << Q_FUNC_INFO << "isService ID =" << model->getId(current);
    ServiceListConstIterator it = serviceList->findService(model->getId(index));
    if (serviceList->serviceListEnd() != it)
    {
        SId = (*it)->SId();
        SCIdS = (*it)->SCIdS();
        uint32_t newFrequency = (*it)->getEnsemble()->frequency();

        if (newFrequency != frequency)
        {
           frequency = newFrequency;

            // change combo
            int idx = 0;
            dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
            while (it != DabTables::channelList.constEnd()) {
                if (it.key() == frequency)
                {
                    ui->channelLabel->setText(it.value());
                    break;
                }
                ++it;
                ++idx;
            }
            ui->channelCombo->setCurrentIndex(idx);

            // set UI to new channel tuning
            onChannelSelection();
        }
        else
        {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
            ui->switchSourceLabel->setVisible(serviceList->numEnsembles(ServiceListItem::getId(SId.value, SCIdS)) > 1);
        }
        onServiceSelection();
        emit serviceRequest(frequency, SId.value, SCIdS);

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
        uint64_t currentServiceId = ServiceListItem::getId(SId.value, SCIdS);
        uint64_t currentEnsId = 0;
        ServiceListConstIterator it = serviceList->findService(currentServiceId);
        if (serviceList->serviceListEnd() != it)
        {  // found
            currentEnsId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->getId();
        }

        if ((model->getId(index) == currentServiceId)
                && model->getId(index.parent()) == currentEnsId)
        {
            return;
        }

        it = serviceList->findService(model->getId(index));
        if (serviceList->serviceListEnd() != it)
        {
            SId = (*it)->SId();
            SCIdS = (*it)->SCIdS();

            uint32_t newFrequency = (*it)->switchEnsemble(model->getId(index.parent()))->frequency();
            if (newFrequency != frequency)
            {
                frequency = newFrequency;

                // change combo
                int idx = 0;
                dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
                while (it != DabTables::channelList.constEnd()) {
                    if (it.key() == frequency)
                    {
                        ui->channelLabel->setText(it.value());
                        break;
                    }
                    ++it;
                    ++idx;
                }
                ui->channelCombo->setCurrentIndex(idx);

                // set UI to new channel tuning
                onChannelSelection();
            }
            else
            {   // if new service has alternatives show icon immediately to avoid UI blocking when audio does not work
                ui->switchSourceLabel->setVisible(serviceList->numEnsembles(ServiceListItem::getId(SId.value, SCIdS)) > 1);
            }
            onServiceSelection();
            qDebug() << "serviceRequest(frequency, SId.value, SCIdS)" << frequency << SId.value << SCIdS;
            emit serviceRequest(frequency, SId.value, SCIdS);

            // we need to find the item in model and select it
            serviceListViewUpdateSelection();
        }
    }
}

void MainWindow::serviceChanged(const RadioControlServiceComponent &s)
{
    if (s.isAudioService() && (s.SId.value == SId.value))
    {
        if (s.label.isEmpty())
        {   // service component not valid -> shoudl not happen
            return;
        }
        // set service name in UI until information arrives from decoder

        uint64_t id = ServiceListItem::getId(s);
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
                                     .arg(QString("%1").arg(s.SId.prog.countryServiceRef, 4, 16, QChar('0')).toUpper() )
                                     .arg(s.SCIdS)
                                     .arg(DabTables::getLangName(s.lang))
                                     .arg(DabTables::getCountryName(s.SId.value)));        

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
    {   // sid it not equal to selected sid -> should not happen
        ui->serviceListView->clearSelection();
        return;
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

void MainWindow::changeInputDevice(const InputDeviceId & d)
{
    qDebug() << Q_FUNC_INFO << int(d);
    if (d != inputDeviceId)
    {
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
}

void MainWindow::initInputDevice(const InputDeviceId & d)
{
    qDebug() << Q_FUNC_INFO << int(d);

    deviceChangeRequested = false;
    if (nullptr != inputDevice)
    {
        delete inputDevice;

        // delete service list
        //serviceList->clear();
    }

    // disable band scan - will be enable when it makes sense (RTL-SDR at the moment)
    bandScanAct->setDisabled(true);

    // disable file dumping
    ensembleInfoDialog->enableDumpToFile(false);

    switch (d)
    {
    case InputDeviceId::UNDEFINED:
        // do nothing
        inputDevice = nullptr;
        ui->channelCombo->setDisabled(true);   // it will be enabled when device is ready
        ui->channelLabel->setText("");
        //setupDialog->setInputDevice(inputDeviceId); // this emits device change

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
        connect(setupDialog, &SetupDialog::setGainMode, dynamic_cast<RtlSdrInput*>(inputDevice), &RtlSdrInput::setGainMode);
        connect(setupDialog, &SetupDialog::setDAGC, dynamic_cast<RtlSdrInput*>(inputDevice), &RtlSdrInput::setDAGC);

        if (inputDevice->openDevice())
        {  // rtl sdr is available
            inputDeviceId = InputDeviceId::RTLSDR;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change

            static_cast<RtlSdrInput *>(inputDevice)->setDAGC(setupDialog->getDAGCState());

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
        }
        else
        {
            delete inputDevice;
            inputDevice = nullptr;
            inputDeviceId = InputDeviceId::UNDEFINED;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change
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
        connect(setupDialog, &SetupDialog::setGainMode, dynamic_cast<RtlTcpInput*>(inputDevice), &RtlTcpInput::setGainMode);
        connect(setupDialog, &SetupDialog::setDAGC, dynamic_cast<RtlTcpInput*>(inputDevice), &RtlTcpInput::setDAGC);

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        if (inputDevice->openDevice())
        {  // rtl tcp is available
            inputDeviceId = InputDeviceId::RTLTCP;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change

            static_cast<RtlTcpInput *>(inputDevice)->setDAGC(setupDialog->getDAGCState());

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
        }
        else
        {
            delete inputDevice;
            inputDevice = nullptr;
            inputDeviceId = InputDeviceId::UNDEFINED;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change
        }
    }
    break;
    case InputDeviceId::RARTTCP:
    {
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

        if (inputDevice->openDevice())
        {  // rtl tcp is available
            inputDeviceId = InputDeviceId::RARTTCP;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change

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
        }
        else
        {
            delete inputDevice;
            inputDevice = nullptr;
            inputDeviceId = InputDeviceId::UNDEFINED;
            setupDialog->setInputDevice(inputDeviceId); // this emits device change
        }
    }
    break;
    case InputDeviceId::RAWFILE:
    {
        inputDevice = new RawFileInput();
        inputDeviceId = InputDeviceId::RAWFILE;
        setupDialog->setInputDevice(inputDeviceId); // this emits device change

        // tuning procedure
        connect(radioControl, &RadioControl::tuneInputDevice, inputDevice, &InputDevice::tune, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::tuned, radioControl, &RadioControl::start, Qt::QueuedConnection);

        // HMI
        connect(inputDevice, &InputDevice::deviceReady, this, &MainWindow::inputDeviceReady, Qt::QueuedConnection);
        connect(inputDevice, &InputDevice::error, this, &MainWindow::onInputDeviceError, Qt::QueuedConnection);

        // setup dialog
        connect(setupDialog, &SetupDialog::rawFileSelected, dynamic_cast<RawFileInput*>(inputDevice), &RawFileInput::openFile);
        connect(setupDialog, &SetupDialog::sampleFormat, dynamic_cast<RawFileInput*>(inputDevice), &RawFileInput::setFileFormat);

        QString filename = setupDialog->getInputFileName();
        if (!filename.isEmpty())
        {
            RawFileInputFormat format = setupDialog->getInputFileFormat();
            dynamic_cast<RawFileInput*>(inputDevice)->openFile(filename, format);
            enableFileLooping(setupDialog->isFileLoopActive());
        }
        setupDialog->enableFileSelection(true);

        // clear service list
        serviceList->clear();

        // enable service list
        ui->serviceListView->setEnabled(true);
        ui->serviceTreeView->setEnabled(true);
        ui->favoriteLabel->setEnabled(true);
    }
        break;
    }
}

void MainWindow::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, appName, appName);

    // load servicelist
    serviceList->load(settings);
    expertMode = settings.value("ExpertMode").toBool();
    setExpertMode(expertMode);
    int inDevice = settings.value("inputDeviceId", int(InputDeviceId::UNDEFINED)).toInt();
    if (InputDeviceId::UNDEFINED != static_cast<InputDeviceId>(inDevice))
    {        
        // if input device has switched to what was stored and it is RTLSDR or RTLTCP
        setupDialog->setInputDevice(static_cast<InputDeviceId>(inDevice));
        if ((static_cast<InputDeviceId>(inDevice) == inputDeviceId)
                && (    (InputDeviceId::RTLSDR == inputDeviceId)
                     || (InputDeviceId::RTLTCP == inputDeviceId)
                     || (InputDeviceId::RARTTCP == inputDeviceId)))
        {   // channel is only restored for RTL SDR and RTL/RART TCP at the moment
            int sid = settings.value("SID", 0).toInt();
            int scids = settings.value("SCIdS", 0).toInt();
            uint64_t id = ServiceListItem::getId(sid, scids);

            // we need to find the item in model and select it
            const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
            QModelIndex index;
            for (int r = 0; r < model->rowCount(); ++r)
            {
                index = model->index(r, 0);
                if (model->getId(index) == id)
                {   // found
                    ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select | QItemSelectionModel::Current);
                    serviceListClicked(index);   // this selects service
                    ui->serviceListView->setFocus();
                    break;
                }
            }
        }
    }

    // load this afetr device is selected to configure file input correctly
    setupDialog->setInputFile(settings.value("inputFileName", QVariant(QString(""))).toString(),
                              RawFileInputFormat(settings.value("inputFileFormat", 0).toInt()),
                              settings.value("inputFileLoop", false).toBool());
    setupDialog->setDAGCState(settings.value("DAGC", false).toBool());
    setupDialog->setGainIdx(settings.value("gainIndex", 1).toInt());

    ensembleInfoDialog->setDumpPath(settings.value("dumpPath", QVariant(QDir::homePath())).toString());
}

void MainWindow::saveSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, appName, appName);
    settings.setValue("inputDeviceId", int(inputDeviceId));
    settings.setValue("inputFileName", setupDialog->getInputFileName());
    settings.setValue("inputFileFormat", int(setupDialog->getInputFileFormat()));
    settings.setValue("inputFileLoop", setupDialog->isFileLoopActive());
    settings.setValue("DAGC", setupDialog->getDAGCState());
    settings.setValue("gainIndex", setupDialog->getGainIdx());
    settings.setValue("dumpPath", ensembleInfoDialog->getDumpPath());

    QModelIndex current = ui->serviceListView->currentIndex();
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    ServiceListConstIterator it = serviceList->findService(model->getId(current));
    if (serviceList->serviceListEnd() != it)
    {
        SId = (*it)->SId();
        SCIdS = (*it)->SCIdS();
        frequency = (*it)->getEnsemble()->frequency();

        settings.setValue("SID", SId.value);
        settings.setValue("SCIdS", SCIdS);
        settings.setValue("Frequency", frequency);
    }
    settings.setValue("ExpertMode", expertMode);

    serviceList->save(settings);

    settings.sync();
}


void MainWindow::favoriteToggled(bool checked)
{
    QModelIndex current = ui->serviceListView->currentIndex();            
    const SLModel * model = reinterpret_cast<const SLModel*>(current.model());
    uint64_t id = model->getId(current);
    serviceList->setServiceFavorite(id, checked);

    slModel->sort(0);

    // find new position of current service and select it
    QModelIndex index;
    for (int r = 0; r < model->rowCount(); ++r)
    {
        index = model->index(r, 0);
        if (model->getId(index) == id)
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
    uint64_t id = model->getId(current);
    ServiceListConstIterator it = serviceList->findService(id);
    if (it != serviceList->serviceListEnd())
    {
        // swith to next ensemble &  get ensemble frequency
        uint32_t newFrequency = (*it)->switchEnsemble()->frequency();
        if (newFrequency)
        {
            if (newFrequency != frequency)
            {
               frequency = newFrequency;

                // change combo
                int idx = 0;
                dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
                while (it != DabTables::channelList.constEnd()) {
                    if (it.key() == frequency)
                    {
                        ui->channelLabel->setText(it.value());
                        break;
                    }
                    ++it;
                    ++idx;
                }
                ui->channelCombo->setCurrentIndex(idx);

                // set UI to new channel tuning
                onChannelSelection();
            }
            frequency = newFrequency;
            onServiceSelection();
            emit serviceRequest(frequency, SId.value, SCIdS);

            // synchronize tree view with service selection
            serviceTreeViewUpdateSelection();
        }
    }
}

void MainWindow::serviceTreeViewUpdateSelection()
{
    const SLTreeModel * model = reinterpret_cast<const SLTreeModel*>(ui->serviceTreeView->model());
    uint64_t serviceId = ServiceListItem::getId(SId.value, SCIdS);
    uint64_t ensembleId = 0;
    ServiceListConstIterator it = serviceList->findService(serviceId);
    if (serviceList->serviceListEnd() != it)
    {  // found
        ensembleId = (*it)->getEnsemble((*it)->currentEnsembleIdx())->getId();
    }
    for (int r = 0; r < model->rowCount(); ++r)
    {   // go through ensembles
        QModelIndex ensembleIndex = model->index(r, 0);
        if (model->getId(ensembleIndex) == ensembleId)
        {   // found ensemble
            for (int s = 0; s < model->rowCount(ensembleIndex); ++s)
            {   // go through services
                QModelIndex serviceIndex = model->index(s, 0, ensembleIndex);
                if (model->getId(serviceIndex) == serviceId)
                {  // found
                    ui->serviceTreeView->selectionModel()->setCurrentIndex(serviceIndex, QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);
                    return;
                }
            }
        }
    }
}

void MainWindow::serviceListViewUpdateSelection()
{
    const SLModel * model = reinterpret_cast<const SLModel*>(ui->serviceListView->model());
    uint64_t id = ServiceListItem::getId(SId.value, SCIdS);
    QModelIndex index;
    for (int r = 0; r < model->rowCount(); ++r)
    {
        index = model->index(r, 0);
        if (model->getId(index) == id)
        {   // found
            ui->serviceListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);
            return;
        }
    }
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

void MainWindow::showCatSLS()
{
    catSlsDialog->show();
    catSlsDialog->raise();
    catSlsDialog->activateWindow();
}

void MainWindow::setExpertMode(bool ena)
{
    if (ena)
    {
        switchModeAct->setText("Basic mode");
    }
    else
    {
        switchModeAct->setText("Expert mode");
    }
    ui->channelCombo->setVisible(ena);
    ui->channelLabel->setVisible(!ena);
    ui->serviceTreeView->setVisible(ena);

    QTimer::singleShot(10, this, [this](){ resize(minimumSizeHint()); } );
}

void MainWindow::bandScan()
{
    BandScanDialog * dialog = new BandScanDialog(this, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    connect(dialog, &BandScanDialog::finished, dialog, &QObject::deleteLater);
    connect(dialog, &BandScanDialog::tuneChannel, this, &MainWindow::tuneChannel);
    connect(radioControl, &RadioControl::ensembleInformation, dialog, &BandScanDialog::ensembleFound, Qt::QueuedConnection);
    connect(radioControl, &RadioControl::tuneDone, dialog, &BandScanDialog::tuneFinished, Qt::QueuedConnection);
    connect(serviceList, &ServiceList::serviceAdded, dialog, &BandScanDialog::serviceFound);
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
    int idx = 0;
    dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
    while (it != DabTables::channelList.constEnd()) {
        if (it.key() == freq)
        {
            ui->channelCombo->setCurrentIndex(idx);
            return;
        }
        ++it;
        ++idx;
    }
}

void MainWindow::stop()
{
    if (isPlaying)
    { // stop
        // tune to 0
        ui->channelCombo->setCurrentIndex(-1);
        ui->channelLabel->setText("");
    }
}

void MainWindow::clearServiceList()
{
    stop();
    serviceList->clear();
}

