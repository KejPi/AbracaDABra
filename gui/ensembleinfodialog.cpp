#include <QFileDialog>
#include <QDateTime>
#include <QDebug>

#include "ensembleinfodialog.h"
#include "ui_ensembleinfodialog.h"

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

    // remove question mark from titlebar
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->snrLabel->setText("");
    ui->freqOffsetLabel->setText("");
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
        ui->snrLabel->setText("");
        ui->freqOffsetLabel->setText("");
    }
}

void EnsembleInfoDialog::updateSnr(float snr)
{
    ui->snrLabel->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
}

void EnsembleInfoDialog::updateFreqOffset(float offset)
{
    ui->freqOffsetLabel->setText(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void EnsembleInfoDialog::enableDumpToFile(bool ena)
{
    ui->dumpButton->setVisible(ena);
    ui->dumpSizeLabel->setVisible(false);
    ui->dumpTimeLabel->setVisible(false);
}

void EnsembleInfoDialog::on_dumpButton_clicked()
{
    ui->dumpButton->setEnabled(false);
    if (!isDumping)
    {
        QString f = QString("%1/%2.raw").arg(QDir::homePath()).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        tr("Dump IQ stream"),
                                                        QDir::toNativeSeparators(f),
                                                        tr("Binary files (*.raw)"));
        if (!fileName.isEmpty())
        {
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
        ui->dumpSizeLabel->setText("");
        ui->dumpTimeLabel->setText("");
    }
    else
    {
        ui->dumpButton->setText("Dump raw data");
    }
    ui->dumpSizeLabel->setVisible(dumping);
    ui->dumpTimeLabel->setVisible(dumping);
    ui->dumpButton->setEnabled(true);
}

void EnsembleInfoDialog::updateDumpStatus(ssize_t bytes)
{
    bytesDumped += bytes;
    ui->dumpSizeLabel->setText(QString::number(double(bytesDumped/(1024*1024.0)),'f', 1) + " MB");
    int timeMs = bytesDumped >> bytesToTimeShiftFactor;
    ui->dumpTimeLabel->setText(QString::number(double(timeMs * 0.001),'f', 1) + " sec");
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


