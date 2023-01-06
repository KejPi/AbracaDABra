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
#include <QSlider>

#include "clickablelabel.h"
#include "setupdialog.h"
#include "ensembleinfodialog.h"
#include "catslsdialog.h"
#include "inputdevice.h"
#include "radiocontrol.h"
#include "dldecoder.h"
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
    MainWindow(const QString & iniFilename = QString(), QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject * o, QEvent * e);


signals:
    void serviceRequest(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void stopUserApps();
    void getAudioInfo();
    void expertModeChanged(bool ena);
    void exit();

protected:        
    void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent *event);
    void changeEvent( QEvent* e );
private:
    // constants
    enum InstanceIdx { Service = 0, Announcement = 1, NumInstances };
    static const QString appName;
    static const QStringList syncLevelLabels;
    static const QStringList syncLevelTooltip;
    static const QStringList snrProgressStylesheet;

    // UI and dialogs
    Ui::MainWindow *ui;
    SetupDialog * m_setupDialog;
    EnsembleInfoDialog * m_ensembleInfoDialog;
    CatSLSDialog * m_catSlsDialog;
    QProgressBar * m_snrProgressbar;
    ClickableLabel * m_settingsLabel;
    ClickableLabel * m_muteLabel;

    // status bar widgets
    QStackedWidget * m_timeBasicQualWidget;
    QLabel  * m_timeLabel;
    QLabel  * m_basicSignalQualityLabel;
    QWidget * m_signalQualityWidget;
    QLabel * m_syncLabel;
    QLabel * m_snrLabel;

    // application menu
    QMenu * m_menu;
    QAction * m_setupAction;
    QAction * m_clearServiceListAction;
    QAction * m_bandScanAction;
    QAction * m_switchModeAction;
    QAction * m_ensembleInfoAction;
    QAction * m_aboutAction;

    // radio control
    QThread * m_radioControlThread;
    RadioControl * m_radioControl;

    // input device
    InputDeviceId m_inputDeviceId = InputDeviceId::UNDEFINED;
    InputDevice * m_inputDevice = nullptr;

    // audio decoder
    QThread * m_audioDecoderThread;
    AudioDecoder * m_audioDecoder;

    // audio output
#if (!HAVE_PORTAUDIO)
    QThread * m_audioOutputThread;
#endif
    QSlider * m_volumeSlider;

    AudioOutput * m_audioOutput;

    // state variables
    QString m_iniFilename;
    bool m_isPlaying = false;
    bool m_deviceChangeRequested = false;
    bool m_expertMode = false;
    bool m_exitRequested = false;
    uint32_t m_frequency = 0;
    DabSId m_SId;
    uint8_t m_SCIdS = 0;
    bool m_hasListViewFocus;
    bool m_hasTreeViewFocus;

    // service list
    ServiceList * m_serviceList;
    SLModel * m_slModel;
    SLTreeModel * m_slTreeModel;

    // user applications
    DLDecoder * m_dlDecoder;
    QMap<DLPlusContentType, DLPlusObjectUI*> m_dlObjCache;
    SlideShowApp * m_slideShowApp;
    SPIApp * m_spiApp;

    // methods
    void loadSettings();
    void saveSettings();

    void switchMode();
    void showEnsembleInfo();
    void showAboutDialog();
    void showSetupDialog();
    void showCatSLS();
    void setExpertMode(bool ena);
    void stop();
    void bandScan();
    void clearServiceList();
    void clearEnsembleInformationLabels();
    void clearServiceInformationLabels();
    void initInputDevice(const InputDeviceId &d);
    bool isDarkMode();
    void setIcons();    
    void serviceSelected();
    void channelSelected();
    void serviceTreeViewUpdateSelection();
    void serviceListViewUpdateSelection();
    void changeInputDevice(const InputDeviceId &d);    

    void onInputDeviceReady();
    void onEnsembleInfo(const RadioControlEnsemble &ens);
    void onServiceListComplete(const RadioControlEnsemble &ens);
    void onEnsembleReconfiguration(const RadioControlEnsemble &ens) const;
    void onEnsembleRemoved(const RadioControlEnsemble &ens);
    void onChannelChange(int index);
    void onBandScanFinished(int result);
    void onFavoriteToggled(bool checked);
    void onSwitchSourceClicked();
    void onSyncStatus(uint8_t sync);
    void onSnrLevel(float snr);
    void onServiceListEntry(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void onDLComplete_Service(const QString &dl);
    void onDLComplete_Announcement(const QString & dl);
    void onDLComplete(QLabel * dlLabel, const QString & dl);
    void onDLPlusToggled(bool toggle);
    void onDLPlusObjReceived(const DLPlusObject & object);
    void onDLPlusItemToggle();
    void onDLPlusItemRunning(bool isRunning);
    void onDLReset_Service();
    void onDLReset_Announcement();
    void onAudioParametersInfo(const AudioParameters &params);
    void onDabTime(const QDateTime & d);
    void onTuneChannel(uint32_t freq);
    void onTuneDone(uint32_t freq);
    void onNewSettings();
    void onInputDeviceError(const InputDeviceErrorCode errCode);
    void onServiceListClicked(const QModelIndex &index);
    void onServiceListTreeClicked(const QModelIndex &index);
    void onAudioServiceSelection(const RadioControlServiceComponent &s);
    void onAudioServiceReconfiguration(const RadioControlServiceComponent &s);
    void onAnnouncement(DabAnnouncement id, bool serviceSwitch);
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
    DLPlusObject m_dlPlusObject;
    QHBoxLayout* m_layout;
    QLabel * m_tagLabel;
    QLabel * m_tagText;

    QString getLabel(DLPlusContentType type) const;
};


#endif // MAINWINDOW_H
