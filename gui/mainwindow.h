/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QItemSelection>
#include <QLabel>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMessageBox>
#include <QProgressBar>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "audiodecoder.h"
#include "audiooutput.h"
#include "audiorecmanager.h"
#include "audiorecscheduledialog.h"
#include "audiorecschedulemodel.h"
#include "catslsdialog.h"
#include "clickablelabel.h"
#include "config.h"
#include "dabchannellistmodel.h"
#include "dldecoder.h"
#include "ensembleinfodialog.h"
#include "epgdialog.h"
#include "inputdevice.h"
#include "inputdevicerecorder.h"
#include "logdialog.h"
#include "metadatamanager.h"
#include "radiocontrol.h"
#include "scannerdialog.h"
#include "servicelist.h"
#include "setupdialog.h"
#include "slideshowapp.h"
#include "slmodel.h"
#include "sltreemodel.h"
#include "spiapp.h"
#include "tiidialog.h"
#if HAVE_QCUSTOMPLOT
#include "signaldialog.h"
#endif

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class DLPlusObjectUI;
class FMListInterface;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static int const EXIT_CODE_RESTART;

    MainWindow(const QString &iniFilename = QString(), const QString &iniSlFilename = QString(), QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject *o, QEvent *e);

signals:
    void serviceRequest(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void stopUserApps();
    void resetUserApps();
    void getAudioInfo();
    void getEnsembleInfo();
    void toggleAnnouncement();
    void audioMute(bool doMute);
    void audioVolume(int volume);
    void audioOutput(const QByteArray &deviceId);
    void audioStop();
    void announcementMask(uint16_t mask);
    void exit();

protected:
    void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent *event);
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    void changeEvent(QEvent *e);
#endif
private:
    // constants
    enum Instance
    {
        Service = 0,
        Announcement = 1,
        NumInstances
    };
    static const QString appName;
    static const char *syncLevelLabels[];
    static const char *syncLevelTooltip[];
    static const QStringList snrProgressStylesheet;
    static const QString slsDumpPatern;
    static const QString spiDumpPatern;

    // UI and dialogs
    Ui::MainWindow *ui;
    SetupDialog *m_setupDialog;
    EPGDialog *m_epgDialog;
    EnsembleInfoDialog *m_ensembleInfoDialog;
    CatSLSDialog *m_catSlsDialog;
    LogDialog *m_logDialog;
    AudioRecScheduleDialog *m_audioRecScheduleDialog;
    TIIDialog *m_tiiDialog;
    ScannerDialog *m_scannerDialog;
#if HAVE_QCUSTOMPLOT
    SignalDialog *m_signalDialog;
    ClickableLabel *m_snrLabel;
#else
    QLabel *m_snrLabel;
#endif
#if HAVE_FMLIST_INTERFACE
    FMListInterface *m_fmlistInterface;
#endif
    QProgressBar *m_snrProgressbar;
    ClickableLabel *m_menuLabel;
    ClickableLabel *m_muteLabel;
    QStackedWidget *m_timeBasicQualInfoWidget;
    QLabel *m_timeLabel;
    QLocale m_timeLocale;
    QLabel *m_basicSignalQualityLabel;
    QLabel *m_infoLabel;
    QWidget *m_signalQualityWidget;
    ClickableLabel *m_audioRecordingLabel;
    QLabel *m_audioRecordingProgressLabel;
    QWidget *m_audioRecordingWidget;
    QLabel *m_syncLabel;

    // application menu
    QMenu *m_menu;
    QMenu *m_audioOutputMenu;

    QAction *m_setupAction;
    QAction *m_exportServiceListAction;
    QAction *m_clearServiceListAction;
    QAction *m_bandScanAction;
    QAction *m_ensembleInfoAction;
    QAction *m_tiiAction;
    QAction *m_scanningToolAction;
#if HAVE_QCUSTOMPLOT
    QAction *m_signalDialogAction;
#endif
    QAction *m_aboutAction;
    QAction *m_logAction;
    QAction *m_audioRecordingAction;
    QAction *m_audioRecordingScheduleAction;
    QAction *m_epgAction;
    QActionGroup *m_audioDevicesGroup = nullptr;

    // tray icon
    QSystemTrayIcon *m_trayIcon = nullptr;

    // dark mode
    QString m_defaultStyleName;
    QPalette m_palette;
    QPalette m_darkPalette;

    // radio control
    QThread *m_radioControlThread;
    RadioControl *m_radioControl;

    // input device
    InputDevice::Id m_inputDeviceId = InputDevice::Id::UNDEFINED;
    InputDevice *m_inputDevice = nullptr;
    InputDevice::Id m_inputDeviceRequest = InputDevice::Id::UNDEFINED;
    QVariant m_inputDeviceIdRequest;
    InputDeviceRecorder *m_inputDeviceRecorder = nullptr;

    // audio decoder
    QThread *m_audioDecoderThread;
    AudioDecoder *m_audioDecoder;

    // Audio recording
    AudioRecManager *m_audioRecManager;
    AudioRecScheduleModel *m_audioRecScheduleModel;

    // audio output
    QThread *m_audioOutputThread = nullptr;
    QSlider *m_audioVolumeSlider;
    AudioOutput *m_audioOutput;

    // state variables
    QString m_iniFilename;
    QString m_serviceListFilename;
    QString m_audioRecScheduleFilename;
    Settings *m_settings;
    bool m_isPlaying = false;
    bool m_deviceChangeRequested = false;
    bool m_exitRequested = false;
    int m_exitCode = 0;
    uint32_t m_frequency = 0;
    DabSId m_SId;
    uint8_t m_SCIdS = 0;
    bool m_hasListViewFocus;
    bool m_hasTreeViewFocus;
    int m_audioVolume = 100;
    bool m_keepServiceListOnScan;
    bool m_isScannerRunning = false;
    QDateTime m_dabTime;
    int m_serviceSpiProgress = -1;
    int m_ensSpiProgress = -1;

    // channel list combo
    DABChannelListFilteredModel *m_channelListModel;

    // service list
    ServiceList *m_serviceList;
    SLModel *m_slModel;
    SLTreeModel *m_slTreeModel;

    // user applications
    DLDecoder *m_dlDecoder[Instance::NumInstances];
    QMap<DLPlusContentType, DLPlusObjectUI *> m_dlObjCache[Instance::NumInstances];
    SlideShowApp *m_slideShowApp[Instance::NumInstances];
    SPIApp *m_spiApp;
    MetadataManager *m_metadataManager;

    // methods
    void loadSettings();
    void saveSettings();
    Settings::AudioFramework getAudioFramework();
    void restoreWindows();

    void showEnsembleInfo();
    void showEPG();
    void showAboutDialog();
    void showSetupDialog();
    void showLog();
    void showCatSLS();
    void showAudioRecordingSchedule();
    void showSignalDialog();
    void showTiiDialog();
    void showScannerDialog();
    void setExpertMode(bool ena);
    void stop();
    void bandScan();
    void audioRecordingToggle();
    void setAudioRecordingUI();
    void exportServiceList();
    void clearServiceList();
    void clearEnsembleInformationLabels();
    void clearServiceInformationLabels();
    void initInputDevice(const InputDevice::Id &d, const QVariant &id);
    void configureForInputDevice();
    bool isDarkMode();
    void forceDarkStyle(bool ena);
    void setupDarkMode();
    void serviceSelected();
    void channelSelected();
    void serviceTreeViewUpdateSelection();
    void serviceListViewUpdateSelection();
    void changeInputDevice(const InputDevice::Id &d, const QVariant &id);
    void displaySubchParams(const RadioControlServiceComponent &s);
    void toggleDLPlus(bool toggle);
    void initStyle();
    void restoreTimeQualWidget();
    bool stopAudioRecordingMsg(const QString &infoText);
    void selectService(const ServiceListId &serviceId);
    void uploadEnsembleCSV(const RadioControlEnsemble &ens, const QString &csv, bool isRequested);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    void onColorSchemeChanged(Qt::ColorScheme colorScheme);
#endif
    void onInputDeviceReady();
    void onEnsembleInfo(const RadioControlEnsemble &ens);
    void onServiceListComplete(const RadioControlEnsemble &ens);
    void onEnsembleReconfiguration(const RadioControlEnsemble &ens) const;
    void onEnsembleRemoved(const RadioControlEnsemble &ens);
    void onChannelChange(int index);
    void onBandScanStart();
    void onChannelUpClicked();
    void onChannelDownClicked();
    void onBandScanFinished(int result);
    void onFavoriteToggled(bool checked);
    void onAudioVolumeSliderChanged(int volume);
    void onMuteLabelToggled(bool doMute);
    void onSwitchSourceClicked();
    void onAnnouncementClicked();
    void onApplicationStyleChanged(Settings::ApplicationStyle style);
    void onExpertModeToggled(bool checked);
    void onSignalState(uint8_t sync, float snr);
    void onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry);
    void onDLComplete_Service(const QString &dl);
    void onDLComplete_Announcement(const QString &dl);
    void onDLComplete(const QString &dl, QLabel *dlLabel);
    void onDLPlusObjReceived_Service(const DLPlusObject &object);
    void onDLPlusObjReceived_Announcement(const DLPlusObject &object);
    void onDLPlusObjReceived(const DLPlusObject &object, Instance inst);
    void onDLPlusItemToggle_Service();
    void onDLPlusItemToggle_Announcement();
    void onDLPlusItemToggle(Instance inst);
    void onDLPlusItemRunning_Service(bool isRunning);
    void onDLPlusItemRunning_Announcement(bool isRunning);
    void onDLPlusItemRunning(bool isRunning, Instance inst);
    void onDLReset_Service();
    void onDLReset_Announcement();
    void onAudioParametersInfo(const AudioParameters &params);
    void onProgrammeTypeChanged(const DabSId &sid, const struct DabPTy &pty);
    void onDabTime(const QDateTime &d);
    void onTuneChannel(uint32_t freq);
    void onTuneDone(uint32_t freq);
    void onNewAnnouncementSettings();
    void onInputDeviceError(const InputDevice::ErrorCode errCode);
    void onServiceListSelection(const QItemSelection &selected, const QItemSelection &deselected);
    void onServiceListTreeSelection(const QItemSelection &selected, const QItemSelection &deselected);
    void onAudioServiceSelection(const RadioControlServiceComponent &s);
    void onAudioServiceReconfiguration(const RadioControlServiceComponent &s);
    void onAnnouncement(const DabAnnouncement id, const RadioControlAnnouncementState state, const RadioControlServiceComponent &s);
    void onAudioDevicesList(QList<QAudioDevice> list);
    void onAudioOutputError();
    void onAudioOutputSelected(QAction *action);
    void onAudioDeviceChanged(const QByteArray &id);
    void onAudioRecordingStarted();
    void onAudioRecordingStopped();
    void onAudioRecordingProgress(size_t bytes, qint64 timeSec);
    void onAudioRecordingCountdown(int numSec);
    void onMetadataUpdated(const ServiceListId &id, MetadataManager::MetadataRole role);
    void onEpgEmpty();
    void onSpiProgress(bool isEns, int decoded, int total);
    void drawSpiProgressLabel(QLabel *label, int progress);
    void setProxy();
    void checkForUpdate();
};

class DLPlusObjectUI
{
public:
    DLPlusObjectUI(const DLPlusObject &obj);
    ~DLPlusObjectUI();
    QHBoxLayout *getLayout() const;
    void update(const DLPlusObject &obj);
    void setVisible(bool visible);
    const DLPlusObject &getDlPlusObject() const;
    QString getLabel(DLPlusContentType type) const;

private:
    DLPlusObject m_dlPlusObject;
    QHBoxLayout *m_layout;
    QLabel *m_tagLabel;
    QLabel *m_tagText;
};

#endif  // MAINWINDOW_H
