#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QStandardItemModel>
#include <QTimer>
#include <QCloseEvent>
#include <QLabel>
#include <QProgressBar>

#include "setupdialog.h"
#include "ensembleinfodialog.h"
#include "slsview.h"
#include "catslsdialog.h"
#include "inputdevice.h"
#include "rawfileinput.h"
#include "rtlsdrinput.h"
#include "rtltcpinput.h"
#include "rarttcpinput.h"
#include "radiocontrol.h"
#include "dldecoder.h"
#include "motdecoder.h"
#include "userapplication.h"
#include "audiodecoder.h"
#include "audiooutput.h"
#include "servicelist.h"
#include "slmodel.h"
#include "sltreemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void inputDeviceReady();
    void updateEnsembleInfo(const RadioControlEnsemble &ens);
    void updateSyncStatus(uint8_t sync);
    void updateSnrLevel(float snr);
    void updateServiceList(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void updateDL(const QString &dl);
    void updateAudioInfo(const AudioParameters &params);
    void updateDabTime(const QDateTime & d);
    void changeInputDevice(const InputDeviceId &d);
    void tuneFinished(uint32_t freq);
    void enableFileLooping(bool ena) { fileLooping = ena; }

signals:
    void serviceRequest(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void stopUserApps();
    void exit();

private slots:
    void on_channelCombo_currentIndexChanged(int index);
    void clearEnsembleInformationLabels();
    void clearServiceInformationLabels();
    void initInputDevice(const InputDeviceId &d);
    void onRawFileStop();
    void onInputDeviceError(const InputDeviceErrorCode errCode);
    void serviceListClicked(const QModelIndex &index);
    void serviceListTreeClicked(const QModelIndex &index);
    void serviceChanged(const RadioControlServiceComponent &s);

protected:        
     void closeEvent(QCloseEvent *event);
     void showEvent(QShowEvent *event);
     void resizeEvent(QResizeEvent *event);
private:
    Ui::MainWindow *ui;
    SetupDialog * setupDialog;
    EnsembleInfoDialog * ensembleInfoDialog;
    CatSLSDialog * catSlsDialog;
    QProgressBar * snrProgress;
    QLabel  * timeLabel;
    QLabel * syncLabel;
    QLabel * snrLabel;

    QMenu * menu;
    QAction * setupAct;
    QAction * clearServiceListAct;
    QAction * bandScanAct;
    QAction * switchModeAct;
    QAction * ensembleInfoAct;
    QAction * catSlsAct;

    QThread * radioControlThr;
    RadioControl * radioControl;

    DLDecoder * dlDecoder;
    SlideShowApp * slideShowApp;

    InputDeviceId inputDeviceId = InputDeviceId::UNDEFINED;
    InputDevice * inputDevice = nullptr;

    QThread * audioDecoderThr;
    AudioDecoder * audioDecoder;

#if (!defined AUDIOOUTPUT_USE_RTAUDIO) &&  (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    QThread * audioOutThr;
#endif
    AudioOutput * audioOutput;

    bool isPlaying = false;
    bool fileLooping = false;
    bool deviceChangeRequested = false;
    bool expertMode = false;
    bool exitRequested = false;
    uint32_t frequency = 0;
    DabSId SId;
    uint32_t SCIdS = 0;
//    uint64_t serviceId = 0;
//    uint64_t ensembleId = 0;

    ServiceList * serviceList;
    SLModel * slModel;
    SLTreeModel * slTreeModel;

    void onServiceSelection();
    void onChannelSelection();
    void loadSettings();
    void saveSettings();

    void setFavoriteLabel(bool ena);
    void favoriteToggled(bool checked);
    void switchServiceSource();
    void switchMode();
    void showEnsembleInfo();
    void showCatSLS();
    void setExpertMode(bool ena);
    void stop();
    void tuneChannel(uint32_t freq);
    void bandScan();
    void bandScanFinished(int result);
    void clearServiceList();

    void serviceTreeViewUpdateSelection();
    void serviceListViewUpdateSelection();
};

#endif // MAINWINDOW_H
