#ifndef BANDSCANDIALOG_H
#define BANDSCANDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#include "radiocontrol.h"
#include "servicelist.h"

namespace Ui {
class BandScanDialog;
}

enum class BandScanState
{
    Idle = 0,
    Init,
    WaitForTune,
    WaitForEnsemble,
    WaitForServices
};

class BandScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BandScanDialog(QWidget *parent = nullptr);
    ~BandScanDialog();

    void tuneFinished(uint32_t freq);
    void ensembleFound(const RadioControlEnsemble &ens);
    void serviceFound(const ServiceListItem *s);

signals:
    void scanStarts();
    void tune(uint32_t freq, uint32_t, uint8_t);

private:
    Ui::BandScanDialog *ui;
    QPushButton * buttonStart;
    QPushButton * buttonStop;
    QTimer * timer = nullptr;

    bool scanning = false;
    BandScanState state = BandScanState::Idle;

    int numEnsemblesFound = 0;
    int numServicesFound = 0;
    dabChannelList_t::ConstIterator channelIt;

    void startScan();

    void scanStep();
};

#endif // BANDSCANDIALOG_H
