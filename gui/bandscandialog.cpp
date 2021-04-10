#include <QGridLayout>

#include "bandscandialog.h"
#include "ui_bandscandialog.h"
#include "dabtables.h"

BandScanDialog::BandScanDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BandScanDialog)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Start");

    // fill channel list
    dabChannelList_t::const_iterator i = DabTables::channelList.constBegin();
    while (i != DabTables::channelList.constEnd()) {
        ui->startComboBox->addItem(i.value(), i.key());
        ui->stopComboBox->addItem(i.value(), i.key());
        ++i;
    }
    ui->startComboBox->setCurrentIndex(0);
    ui->stopComboBox->setCurrentIndex(ui->stopComboBox->count()-1);
}

BandScanDialog::~BandScanDialog()
{
    delete ui;
}
