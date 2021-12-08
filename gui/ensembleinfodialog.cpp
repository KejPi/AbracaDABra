#include <QFileDialog>
#include <QDateTime>
#include <QDebug>

#include "ensembleinfodialog.h"
#include "ui_ensembleinfodialog.h"

#include "inputdevice.h"   // needed for AGC gain

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    dumpPath = QDir::homePath();

    ui->snr->setText("");
    ui->freqOffset->setText("");
    enableDumpToFile(false);
}

EnsembleInfoDialog::~EnsembleInfoDialog()
{
    delete ui;
}

void EnsembleInfoDialog::refreshEnsembleConfiguration(const QString & txt)
{
    ui->ensStructureTextEdit->setHtml(txt);

    if (txt.isEmpty())
    {  // empty ensemble configuration means tuning to new frequency
        ui->snr->setText("N/A");
        ui->freqOffset->setText("N/A");
        ui->agcGain->setText("N/A");

        fibCounter = 0;
        fibErrorCounter = 0;
        ui->fibCount->setText("0");
        ui->fibErrCount->setText("0");
        ui->fibErrRate->setText("N/A");


    }
}

void EnsembleInfoDialog::updateSnr(float snr)
{
    ui->snr->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
}

void EnsembleInfoDialog::updateFreqOffset(float offset)
{
    ui->freqOffset->setText(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void EnsembleInfoDialog::enableDumpToFile(bool ena)
{
    ui->dumpButton->setVisible(ena);   
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");
    ui->dumpLengthLabel->setVisible(false);
    ui->dumpSizeLabel->setVisible(false);
}

void EnsembleInfoDialog::on_dumpButton_clicked()
{
    ui->dumpButton->setEnabled(false);
    if (!isDumping)
    {
        QString f = QString("%1/%2_%3.raw")
                .arg(dumpPath)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"))
                .arg(DabTables::channelList.value(frequency));

        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Dump IQ stream"),
                                                        QDir::toNativeSeparators(f),
                                                        tr("Binary files (*.raw)"));
        if (!fileName.isEmpty())
        {
            dumpPath = QFileInfo(fileName).path(); // store path for next time
            emit dumpToFileStart(fileName);
        }
        else
        {
            ui->dumpButton->setEnabled(true);
        }
    }
    else
    {
        emit dumpToFileStop();
    }
}

void EnsembleInfoDialog::dumpToFileStateToggle(bool dumping, int bytesPerSample)
{
    isDumping = dumping;
    if (dumping)
    {
        ui->dumpButton->setText("Stop dumping");
        bytesDumped = 0;

        // default is bytes/2048/2 => 2 bytes per sample, 2048 samples per milisecond => 2^-12
        bytesToTimeShiftFactor = 12 + (4 == bytesPerSample);
    }
    else
    {
        ui->dumpButton->setText("Dump raw data");
    }
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");

    ui->dumpLengthLabel->setVisible(dumping);
    ui->dumpSizeLabel->setVisible(dumping);
    ui->dumpButton->setEnabled(true);
}

void EnsembleInfoDialog::updateDumpStatus(ssize_t bytes)
{
    bytesDumped += bytes;
    ui->dumpSize->setText(QString::number(double(bytesDumped/(1024*1024.0)),'f', 1) + " MB");
    int timeMs = bytesDumped >> bytesToTimeShiftFactor;
    ui->dumpLength->setText(QString::number(double(timeMs * 0.001),'f', 1) + " sec");
}

void EnsembleInfoDialog::updateAgcGain(int gain10)
{
    if (INPUTDEVICE_AGC_GAIN_NA == gain10)
    {   // gain is not available (input device in HW mode)
        ui->agcGain->setText("N/A");
        return;
    }
    ui->agcGain->setText(QString::number(double(gain10 * 0.1),'f', 1) + " dB");
}

void EnsembleInfoDialog::updateFIBstatus(int fibCntr, int fibErrCount)
{
    fibCounter += (fibCntr - fibErrCount);
    fibCounter &= 0x7FFFFFFF;       // wrapping
    fibErrorCounter += fibErrCount;
    fibErrorCounter &= 0x7FFFFFFF;  // wrapping
    ui->fibCount->setText(QString::number(fibCounter));
    ui->fibErrCount->setText(QString::number(fibErrorCounter));
    ui->fibErrRate->setText(QString::number(double(fibErrorCounter)/fibCounter,'e', 4));
}

void EnsembleInfoDialog::showEvent(QShowEvent *event)
{
    emit requestEnsembleConfiguration();
    event->accept();
}

void EnsembleInfoDialog::closeEvent(QCloseEvent *event)
{
    if (isDumping)
    {
        emit dumpToFileStop();
    }
    event->accept();
}

const QString &EnsembleInfoDialog::getDumpPath() const
{
    return dumpPath;
}

void EnsembleInfoDialog::setDumpPath(const QString &newDumpPath)
{
    dumpPath = newDumpPath;
}


