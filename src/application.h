/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCloseEvent>
#include <QDesktopServices>
#include <QEvent>
#include <QFontDatabase>
#include <QItemSelection>
#include <QLoggingCategory>
#include <QPalette>
#include <QQuickImageProvider>
#include <QStandardItemModel>
// #include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "audiodecoder.h"
#include "audiooutput.h"
#include "audiorecmanager.h"
#include "audiorecschedulemodel.h"
#include "catslsbackend.h"
#include "config.h"
#include "contextmenumodel.h"
#include "dabchannellistmodel.h"
#include "dldecoder.h"
#include "epgbackend.h"
#include "inputdevice.h"
#include "inputdevicerecorder.h"
#include "messageboxbackend.h"
#include "metadatamanager.h"
#include "radiocontrol.h"
#include "scannerbackend.h"
#include "servicelist.h"
#include "settingsbackend.h"
#include "signalbackend.h"
#include "slideshowapp.h"
#include "slmodel.h"
#include "sltreemodel.h"
#include "spiapp.h"
#include "tiibackend.h"
#include "uicontrolprovider.h"

class ApplicationUI : public UIControlProvider
{
    Q_OBJECT
    QML_ELEMENT
public:
    using UIControlProvider::UIControlProvider;

    enum ColorName
    {
        Background = 0,
        BackgroundLight,
        BackgroundDark,
        StatusbarBackground,
        Divider,
        Accent,
        Disabled,
        Hovered,
        Clicked,
        Highlight,
        Inactive,
        SelectionColor,
        ListItemHovered,
        ListItemSelected,
        ButtonPrimary,
        ButtonNeutral,
        ButtonPositive,
        ButtonNegative,
        ButtonTextPrimary,
        ButtonTextNeutral,
        ButtonTextPositive,
        ButtonTextNegative,
        ButtonPrimaryHover,
        ButtonNeutralHover,
        ButtonPositiveHover,
        ButtonNegativeHover,
        ButtonPrimaryClicked,
        ButtonNeutralClicked,
        ButtonPositiveClicked,
        ButtonNegativeClicked,
        TextPrimary,
        TextSecondary,
        TextDisabled,
        TextSelected,
        Link,
        Icon,
        IconInactive,
        IconDisabled,
        InputBackground,
        ControlBackground,
        ControlBorder,
        NoSignal,
        LowSignal,
        MidSignal,
        GoodSignal,
        EpgCurrentProgColor,
        EpgCurrentProgProgressColor,
        EmptyLogoColor,
        MessagePositive,
        MessageNeutral,
        MessageNegative,
        MessageTextPositive,
        MessageTextNeutral,
        MessageTextNegative,

        NumColors,
    };
    Q_ENUM(ColorName)
    enum ViewType
    {
        LandscapeView = 0,
        PortraitWideView = 1,
        PortraitNarrowView = 2
    };
    Q_ENUM(ViewType)

    ApplicationUI(QObject *parent = nullptr) : UIControlProvider(parent) { m_colors.resize(NumColors); }

    Q_PROPERTY(QString fixedFontFamily READ fixedFontFamily CONSTANT FINAL)
    Q_PROPERTY(QList<QColor> colors READ colors NOTIFY colorsChanged FINAL)
    Q_PROPERTY(bool isPortraitView READ isPortraitView NOTIFY isPortraitViewChanged FINAL)
    Q_PROPERTY(int currentView READ currentView WRITE setCurrentView NOTIFY currentViewChanged FINAL)

    UI_PROPERTY_DEFAULT(int, serviceViewLandscapeWidth, 650)
    UI_PROPERTY_DEFAULT(int, channelIndex, -1)
    UI_PROPERTY_DEFAULT(bool, tuneEnabled, false);
    UI_PROPERTY_DEFAULT(bool, serviceSelectionEnabled, false);
    UI_PROPERTY(QString, frequencyLabel)
    UI_PROPERTY(QString, timeLabel)
    UI_PROPERTY(QString, timeLabelToolTip)
    UI_PROPERTY(QString, ensembleLabel)
    UI_PROPERTY(QString, ensembleLabelToolTip)
    UI_PROPERTY(QString, serviceLabel)
    UI_PROPERTY(QString, serviceLabelToolTip)
    UI_PROPERTY(QString, programTypeLabel)
    UI_PROPERTY(QString, programTypeLabelToolTip)
    UI_PROPERTY(QString, channelUpToolTip)
    UI_PROPERTY(QString, channelDownToolTip)
    UI_PROPERTY(QString, audioEncodingLabel)
    UI_PROPERTY(QString, audioEncodingLabelToolTip)
    UI_PROPERTY(QString, stereoLabel)
    UI_PROPERTY(QString, stereoLabelToolTip)
    UI_PROPERTY(QString, protectionLabel)
    UI_PROPERTY(QString, protectionLabelToolTip)
    UI_PROPERTY(QString, audioBitrateLabel)
    UI_PROPERTY(QString, audioBitrateLabelToolTip)
    UI_PROPERTY_DEFAULT(uint64_t, serviceId, 0)
    UI_PROPERTY_DEFAULT(uint64_t, ensembleId, 0)
    UI_PROPERTY_DEFAULT(int, signalQualityLevel, 0)
    UI_PROPERTY_DEFAULT(bool, isServiceLogoVisible, false);
    UI_PROPERTY_DEFAULT(bool, isEnsembleLogoVisible, false);
    UI_PROPERTY_DEFAULT(bool, isServiceFlagVisible, false);
    UI_PROPERTY_DEFAULT(bool, isEnsembleFlagVisible, false);
    UI_PROPERTY_DEFAULT(bool, isServiceFavorite, false);
    UI_PROPERTY_DEFAULT(bool, isServiceSelected, false);
    UI_PROPERTY(QString, favoriteButtonToolTip)
    UI_PROPERTY(QString, serviceFlagToolTip)
    UI_PROPERTY(QString, ensembleFlagToolTip)
    UI_PROPERTY(QString, snrLabel)

    UI_PROPERTY_DEFAULT(int, serviceInstance, 0)
    UI_PROPERTY(QString, dlService)
    UI_PROPERTY(QString, dlAnnouncement)
    UI_PROPERTY_DEFAULT(bool, isDlPlusVisible, false);

    UI_PROPERTY_DEFAULT(bool, isAnnouncementOngoing, false);
    UI_PROPERTY_DEFAULT(bool, isAnnouncementActive, false);
    UI_PROPERTY_DEFAULT(bool, isAnnouncementButtonEnabled, false);
    UI_PROPERTY(QString, announcementButtonTooltip)

    UI_PROPERTY_DEFAULT(bool, isMuted, false);
    UI_PROPERTY_DEFAULT(int, audioVolume, 100);

    UI_PROPERTY_DEFAULT(bool, isSpiProgressServiceVisible, false);
    UI_PROPERTY_DEFAULT(bool, isSpiProgressEnsVisible, false);
    UI_PROPERTY_DEFAULT(int, spiProgressService, -1);
    UI_PROPERTY_DEFAULT(int, spiProgressEns, -1);
    UI_PROPERTY(QString, spiProgressServiceToolTip)
    UI_PROPERTY(QString, spiProgressEnsToolTip)

    UI_PROPERTY(QString, audioRecordingProgressLabel)
    UI_PROPERTY(QString, audioRecordingProgressLabelToolTip)

    UI_PROPERTY(QStringList, updateVersionInfo)

    UI_PROPERTY_DEFAULT(int, infoLabelIndex, 0)
    UI_PROPERTY(QString, messageInfoTitle)
    UI_PROPERTY(QString, messageInfoDetails)
    UI_PROPERTY_DEFAULT(bool, isSystemDarkMode, false)
    UI_PROPERTY_DEFAULT(bool, isCompact, false)

    QString fixedFontFamily() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont).family(); }
    QList<QColor> colors() const { return m_colors; }

signals:
    void colorsChanged();
    void isPortraitViewChanged();
    void currentViewChanged();

public:
    void setupDarkMode(bool isDarkMode);
    QColor color(ColorName name) const { return m_colors.value(static_cast<int>(name)); }
    bool isPortraitView() const { return m_isPortraitView; }
    int currentView() const { return m_currentView; }
    void setCurrentView(int newCurrentView);
    void setIsPortraitView(bool portraitView);

private:
    QList<QColor> m_colors;
    int m_currentView = ViewType::LandscapeView;
    bool m_isPortraitView = false;
};

class DLPlusObjectUI;
class FMListInterface;
class QQmlApplicationEngine;
class NavigationModel;
class DLPlusModel;
class SLSBackend;
class EnsembleInfoBackend;
class LogBackend;
class AboutUI;

class Application : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MessageBoxBackend *messageBoxBackend READ messageBoxBackend CONSTANT FINAL)
    Q_PROPERTY(ContextMenuModel *audioOutputMenuModel READ audioOutputMenuModel CONSTANT FINAL)
    Q_PROPERTY(ContextMenuModel *serviceSourcesMenuModel READ serviceSourcesMenuModel CONSTANT FINAL)
    Q_PROPERTY(SignalBackend *signalBackend READ signalBackend NOTIFY signalBackendChanged FINAL)
    Q_PROPERTY(TIIBackend *tiiBackend READ tiiBackend NOTIFY tiiBackendChanged FINAL)
    Q_PROPERTY(ScannerBackend *scannerBackend READ scannerBackend NOTIFY scannerBackendChanged FINAL)
    Q_PROPERTY(EPGBackend *epgBackend READ epgBackend NOTIFY epgBackendChanged FINAL)
    Q_PROPERTY(CatSLSBackend *catSlsBackend READ catSlsBackend NOTIFY catSlsBackendChanged FINAL)

public:
    static int const EXIT_CODE_RESTART;

    Application(const QString &iniFilename = QString(), const QString &iniSlFilename = QString(), QObject *parent = nullptr);
    ~Application();

    QQmlApplicationEngine *qmlEngine() const;

    Q_INVOKABLE void setChannelIndex(int index);
    Q_INVOKABLE void onChannelUpClicked();
    Q_INVOKABLE void onChannelDownClicked();
    Q_INVOKABLE void setCurrentServiceFavorite(bool checked);
    Q_INVOKABLE void setServiceFavorite(const QModelIndex &index, bool checked);
    Q_INVOKABLE void onMuteButtonToggled(bool doMute);
    Q_INVOKABLE void onAnnouncementClicked();
    Q_INVOKABLE void openLink(const QString &link) { QDesktopServices::openUrl(QUrl::fromUserInput(link)); }
    Q_INVOKABLE void copyDlToClipboard();
    Q_INVOKABLE void copyDlPlusToClipboard();
    Q_INVOKABLE void audioRecordingToggle();
    Q_INVOKABLE void exportServiceList();
    Q_INVOKABLE void clearServiceList();
    Q_INVOKABLE QObject *createBackend(int id);
    Q_INVOKABLE void close();
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height) const;
    Q_INVOKABLE QVariantMap restoreWindowGeometry();
    Q_INVOKABLE void saveUndockedWindows() const;
    Q_INVOKABLE void setAndroidNavigationBar();
    Q_INVOKABLE void setAndroidKeepScreenOn(bool enable);

    bool eventFilter(QObject *obj, QEvent *event) override;

    MessageBoxBackend *messageBoxBackend() const { return m_messageBoxBackend; }
    ContextMenuModel *audioOutputMenuModel() const { return m_audioOutputMenuModel; }
    ContextMenuModel *serviceSourcesMenuModel() const { return m_serviceSourcesMenuModel; }
    SignalBackend *signalBackend();
    TIIBackend *tiiBackend();
    ScannerBackend *scannerBackend();
    EPGBackend *epgBackend();
    CatSLSBackend *catSlsBackend();

signals:
    void undockPageRequest(int functionalityId);
    void showPageRequest(int functionalityId);
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
    void startBandScan();
    void signalBackendChanged();
    void tiiBackendChanged();
    void updateAvailable();
    void showMessage();
    void closeRequested();
    void scannerBackendChanged();
    void epgBackendChanged();
    void catSlsBackendChanged();
    void applicationQuitEvent();
    void showInfoMessage(const QString &message, int type);
    void requestActivate();

private:
    enum Instance
    {
        Service = 0,
        Announcement = 1,
        NumInstances
    };

    enum AudioRecMsg
    {
        AudioRecMsg_ChannelChange = 0,
        AudioRecMsg_SeviceChange = 1
    };

    static const QString appName;
    static const QString slsDumpPatern;
    static const QString spiDumpPatern;

    // QML
    QQmlApplicationEngine *m_qmlEngine = nullptr;
    NavigationModel *m_navigationModel = nullptr;
    ApplicationUI *m_ui = nullptr;
    QItemSelectionModel *m_slSelectionModel = nullptr;
    QItemSelectionModel *m_slTreeSelectionModel = nullptr;
    DLPlusModel *m_dlPlusModel[Instance::NumInstances] = {nullptr};
    SLSBackend *m_slsBackend[Instance::NumInstances] = {nullptr};
    EnsembleInfoBackend *m_ensembleInfoBackend = nullptr;
    CatSLSBackend *m_catSlsBackend = nullptr;
    SignalBackend *m_signalBackend = nullptr;
    TIIBackend *m_tiiBackend = nullptr;
    EPGBackend *m_epgBackend = nullptr;
    LogBackend *m_logBackend = nullptr;
    SettingsBackend *m_settingsBackend = nullptr;
    ScannerBackend *m_scannerBackend = nullptr;
    MessageBoxBackend *m_messageBoxBackend = nullptr;
    ContextMenuModel *m_audioOutputMenuModel = nullptr;
    ContextMenuModel *m_serviceSourcesMenuModel = nullptr;

    void setContextProperties();

#if HAVE_FMLIST_INTERFACE
    FMListInterface *m_fmlistInterface;
#endif
    QLocale m_timeLocale;

    // tray icon
    // QSystemTrayIcon *m_trayIcon = nullptr;

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
    bool m_quitEventIntercepted = false;
    bool m_allowQuitEvent = false;
    uint32_t m_frequency = 0;
    uint32_t m_ueid = 0;
    DabSId m_SId;
    uint8_t m_SCIdS = 0;
    bool m_ensembleRemoved = false;
    bool m_hasListViewFocus;
    bool m_hasTreeViewFocus;
    bool m_isScannerRunning = false;
    float m_snrQualOffset = 0;

    // Time
    QDateTime m_dabTime;
    QTimer *m_sysTimeTimer = nullptr;

    // channel list combo
    DABChannelListFilteredModel *m_channelListModel;

    // service list
    ServiceList *m_serviceList;
    SLModel *m_slModel;
    SLTreeModel *m_slTreeModel;

    // user applications
    DLDecoder *m_dlDecoder[Instance::NumInstances];
    SlideShowApp *m_slideShowApp[Instance::NumInstances];
    SPIApp *m_spiApp;
    MetadataManager *m_metadataManager;

    // methods
    void loadSettings();
    void saveSettings();
    void getAudioSettings(Settings::AudioFramework &framework, Settings::AudioDecoder &decoder);
    void restoreWindows();

    QObject *createTiiBackend();
    QObject *createSignalBackend();
    QObject *createScannerBackend();
    QObject *createEpgBackend();
    QObject *createCatSlsBackend();
    void resetDabTime();
    void stop();
    void clearEnsembleInformationLabels();
    void clearServiceInformationLabels();
    void initInputDevice(const InputDevice::Id &d, const QVariant &id);
    void configureForInputDevice();
    bool isDarkMode();
    void setColorTheme();
    void serviceSelected();
    void channelSelected();
    void serviceTreeViewUpdateSelection();
    void serviceListViewUpdateSelection();
    void changeInputDevice(const InputDevice::Id &d, const QVariant &id);
    void displaySubchParams(const RadioControlServiceComponent &s);
    void setSnrQualOffset(DabProtectionLevel protectionLevel);
    void toggleDLPlus(bool toggle);
    void checkAudioRecording(AudioRecMsg msgId, std::function<void(bool)> callback);
    void selectService(const ServiceListId &serviceId);
    void uploadEnsembleCSV(const RadioControlEnsemble &ens, const QString &csv, bool isRequested);
    QString adjustDLString(const QString &dl);

    void onInputDeviceReady();
    void onEnsembleInfo(const RadioControlEnsemble &ens);
    void onServiceListComplete(const RadioControlEnsemble &ens);
    void onEnsembleReconfiguration(const RadioControlEnsemble &ens) const;
    void onEnsembleRemoved(const RadioControlEnsemble &ens);
    void onBandScanStart();
    void onBandScanFinished(int result);
    void onAudioVolumeChanged();
    void onShowSystemTimeChanged();
    void onShowCountryFlagChanged();
    void onSignalState(uint8_t sync, float snr);
    void onServiceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry);
    void onDLComplete_Service(const QString &dl);
    void onDLComplete_Announcement(const QString &dl);
    void onDLPlusObjReceived_Service(const DLPlusObject &object);
    void onDLPlusObjReceived_Announcement(const DLPlusObject &object);
    void onDLPlusObjReceived(const DLPlusObject &object, Instance inst);
    void onDLPlusItemToggle_Service();
    void onDLPlusItemToggle_Announcement();
    void onDLPlusItemRunning_Service(bool isRunning);
    void onDLPlusItemRunning_Announcement(bool isRunning);
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
    void handleAudioOutputSelection(int actionId, const QVariant &data);
    void onAudioDeviceChanged(const QByteArray &id);
    void onAudioRecordingStarted();
    void onAudioRecordingStopped();
    void onAudioRecordingProgress(size_t bytes, qint64 timeSec);
    void onAudioRecordingCountdown(int numSec);
    void onMetadataUpdated(const ServiceListId &id, MetadataManager::MetadataRole role);
    void onEpgEmpty();
    void onSpiProgressSettingsChanged();
    void onSpiProgress(bool isEns, int decoded, int total);
    void setProxy();
    void checkForUpdate();
    void pageUndocked(int id, bool isUndocked);
    void pageActive(int id, bool isActive);
    void populateServiceSourcesMenu();
    void handleServiceSourceSelection(int actionId, bool checked, const QVariant &data);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void updateAndroidNotification(const QString &title, const QString &text);

    // this class serves as simple image provider for QML using MatedataManager as backend
    class LogoProvider : public QQuickImageProvider
    {
    public:
        explicit LogoProvider(MetadataManager *metadataManager)
            : QQuickImageProvider(QQuickImageProvider::Pixmap), m_metadataManager(metadataManager) {};

        QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
        {
            QStringList list = id.split('/');

            Q_ASSERT(list.size() >= 2);  // at least type and serviceId should be present
            if (list.size() == 3)
            {
                // uint32_t ueid = list.at(1).toUInt();
                ServiceListId ensId = ServiceListId(static_cast<uint64_t>(list.at(1).toULongLong()));  // ServiceListId(0, ueid);
                ServiceListId serviceId = ServiceListId(static_cast<uint64_t>(list.at(2).toULongLong()));
                if (list.first() == "logo")
                {
                    QPixmap logo = m_metadataManager->data(ensId, serviceId, MetadataManager::MetadataRole::SmallLogo).value<QPixmap>();
                    if (logo.isNull())
                    {
                        logo =
                            QPixmap(requestedSize.width() > 0 ? requestedSize.width() : 32, requestedSize.height() > 0 ? requestedSize.height() : 32);
                        logo.fill(QColor(0, 0, 0, 0));
                    }

                    if (size)
                    {
                        *size = QSize(logo.width(), logo.height());
                    }

                    return logo;
                }
            }
            if (list.first() == "flag")
            {
                ServiceListId servId = ServiceListId(static_cast<uint64_t>(list.at(1).toULongLong()));
                QPixmap flag;
                if (servId.isService())
                {
                    flag = m_metadataManager->data(ServiceListId(), servId, MetadataManager::MetadataRole::CountryFlag).value<QPixmap>();
                }
                else
                {
                    flag = m_metadataManager->data(servId, ServiceListId(), MetadataManager::MetadataRole::CountryFlag).value<QPixmap>();
                }
                if (flag.isNull())
                {
                    flag = QPixmap(requestedSize.width() > 0 ? requestedSize.width() : 40, requestedSize.height() > 0 ? requestedSize.height() : 30);
                    flag.fill(QColor(0, 0, 0, 0));
                }

                if (size)
                {
                    *size = QSize(flag.width(), flag.height());
                }

                return flag;
            }

            *size = QSize(0, 0);
            return QPixmap();
        }

    private:
        MetadataManager *m_metadataManager;
    };
};

#endif  // APPLICATION_H
