#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QStandardItemModel>
#include <QTimer>
#include <QCloseEvent>
#include <QLabel>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QStackedWidget>

#include "clickablelabel.h"
#include "setupdialog.h"
#include "ensembleinfodialog.h"
#include "aboutdialog.h"
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
#include "slideshowapp.h"
#include "spiapp.h"
#include "audiodecoder.h"
#include "audiooutput.h"
#include "servicelist.h"
#include "slmodel.h"
#include "sltreemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class DLPlusObjectUI;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject * o, QEvent * e);

public slots:
    void inputDeviceReady();
    void updateEnsembleInfo(const RadioControlEnsemble &ens);
    void onEnsembleComplete(const RadioControlEnsemble &ens);
    void onEnsembleReconfiguration(const RadioControlEnsemble &ens) const;
    void updateSyncStatus(uint8_t sync);
    void updateSnrLevel(float snr);
    void updateServiceList(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void updateDL(const QString &dl);
    void onDLPlusObjReceived(const DLPlusObject & object);
    void onDLPlusItemToggle();
    void onDLPlusItemRunning(bool isRunning);
    void onDLReset();
    void updateAudioInfo(const AudioParameters &params);
    void updateDabTime(const QDateTime & d);
    void changeInputDevice(const InputDeviceId &d);
    void tuneFinished(uint32_t freq);

signals:
    void serviceRequest(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void stopUserApps();
    void getAudioInfo();
    void expertModeChanged(bool ena);
    void exit();

private slots:
    void onChannelChange(int index);
    void clearEnsembleInformationLabels();
    void clearServiceInformationLabels();
    void onNewSettings();
    void initInputDevice(const InputDeviceId &d);
    void onInputDeviceError(const InputDeviceErrorCode errCode);
    void serviceListClicked(const QModelIndex &index);
    void serviceListTreeClicked(const QModelIndex &index);
    void serviceChanged(const RadioControlServiceComponent &s);
    void audioServiceReconfiguration(const RadioControlServiceComponent &s);    

protected:        
     void closeEvent(QCloseEvent *event);
     void resizeEvent(QResizeEvent *event);
     void changeEvent( QEvent* e );
private:
    Ui::MainWindow *ui;
    SetupDialog * setupDialog;
    EnsembleInfoDialog * ensembleInfoDialog;
    CatSLSDialog * catSlsDialog;   
    QProgressBar * snrProgress;
    ClickableLabel * settingsLabel;
    ClickableLabel * muteLabel;

    QStackedWidget * timeBasicQualWidget;
    QLabel  * timeLabel;
    QLabel  * basicSignalQualityLabel;

    QWidget * signalQualityWidget;
    QLabel * syncLabel;
    QLabel * snrLabel;

    QMenu * menu;
    QAction * setupAct;
    QAction * clearServiceListAct;
    QAction * bandScanAct;
    QAction * switchModeAct;
    QAction * ensembleInfoAct;
    QAction * aboutAct;

    QThread * radioControlThr;
    RadioControl * radioControl;

    DLDecoder * dlDecoder;
    SlideShowApp * slideShowApp;
    SPIApp * spiApp;

    InputDeviceId inputDeviceId = InputDeviceId::UNDEFINED;
    InputDevice * inputDevice = nullptr;

    QThread * audioDecoderThr;
    AudioDecoder * audioDecoder;

#if (!defined AUDIOOUTPUT_USE_RTAUDIO) &&  (!defined AUDIOOUTPUT_USE_PORTAUDIO)
    QThread * audioOutThr;
#endif
    AudioOutput * audioOutput;

    bool isPlaying = false;
    bool deviceChangeRequested = false;
    bool expertMode = false;
    bool exitRequested = false;
    uint32_t frequency = 0;
    DabSId SId;
    uint8_t SCIdS = 0;
    bool hasListViewFocus;
    bool hasTreeViewFocus;

    ServiceList * serviceList;
    SLModel * slModel;
    SLTreeModel * slTreeModel;

    //SetupDialog::Settings m_settings;

    QMap<DLPlusContentType, DLPlusObjectUI*> dlObjCache;

    void onServiceSelection();
    void onChannelSelection();
    void loadSettings();
    void saveSettings();

    void setFavoriteLabel(bool ena);
    void favoriteToggled(bool checked);
    void switchServiceSource();
    void switchMode();
    void showEnsembleInfo();
    void showAboutDialog();
    void showSetupDialog();
    void showCatSLS();
    void setExpertMode(bool ena);
    void stop();
    void tuneChannel(uint32_t freq);
    void bandScan();
    void bandScanFinished(int result);
    void clearServiceList();

    bool isDarkMode();
    void setIcons();

    void serviceTreeViewUpdateSelection();
    void serviceListViewUpdateSelection();

    void onDLPlusToggle(bool toggle);    

};

class DLPlusObjectUI
{
public:
    DLPlusObjectUI(const DLPlusObject & obj);
    ~DLPlusObjectUI();
    QHBoxLayout *getLayout() const;
    void update(const DLPlusObject & obj);
    void setVisible(bool visible);
    const DLPlusObject &getDlPlusObject() const;

private:
    DLPlusObject dlPlusObject;
    QHBoxLayout* layout;
    QLabel * tagLabel;
    QLabel * tagText;

    QString getLabel(DLPlusContentType type) const;
};


#endif // MAINWINDOW_H
