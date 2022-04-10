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

namespace BandScanDialogResult {
enum
{
    Cancelled = 0,
    Interrupted = 1,
    Done = 2
};
}

enum class BandScanState
{
    Idle = 0,
    Init,
    WaitForTune,
    WaitForSync,
    WaitForEnsemble,
    WaitForServices,
    Interrupted
};

class BandScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BandScanDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~BandScanDialog();

    void tuneFinished(uint32_t freq);
    void onSyncStatus(uint8_t sync);
    void onEnsembleFound(const RadioControlEnsemble &ens);
    void onServiceFound(const ServiceListId &);
    void onEnsembleComplete(const RadioControlEnsemble &);

signals:
    void scanStarts();
    void tuneChannel(uint32_t freq);

private:
    Ui::BandScanDialog *ui;
    QPushButton * buttonStart;
    QPushButton * buttonStop;
    QTimer * timer = nullptr;

    bool isScanning = false;
    BandScanState state = BandScanState::Idle;

    int numEnsemblesFound = 0;
    int numServicesFound = 0;
    dabChannelList_t::ConstIterator channelIt;

    void startScan();
    void scanStep();
    void stopPressed();
};

#endif // BANDSCANDIALOG_H
