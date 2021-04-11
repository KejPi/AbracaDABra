#include <QDebug>

#include "bandscandialog.h"
#include "ui_bandscandialog.h"
#include "dabtables.h"

BandScanDialog::BandScanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BandScanDialog)
{
    ui->setupUi(this);

    buttonStart = ui->buttonBox->button(QDialogButtonBox::Ok);
    buttonStart->setText("Start");
    connect(buttonStart, &QPushButton::clicked, this, &BandScanDialog::startScan);

    buttonStop = ui->buttonBox->button(QDialogButtonBox::Cancel);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    //ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Start");

//    // fill channel list
//    dabChannelList_t::const_iterator i = DabTables::channelList.constBegin();
//    while (i != DabTables::channelList.constEnd()) {
//        ui->startComboBox->addItem(i.value(), i.key());
//        ui->stopComboBox->addItem(i.value(), i.key());
//        ++i;
//    }
//    ui->startComboBox->setCurrentIndex(0);
//    ui->stopComboBox->setCurrentIndex(ui->stopComboBox->count()-1);

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(numEnsemblesFound));
    ui->numServicesFoundLabel->setText(QString("%1").arg(numServicesFound));
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(DabTables::channelList.size());
    ui->progressBar->setValue(0);
    ui->progressLabel->setText(QString("0 / %1").arg(DabTables::channelList.size()));
    ui->progressChannel->setText("None");

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


void BandScanDialog::startScan()
{
    qDebug() << Q_FUNC_INFO;

    scanning = true;
    buttonStart->setText("Scanning");
    buttonStart->setEnabled(false);
    buttonStop->setText("Stop");

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BandScanDialog::scanStep);

    state = BandScanState::Init;
    // this stops audio and tunes to 0
    emit scanStarts();

//    QThread::msleep(5000);

//    emit tune(174928, 0, 0);

//    QThread::msleep(6000);

//    emit tune(188928, 0, 0);

//    QThread::msleep(6000);

//    emit tune(227360, 0, 0);

//    int progress = 0;
//    dabChannelList_t::const_iterator it = DabTables::channelList.constBegin();
//    while (it != DabTables::channelList.constEnd())
//    {
//        int numEns = numEnsemblesFound;
//        ui->progressLabel->setText(QString("%1 / %1").arg(++progress).arg(DabTables::channelList.size()));
//        ui->progressBar->setValue(progress);
//        ui->progressChannel->setText(it.value());

//        emit tune(it.key(), 0, 0);
//        QThread::msleep(3000);
//        if (numEnsemblesFound > numEns)
//        {  // signal -- wait for services
//           QThread::msleep(3000);
//        }
//        ++it;
//    }
}

void BandScanDialog::scanStep()
{
    if (BandScanState::Init == state)
    {  // first step
       channelIt = DabTables::channelList.constBegin();
    }
    else
    {  // next step
       ++channelIt;
    }

    if (DabTables::channelList.constEnd() == channelIt)
    {
        // scan finished
        accept();
    }

    ui->progressBar->setValue(ui->progressBar->value()+1);
    ui->progressLabel->setText(QString("%1 / %2")
                               .arg(ui->progressBar->value())
                               .arg(DabTables::channelList.size()));
    ui->progressChannel->setText(channelIt.value());
    state = BandScanState::WaitForTune;
    emit tune(channelIt.key(), 0, 0);
}

void BandScanDialog::tuneFinished(uint32_t freq)
{
    qDebug() << Q_FUNC_INFO << freq;

    if (BandScanState::Init == state)
    {
        scanStep();
    }
    else
    {   // tuned to some frequency -> wait for ensemble
        state = BandScanState::WaitForEnsemble;
        timer->start(2000);
    }
}

void BandScanDialog::ensembleFound(const RadioControlEnsemble &ens)
{
    qDebug() << Q_FUNC_INFO << ens.label;
    timer->stop();

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(++numEnsemblesFound));

    state = BandScanState::WaitForServices;
    timer->start(3000);
}

void BandScanDialog::serviceFound(const ServiceListItem *s)
{
    qDebug() << Q_FUNC_INFO << s->label();
    ui->numServicesFoundLabel->setText(QString("%1").arg(++numServicesFound));
}

