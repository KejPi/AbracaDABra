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
    //ui->dabTimeLabel->setText("");
    ui->dumpButton->setVisible(false);
}

EnsembleInfoDialog::~EnsembleInfoDialog()
{
    delete ui;
}

void EnsembleInfoDialog::setEnsStructText(const QString & txt)
{
    ui->ensStructureTextEdit->setHtml(txt);
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

void EnsembleInfoDialog::dumpToFileStateToggle(bool dumping)
{
    isDumping = dumping;
    if (dumping)
    {
        ui->dumpButton->setText("Stop dumping");
    }
    else
    {
        ui->dumpButton->setText("Dump raw data");
    }
    ui->dumpButton->setEnabled(true);
}

void EnsembleInfoDialog::closeEvent(QCloseEvent *event)
{
    if (isDumping)
    {
        emit dumpToFileStop();
    }
    event->accept();
}


