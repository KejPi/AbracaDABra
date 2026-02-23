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

#ifndef SETTINGSBACKEND_H
#define SETTINGSBACKEND_H

#include <QGeoCoordinate>
#include <QList>
#include <QLocale>
#include <QNetworkReply>
#include <QQmlApplicationEngine>
#include <QSortFilterProxyModel>

#include "config.h"
#include "dabtables.h"
#include "itemmodel.h"
#include "settings.h"
#include "soapysdrgainmodel.h"
#include "tiitablecolssettingsmodel.h"
#include "uicontrolprovider.h"

class SettingsBackend : public UIControlProvider
{
    Q_OBJECT
    Q_PROPERTY(ItemModel *tabsModel READ tabsModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *inputsModel READ inputsModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *rawFileFormatModel READ rawFileFormatModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *rtlSdrDevicesModel READ rtlSdrDevicesModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *airspyDevicesModel READ airspyDevicesModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *sdrplayDevicesModel READ sdrplayDevicesModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *sdrplayAntennaModel READ sdrplayAntennaModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *sdrplayChannelModel READ sdrplayChannelModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *audioNoiseConcealModel READ audioNoiseConcealModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *audioDecoderModel READ audioDecoderModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *audioOutputModel READ audioOutputModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *announcementModel READ announcementModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *locationSourceModel READ locationSourceModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *serialPortBaudrateModel READ serialPortBaudrateModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *languageSelectionModel READ languageSelectionModel CONSTANT FINAL)
    Q_PROPERTY(ItemModel *proxyConfigModel READ proxyConfigModel CONSTANT FINAL)
    Q_PROPERTY(TiiTableColsSettingsModel *tiiTableColsModel READ tiiTableColsModel CONSTANT FINAL)
    Q_PROPERTY(SoapySdrGainModel *soapySdrGainModel READ soapySdrGainModel CONSTANT FINAL)
    Q_PROPERTY(QString slsDumpPaternDefault READ slsDumpPaternDefault CONSTANT FINAL)
    Q_PROPERTY(QString spiDumpPaternDefault READ spiDumpPaternDefault CONSTANT FINAL)
    Q_PROPERTY(QString coordinatesHelp READ coordinatesHelp CONSTANT FINAL)

    Q_PROPERTY(bool haveRtlSdrOldDabDriver READ haveRtlSdrOldDabDriver CONSTANT FINAL)
    Q_PROPERTY(bool haveAirspy READ haveAirspy CONSTANT FINAL)
    Q_PROPERTY(bool haveSoapySdr READ haveSoapySdr CONSTANT FINAL)

    Q_PROPERTY(int rawFileProgressValue READ rawFileProgressValue WRITE setRawFileProgressValue NOTIFY rawFileProgressValueChanged FINAL)
    Q_PROPERTY(int rtlSdrGainIndex READ rtlSdrGainIndex WRITE setRtlSdrGainIndex NOTIFY rtlSdrGainIndexChanged FINAL)
    Q_PROPERTY(int rtlSdrAgcLevelThr READ rtlSdrAgcLevelThr WRITE setRtlSdrAgcLevelThr NOTIFY rtlSdrAgcLevelThrChanged FINAL)
    Q_PROPERTY(int rtlTcpGainIndex READ rtlTcpGainIndex WRITE setRtlTcpGainIndex NOTIFY rtlTcpGainIndexChanged FINAL)
    Q_PROPERTY(int rtlTcpAgcLevelThr READ rtlTcpAgcLevelThr WRITE setRtlTcpAgcLevelThr NOTIFY rtlTcpAgcLevelThrChanged FINAL)
    Q_PROPERTY(QString locationCoordinates READ locationCoordinates WRITE setLocationCoordinates NOTIFY locationCoordinatesChanged FINAL)

    UI_PROPERTY(QString, statusLabel)
    UI_PROPERTY_DEFAULT(bool, isInputDeviceSelectionEnabled, true);

    UI_PROPERTY_DEFAULT(int, rawFileProgressMax, 0)
    UI_PROPERTY_DEFAULT(bool, isConnectButton, true)
    UI_PROPERTY_DEFAULT(bool, isConnectButtonEnabled, false)
    UI_PROPERTY_DEFAULT(bool, isRawFileProgressVisible, false)
    UI_PROPERTY_DEFAULT(bool, isRawFileProgressEnabled, false)
    UI_PROPERTY_DEFAULT(bool, isRawFileFormatSelectionEnabled, false)
    UI_PROPERTY(QStringList, rawFileXmlHeader)
    UI_PROPERTY(QString, rawFileName)
    UI_PROPERTY(QString, rawFileProgressLabel)
    UI_PROPERTY_SETTINGS(bool, isRawFileLoopActive, m_settings->rawfile.loopEna)

    UI_PROPERTY(QStringList, rtlSdrDeviceDesc)
    UI_PROPERTY_DEFAULT(int, rtlSdrGainIndexMax, -1)
    UI_PROPERTY_DEFAULT(bool, isRtlSdrDeviceSelectionEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isRtlSdrControlEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isRtlSdrGainEnabled, false);
    UI_PROPERTY(QString, rtlSdrGainLabel)
    UI_PROPERTY_DEFAULT(int, rtlSdrGainMode, 0)
    UI_PROPERTY_DEFAULT(int, rtlSdrBandWidth, 0)
    UI_PROPERTY_DEFAULT(int, rtlSdrFreqCorrection, 0)
    UI_PROPERTY_DEFAULT(float, rtlSdrRfLevelCorrection, 0)
    UI_PROPERTY_DEFAULT(bool, rtlSdrBiasT, false)
    UI_PROPERTY_SETTINGS(bool, isRtlSdrFallbackChecked, m_settings->rtlsdr.fallbackConnection)

    UI_PROPERTY(QString, rtlTcpIpAddress)
    UI_PROPERTY_DEFAULT(int, rtlTcpPort, 1234)
    UI_PROPERTY_DEFAULT(bool, isRtlTcpControlSocketChecked, true)
    UI_PROPERTY(QStringList, rtlTcpDeviceDesc)
    UI_PROPERTY_DEFAULT(bool, isRtlTcpControlEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isRtlTcpGainEnabled, false);
    UI_PROPERTY(QString, rtlTcpGainLabel)
    UI_PROPERTY_DEFAULT(int, rtlTcpGainIndexMax, -1)
    UI_PROPERTY_DEFAULT(int, rtlTcpGainMode, 0)
    UI_PROPERTY_DEFAULT(int, rtlTcpFreqCorrection, 0)
    UI_PROPERTY_DEFAULT(float, rtlTcpRfLevelCorrection, 0)

#if HAVE_AIRSPY
    Q_PROPERTY(int airspySensitivityGainIndex READ airspySensitivityGainIndex WRITE setAirspySensitivityGainIndex NOTIFY
                   airspySensitivityGainIndexChanged FINAL)
    Q_PROPERTY(int airspyIfGainIndex READ airspyIfGainIndex WRITE setAirspyIfGainIndex NOTIFY airspyIfGainIndexChanged FINAL)
    Q_PROPERTY(int airspyMixerGainIndex READ airspyMixerGainIndex WRITE setAirspyMixerGainIndex NOTIFY airspyMixerGainIndexChanged FINAL)
    Q_PROPERTY(int airspyLnaGainIndex READ airspyLnaGainIndex WRITE setAirspyLnaGainIndex NOTIFY airspyLnaGainIndexChanged FINAL)
    Q_PROPERTY(bool isAirspyMixerAgcChecked READ isAirspyMixerAgcChecked WRITE setIsAirspyMixerAgcChecked NOTIFY isAirspyMixerAgcCheckedChanged FINAL)
    Q_PROPERTY(bool isAirspyLnaAgcChecked READ isAirspyLnaAgcChecked WRITE setIsAirspyLnaAgcChecked NOTIFY isAirspyLnaAgcCheckedChanged FINAL)

    UI_PROPERTY_DEFAULT(bool, isAirspyDeviceSelectionEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isAirspyControlEnabled, false);
    UI_PROPERTY(QStringList, airspyDeviceDesc)
    UI_PROPERTY_DEFAULT(int, airspyGainMode, 0)
    UI_PROPERTY_DEFAULT(bool, isAirspySensitivityGainEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isAirspyManualGainEnabled, false);
    UI_PROPERTY_DEFAULT(bool, airspyBiasT, false)
    UI_PROPERTY_SETTINGS(bool, isAirspyFallbackChecked, m_settings->airspy.fallbackConnection);
    UI_PROPERTY_SETTINGS(bool, airspyPrefer4096kHz, m_settings->airspy.prefer4096kHz)
#endif
#if HAVE_SOAPYSDR
    UI_PROPERTY(QStringList, soapySdrDeviceDesc)
    UI_PROPERTY(QString, soapySdrDevArgs)
    UI_PROPERTY_DEFAULT(int, soapySdrChannelNum, 0)
    UI_PROPERTY(QString, soapySdrAntenna)
    UI_PROPERTY_DEFAULT(bool, isSoapySdrControlEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isSoapySdrGainEnabled, false);
    UI_PROPERTY_DEFAULT(int, soapySdrGainMode, 0)
    UI_PROPERTY_DEFAULT(int, soapySdrBandWidth, 0)
    UI_PROPERTY_DEFAULT(int, soapySdrFreqCorrection, 0)

    Q_PROPERTY(bool isSdrplayIfAgcChecked READ isSdrplayIfAgcChecked WRITE setIsSdrplayIfAgcChecked NOTIFY isSdrplayIfAgcCheckedChanged FINAL)
    Q_PROPERTY(int sdrplayRfGainIndex READ sdrplayRfGainIndex WRITE setSdrplayRfGainIndex NOTIFY sdrplayRfGainIndexChanged FINAL)
    Q_PROPERTY(int sdrplayIfGain READ sdrplayIfGain WRITE setSdrplayIfGain NOTIFY sdrplayIfGainChanged FINAL)
    UI_PROPERTY(QStringList, sdrplayDeviceDesc)
    UI_PROPERTY_DEFAULT(bool, isSdrplayDeviceSelectionEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isSdrplayControlEnabled, false);
    UI_PROPERTY_DEFAULT(int, sdrplayGainMode, 0)
    UI_PROPERTY_DEFAULT(int, sdrplayRfGainIndexMax, -1)
    UI_PROPERTY(QString, sdrplayRfGainLabel)
    UI_PROPERTY_DEFAULT(int, sdrplayFreqCorrection, 0)
    UI_PROPERTY_DEFAULT(bool, sdrplayBiasT, false)
    UI_PROPERTY_SETTINGS(bool, isSdrplayFallbackChecked, m_settings->sdrplay.fallbackConnection);
#endif

    UI_PROPERTY(QString, rartTcpIpAddress)
    UI_PROPERTY_DEFAULT(int, rartTcpPort, 1235)

    UI_PROPERTY(QString, audioRecPath)
    UI_PROPERTY_SETTINGS(bool, audioRecCaptureOutput, m_settings->audioRec.captureOutput)
    UI_PROPERTY_SETTINGS(bool, audioRecDl, m_settings->audioRec.dl)
    UI_PROPERTY_SETTINGS(bool, audioRecDlAbsTime, m_settings->audioRec.dlAbsTime)
    UI_PROPERTY_SETTINGS(bool, audioRecAutoStop, m_settings->audioRec.autoStopEna)
    UI_PROPERTY_DEFAULT(bool, audioDecoderChanged, false)
    UI_PROPERTY_DEFAULT(bool, audioOutputChanged, false)

    UI_PROPERTY_SETTINGS(bool, annBringToForeground, m_settings->bringWindowToForeground)
    UI_PROPERTY_SETTINGS(bool, isSpiAppEnabled, m_settings->spiAppEna)
    UI_PROPERTY_SETTINGS(bool, isSpiInternetEnabled, m_settings->useInternet)
    UI_PROPERTY_SETTINGS(bool, isRadioDnsEnabled, m_settings->radioDnsEna)
    UI_PROPERTY_SETTINGS(bool, isSpiProgressEnabled, m_settings->spiProgressEna)
    UI_PROPERTY_SETTINGS(bool, spiProgressHideComplete, m_settings->spiProgressHideComplete)
    UI_PROPERTY_SETTINGS(bool, isUaDumpOverwiteEnabled, m_settings->uaDump.overwriteEna)
    UI_PROPERTY_SETTINGS(bool, isUaDumpSlsEnabled, m_settings->uaDump.slsEna)
    UI_PROPERTY_SETTINGS(bool, isUaDumpSpiEnabled, m_settings->uaDump.spiEna)
    UI_PROPERTY_SETTINGS(QString, uaDumpSlsPattern, m_settings->uaDump.slsPattern)
    UI_PROPERTY_SETTINGS(QString, uaDumpSpiPattern, m_settings->uaDump.spiPattern)
    UI_PROPERTY(QString, dataDumpPath)

    UI_PROPERTY(QString, tiiDbLabel)
    UI_PROPERTY(QString, tiiLogPath)
    UI_PROPERTY_DEFAULT(bool, isTiiDbUpdating, false)
    UI_PROPERTY_DEFAULT(bool, isTiiUpdateEnabled, false)
    UI_PROPERTY_SETTINGS(QString, tiiSerialPort, m_settings->tii.serialPort)
    UI_PROPERTY_SETTINGS(bool, tiiLogUtcTimestamp, m_settings->tii.timestampInUTC)
    UI_PROPERTY_SETTINGS(bool, tiiLogCoordinates, m_settings->tii.saveCoordinates)
    UI_PROPERTY_SETTINGS(int, tiiModeValue, m_settings->tii.mode)
    UI_PROPERTY_SETTINGS(bool, tiiShowSpectrumPlot, m_settings->tii.showSpectumPlot)
    UI_PROPERTY_SETTINGS(bool, tiiShowInactive, m_settings->tii.showInactiveTx)
    UI_PROPERTY_SETTINGS(bool, isTiiInactiveTimeoutEnabled, m_settings->tii.inactiveTxTimeoutEna)
    UI_PROPERTY_SETTINGS(int, tiiInactiveTimeout, m_settings->tii.inactiveTxTimeout)

    UI_PROPERTY_DEFAULT(int, applicationTheme, 0)
    UI_PROPERTY_SETTINGS(bool, showDlPlus, m_settings->dlPlusEna)
    UI_PROPERTY_SETTINGS(bool, showTrayIcon, m_settings->trayIconEna)
    UI_PROPERTY_SETTINGS(bool, showSystemTime, m_settings->showSystemTime)
    UI_PROPERTY_SETTINGS(bool, showEnsembleCountryFlag, m_settings->showEnsFlag)
    UI_PROPERTY_SETTINGS(bool, showServiceCountryFlag, m_settings->showServiceFlag)
    UI_PROPERTY_SETTINGS(bool, fullscreen, m_settings->appWindow.fullscreen)
    UI_PROPERTY_SETTINGS(bool, compactUi, m_settings->compactUi)
    UI_PROPERTY_SETTINGS(bool, cableChannelsEna, m_settings->cableChannelsEna)
    UI_PROPERTY_SETTINGS(QColor, slsBackgroundColor, m_settings->slsBackground)
    UI_PROPERTY_DEFAULT(bool, languageChanged, false)
    UI_PROPERTY(QString, proxyServer)
    UI_PROPERTY_DEFAULT(int, proxyPort, 0)
    UI_PROPERTY(QString, proxyUser)
    UI_PROPERTY(QString, proxyPass)
    UI_PROPERTY_DEFAULT(bool, isProxyApplyEnabled, false)
    UI_PROPERTY_SETTINGS(bool, uploadEnsembleInfo, m_settings->uploadEnsembleInfo)
    UI_PROPERTY_SETTINGS(bool, restoreWindowsOnStart, m_settings->restoreWindows)
    UI_PROPERTY_SETTINGS(bool, isCheckForUpdatesEnabled, m_settings->updateCheckEna)
    UI_PROPERTY_SETTINGS(bool, isXmlHeaderEnabled, m_settings->xmlHeaderEna)
    UI_PROPERTY_SETTINGS(QString, dataStoragePath, m_settings->dataStoragePath)

public:
    SettingsBackend(QQmlApplicationEngine *qmlEngine, QObject *parent = nullptr);

    Q_INVOKABLE QUrl rawFilePath() const;
    Q_INVOKABLE void selectRawFile(const QUrl &fileUrl);
    Q_INVOKABLE void requestRawFileSeek(int msec) { emit rawFileSeek(msec); };
    Q_INVOKABLE void requestConnectDisconnectDevice();
    Q_INVOKABLE void rtlSdrReloadDeviceList() { reloadDeviceList(InputDevice::Id::RTLSDR, m_rtlSdrDevicesModel); }
    Q_INVOKABLE void requestRtlSdrGainMode(int modeIdx);
    Q_INVOKABLE void requestRtlTcpGainMode(int modeIdx);
#if HAVE_AIRSPY
    Q_INVOKABLE void requestAirspyGainMode(int modeIdx);
    Q_INVOKABLE void airspyReloadDeviceList() { reloadDeviceList(InputDevice::Id::AIRSPY, m_airspyDevicesModel); }
#endif
#if HAVE_SOAPYSDR
    Q_INVOKABLE void requestSoapySdrGainMode(int modeIdx);
    Q_INVOKABLE void requestSdrplayGainMode(int modeIdx);
    Q_INVOKABLE void sdrplayReloadRequest();
#endif
    Q_INVOKABLE void requestRestart() { emit restartRequested(); }
    Q_INVOKABLE QUrl dataStoragePathUrl() const;
    Q_INVOKABLE void selectDataStoragePath(const QUrl &dirUrl);
    Q_INVOKABLE void requestTiiDbUpdate();
    Q_INVOKABLE void requestVisualStyle(int index);
    Q_INVOKABLE void setSlsBackground(const QColor &color);
    Q_INVOKABLE void requestApplyProxyConfig();
    Q_INVOKABLE QString getDisplayPath(const QString &uriPath) const;

    ItemModel *tabsModel() const { return m_tabsModel; }
    ItemModel *inputsModel() const { return m_inputsModel; }
    ItemModel *rawFileFormatModel() const { return m_rawFileFormatModel; }
    ItemModel *rtlSdrDevicesModel() const { return m_rtlSdrDevicesModel; }
    ItemModel *airspyDevicesModel() const { return m_airspyDevicesModel; }
    SoapySdrGainModel *soapySdrGainModel() const { return m_soapySdrGainModel; }
    ItemModel *sdrplayDevicesModel() const { return m_sdrplayDevicesModel; }
    ItemModel *sdrplayAntennaModel() const { return m_sdrplayAntennaModel; }
    ItemModel *sdrplayChannelModel() const { return m_sdrplayChannelModel; }
    ItemModel *audioNoiseConcealModel() const { return m_audioNoiseConcealModel; }
    ItemModel *audioDecoderModel() const { return m_audioDecoderModel; }
    ItemModel *audioOutputModel() const { return m_audioOutputModel; }
    ItemModel *announcementModel() const { return m_announcementModel; }
    ItemModel *locationSourceModel() const { return m_locationSourceModel; }
    ItemModel *serialPortBaudrateModel() const { return m_serialPortBaudrateModel; }
    ItemModel *languageSelectionModel() const { return m_languageSelectionModel; }
    ItemModel *proxyConfigModel() const { return m_proxyConfigModel; }
    TiiTableColsSettingsModel *tiiTableColsModel() const { return m_tiiTableColsModel; }

    void setInputDevice(InputDevice::Id id, InputDevice *device);
    void resetInputDevice();
    void setSettings(Settings *settings);
    void setRawFileLength(int msec);
    int rawFileProgressValue() const;
    void setRawFileProgressValue(int msec);
    QLocale::Language applicationLanguage() const;
    void setSlsDumpPaternDefault(const QString &newSlsDumpPaternDefault);
    void setSpiDumpPaternDefault(const QString &newSpiDumpPaternDefault);
    void onTiiUpdateFinished(QNetworkReply::NetworkError err);
    void setTiiDbUpdate();
    QString slsDumpPaternDefault() const { return m_slsDumpPaternDefault; }
    QString spiDumpPaternDefault() const { return m_spiDumpPaternDefault; }
    QString coordinatesHelp() const { return m_coordinatesHelp; }
    QString locationCoordinates() const;
    void setLocationCoordinates(const QString &locationCoordinates);

    bool haveRtlSdrOldDabDriver() const;
    bool haveAirspy() const;
    bool haveSoapySdr() const;

    int rtlSdrGainIndex() const;
    void setRtlSdrGainIndex(int rtlSdrGainIndex);

    int rtlSdrAgcLevelThr() const;
    void setRtlSdrAgcLevelThr(int rtlSdrAgcLevelThr);

    int rtlTcpGainIndex() const;
    void setRtlTcpGainIndex(int rtlTcpGainIndex);

    int rtlTcpAgcLevelThr() const;
    void setRtlTcpAgcLevelThr(int rtlTcpAgcLevelThr);

#if HAVE_AIRSPY
    int airspySensitivityGainIndex() const;
    void setAirspySensitivityGainIndex(int airspySensitivityGainIndex);

    int airspyIfGainIndex() const;
    void setAirspyIfGainIndex(int newAirspyIfGainIndex);

    bool isAirspyMixerAgcChecked() const;
    void setIsAirspyMixerAgcChecked(bool newIsAirspyMixerAgcChecked);

    bool isAirspyLnaAgcChecked() const;
    void setIsAirspyLnaAgcChecked(bool isAirspyLnaAgcChecked);

    int airspyMixerGainIndex() const;
    void setAirspyMixerGainIndex(int airspyMixerGainIndex);

    int airspyLnaGainIndex() const;
    void setAirspyLnaGainIndex(int airspyLnaGainIndex);
#endif
#if HAVE_SOAPYSDR
    bool isSdrplayIfAgcChecked() const;
    void setIsSdrplayIfAgcChecked(bool isSdrplayIfAgcChecked);

    int sdrplayRfGainIndex() const;
    void setSdrplayRfGainIndex(int newSdrplayRfGainIndex);

    int sdrplayIfGain() const;
    void setSdrplayIfGain(int sdrplayIfGain);
#endif

signals:
    void inputDeviceChanged(const InputDevice::Id &inputDevice, const QVariant &id);
    void newAnnouncementSettings();
    void applicationStyleChanged();
    void noiseConcealmentLevelChanged(int level);
    void xmlHeaderToggled(bool enabled);
    void spiApplicationEnabled(bool enabled);
    void spiApplicationSettingsChanged(bool useInternet, bool enaRadioDNS);
    void spiIconSettingsChanged();
    void audioRecordingSettings(const QString &folder, bool doOutputRecording);
    void uaDumpSettings(const Settings::UADumpSettings &settings);
    void tiiSettingsChanged();
    void tiiTableSettingsChanged();
    void tiiModeChanged(int mode);
    void rawFileSeek(int msec);
    void updateTxDb();
    void proxySettingsChanged();
    void restartRequested();

    void rawFileProgressValueChanged();
    void rawFileFormatChanged();
    void rtlSdrGainIndexChanged();
    void rtlSdrAgcLevelThrChanged();
    void rtlTcpGainIndexChanged();
    void rtlTcpAgcLevelThrChanged();
    void airspySensitivityGainIndexChanged();
    void airspyIfGainIndexChanged();
    void isAirspyMixerAgcCheckedChanged();
    void isAirspyLnaAgcCheckedChanged();
    void airspyMixerGainIndexChanged();
    void airspyLnaGainIndexChanged();
    void isSdrplayIfAgcCheckedChanged();
    void sdrplayRfGainIndexChanged();
    void sdrplayIfGainChanged();

    void locationCoordinatesChanged();

protected:
    // void showEvent(QShowEvent *event);

private:
    enum SettingsBackendTabs
    {
        Device = 0,
        Audio,
        Announcement,
        UserApps,
        Tii,
        Other,
        NumTabs
    };
    enum SettingsBackendXmlHeader
    {
        XMLDate = 0,
        XMLRecorder,
        XMLDevice,
        XMLModel,
        XMLSampleRate,
        XMLFreq,
        XMLLength,
        XMLFormat,
        XMLNumLabels
    };
    enum SettingsBackendConnectButtonState
    {
        ConnectButtonOn = 0,
        ConnectButtonOff,
        ConnectButtonAuto
    };
    static const QString m_coordinatesHelp;

    const QList<QLocale::Language> m_supportedLocalization = {QLocale::Czech, QLocale::German, QLocale::Polish};
    const QString m_noFileString = tr("No file selected");

    Settings *m_settings = nullptr;
    InputDevice::Id m_inputDeviceId = InputDevice::Id::UNDEFINED;
    InputDevice *m_device = nullptr;
    QList<float> m_rtlsdrGainList;
    QList<float> m_rtltcpGainList;

#if HAVE_SOAPYSDR
    QList<float> m_sdrplayGainList;
#endif

    QString m_slsDumpPaternDefault;
    QString m_spiDumpPaternDefault;

    void setStatusLabel(bool clearLabel = false);

    void onCurrentDeviceChanged();

    void setGainValues(const QList<float> &gainList);
    void setDeviceDescription(const InputDevice::Description &desc);
    void reloadDeviceList(const InputDevice::Id inputDeviceId, ItemModel *model);
    void setConnectButton(SettingsBackendConnectButtonState state);

    void onBandwidthChanged();
    void onFrequencyCorrectionChanged();
    void onBiasTChanged();
    void onRfLevelOffsetChanged();

    void activateRtlSdrControls(bool en);
    void updateRtlSdrGainLabel();

    void activateRtlTcpControls(bool en);
    void updateRtlTcpGainLabel();

    void updateRawFileProgressLabel();
    void updateAnnouncementFlags();

    void onLanguageChanged();
    void onNoiseLevelChanged();
    void onAudioOutChanged();
    void onAudioDecChanged();
    void onGeolocationSourceChanged();

#if HAVE_AIRSPY
    void activateAirspyControls(bool en);
#endif
#if HAVE_SOAPYSDR
    void updateSdrplayRfGainLabel();
    void activateSoapySdrControls(bool en);
    void setSoapySdrGainModel(bool activate);

    void activateSdrplayControls(bool en);
    void onSdrplayDeviceChanged();
    void onSdrplayChannelChanged();
    void onSdrplayAntennaChanged();
#endif
    ItemModel *m_inputsModel = nullptr;
    ItemModel *m_rawFileFormatModel = nullptr;
    ItemModel *m_rtlSdrDevicesModel = nullptr;
    ItemModel *m_airspyDevicesModel = nullptr;
    ItemModel *m_sdrplayDevicesModel = nullptr;
    ItemModel *m_sdrplayChannelModel = nullptr;
    ItemModel *m_sdrplayAntennaModel = nullptr;

    int m_rawFileProgressValue = 0;
    int m_rtlSdrGainIndex = -1;
    int m_rtlTcpGainIndex;
    int m_rtlTcpAgcLevelThr;
    SoapySdrGainModel *m_soapySdrGainModel = nullptr;
    ItemModel *m_audioNoiseConcealModel = nullptr;
    ItemModel *m_audioDecoderModel = nullptr;
    ItemModel *m_audioOutputModel = nullptr;
    ItemModel *m_announcementModel = nullptr;
    ItemModel *m_tabsModel = nullptr;
    ItemModel *m_locationSourceModel = nullptr;
    ItemModel *m_serialPortBaudrateModel = nullptr;
    QString m_locationCoordinates;
    ItemModel *m_languageSelectionModel = nullptr;
    ItemModel *m_proxyConfigModel = nullptr;
    TiiTableColsSettingsModel *m_tiiTableColsModel = nullptr;
};

class AnnouncementsProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool alarm READ alarm WRITE setAlarm NOTIFY alarmChanged FINAL)
public:
    AnnouncementsProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
    Q_INVOKABLE void setChecked(int row, bool checked)
    {
        QModelIndex idx = mapToSource(index(row, 0));
        if (idx.isValid())
        {
            sourceModel()->setData(idx, checked, ItemModel::ItemModelRoles::DataRole);
        }
    }
    bool alarm() const { return m_alarm; }
    void setAlarm(bool alarm)
    {
        if (m_alarm == alarm)
        {
            return;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
#endif

        m_alarm = alarm;

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        endFilterChange();
#else
        invalidateFilter();
#endif
        emit alarmChanged();
    }

signals:
    void alarmChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
    {
        QModelIndex idx = sourceModel()->index(source_row, 0);
        auto id = static_cast<DabAnnouncement>(idx.row());
        return (m_alarm == ((id == DabAnnouncement::Alarm) || (id == DabAnnouncement::AlarmTest)));
    }

private:
    bool m_alarm;
};

#endif  // SETTINGSBACKEND_H
