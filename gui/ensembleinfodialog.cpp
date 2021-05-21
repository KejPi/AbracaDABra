#include <QDebug>

#include "ensembleinfodialog.h"
#include "ui_ensembleinfodialog.h"

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

    ui->snrLabel->setText("");
    ui->freqOffsetLabel->setText("");
    //ui->dabTimeLabel->setText("");
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
