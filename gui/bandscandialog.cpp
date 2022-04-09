#include <QDebug>

#include "bandscandialog.h"
#include "ui_bandscandialog.h"
#include "dabtables.h"

BandScanDialog::BandScanDialog(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    ui(new Ui::BandScanDialog)
{
    ui->setupUi(this);

    setSizeGripEnabled(false);

    buttonStart = ui->buttonBox->button(QDialogButtonBox::Ok);
    buttonStart->setText("Start");
    connect(buttonStart, &QPushButton::clicked, this, &BandScanDialog::startScan);

    buttonStop = ui->buttonBox->button(QDialogButtonBox::Cancel);
    buttonStop->setDefault(true);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &BandScanDialog::stopPressed);

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(numEnsemblesFound));
    ui->numServicesFoundLabel->setText(QString("%1").arg(numServicesFound));
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(DabTables::channelList.size());
    ui->progressBar->setValue(0);
    ui->progressLabel->setText(QString("0 / %1").arg(DabTables::channelList.size()));
    ui->progressChannel->setText("None");

    ui->progressLabel->setVisible(false);
    ui->progressChannel->setVisible(false);
    ui->scanningLabel->setText("<span style=\"color:red\"><b>Warning:</b> Band scan deletes current service list and all favorites!</span>");


    //layout()->setSizeConstraint( QLayout::SetFixedSize );
    setFixedSize( sizeHint() );
}

BandScanDialog::~BandScanDialog()
{
    qDebug() << Q_FUNC_INFO;

    if (nullptr != timer)
    {
        timer->stop();
        delete timer;
    }

    delete ui;        
}

void BandScanDialog::stopPressed()
{
    qDebug() << Q_FUNC_INFO;
    if (isScanning)
    {
        // the state machine has 4 possible states
        // 1. wait for tune (event)
        // 2. wait for sync (timer or event)
        // 4. wait for enseble (timer or event)
        // 5. wait for services (timer)
        if (timer->isActive())
        {   // state 2, 3, 4
            timer->stop();
            done(BandScanDialogResult::Interrupted);
        }
        // timer not running -> state 1
        state = BandScanState::Interrupted;  // ==> it will be finished when tune is complete
    }
    else
    {
        done(BandScanDialogResult::Cancelled);
    }
}

void BandScanDialog::startScan()
{
    qDebug() << Q_FUNC_INFO;

    isScanning = true;

    ui->scanningLabel->setText("Scanning channel:");
    ui->progressLabel->setVisible(true);
    ui->progressChannel->setVisible(true);

    buttonStart->setText("Scanning");
    buttonStart->setEnabled(false);
    buttonStop->setText("Stop");
    buttonStop->setDefault(false);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BandScanDialog::scanStep);

    state = BandScanState::Init;

    // using timer for mainwindow to cleanup and tune to 0 potentially (no timeout in case)
    timer->start(2000);
    emit scanStarts();        
}

void BandScanDialog::scanStep()
{
    if (BandScanState::Init == state)
    {  // first step
       channelIt = DabTables::channelList.constBegin();

       // debug
       //channelIt = DabTables::channelList.find(225648);
    }
    else
    {  // next step
       ++channelIt;
    }

    if (DabTables::channelList.constEnd() == channelIt)
    {
        // scan finished
        done(BandScanDialogResult::Done);
        return;
    }

    ui->progressBar->setValue(ui->progressBar->value()+1);
    ui->progressLabel->setText(QString("%1 / %2")
                               .arg(ui->progressBar->value())
                               .arg(DabTables::channelList.size()));
    ui->progressChannel->setText(channelIt.value());
    state = BandScanState::WaitForTune;
    emit tuneChannel(channelIt.key());
}

void BandScanDialog::tuneFinished(uint32_t freq)
{
     qDebug() << Q_FUNC_INFO << freq;

    if (BandScanState::Init == state)
    {
        if (timer->isActive())
        {
            timer->stop();
        }
        scanStep();
    }
    else if (BandScanState::Interrupted == state)
    {   // exit
        done(BandScanDialogResult::Interrupted);
    }
    else
    {   // tuned to some frequency -> wait for sync
        state = BandScanState::WaitForSync;
        timer->start(2000);
    }
}

void BandScanDialog::onSyncStatus(uint8_t sync)
{
    if (DabSyncLevel::NullSync <= DabSyncLevel(sync))
    {
        timer->stop();
        state = BandScanState::WaitForEnsemble;
        timer->start(3000);
    }
}

void BandScanDialog::onEnsembleFound(const RadioControlEnsemble &ens)
{
    timer->stop();

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(++numEnsemblesFound));

    state = BandScanState::WaitForServices;

    // this can be interrupted by enseble complete signal (ensembleConfiguration)
    timer->start(8000);
}

void BandScanDialog::onServiceFound(uint64_t)
{
    ui->numServicesFoundLabel->setText(QString("%1").arg(++numServicesFound));
}

void BandScanDialog::onEnsembleComplete()
{   // this means that ensemble information is complete => stop timer and do next set
    qDebug() << Q_FUNC_INFO;
    if ((nullptr != timer) && (timer->isActive()))
    {
        timer->stop();
        scanStep();
    }
}

