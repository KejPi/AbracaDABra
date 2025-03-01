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

#ifndef RADIOCONTROL_H
#define RADIOCONTROL_H

#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "dabsdr.h"
#include "dabtables.h"

#define RADIO_CONTROL_UEID_INVALID 0xFF000000
#define RADIO_CONTROL_N_CHANNELS_ENABLE 0
#define RADIO_CONTROL_NOTIFICATION_PERIOD 3  // 2^3 = 8 DAB frames = 8*96ms = 768ms
// number of FIB expected to be received during noticication period
// there are 12 FIB's in one DAB frame
#define RADIO_CONTROL_NOTIFICATION_FIB_EXPECTED (12 * (1 << RADIO_CONTROL_NOTIFICATION_PERIOD))

#define RADIO_CONTROL_ENSEMBLE_CONFIGURATION_UPDATE_TIMEOUT_SEC (1)
#define RADIO_CONTROL_ANNOUNCEMENT_TIMEOUT_SEC (5)

// this is used for testing of receiver perfomance, it allows ensemble ECC = 0
// and data services without user application
#define RADIO_CONTROL_TEST_MODE 0

enum class DabSyncLevel
{
    NoSync = 0,
    NullSync = 1,
    FullSync = 2,
};

class DabSId
{
public:
    DabSId() : m_eccsid(0) {};
    DabSId(uint32_t sid, uint8_t ensEcc = 0) { set(sid, ensEcc); }
    bool isProgServiceId() const { return 0 == (m_eccsid & 0xFF000000); }
    uint32_t value() const { return m_eccsid; }
    uint16_t progSId() const { return uint16_t(m_eccsid); }
    uint8_t ecc() const { return uint8_t(m_eccsid >> 16); }
    uint16_t gcc() const
    {
        return isProgServiceId() ? ((m_eccsid & 0x00FF0000) >> 16) | ((m_eccsid & 0x0000F000) >> 4)
                                 : ((m_eccsid & 0xFF000000) >> 24) | ((m_eccsid & 0x00F00000) >> 12);
    }
    void set(uint32_t sid, uint8_t ensEcc = 0)
    {
        if (0 == (sid & 0xFF000000))
        {  // programme service ID
            uint8_t ecc = (sid >> 16);
            if (0 == ecc)
            {
                ecc = ensEcc;
            }
            m_eccsid = (ecc << 16) | (sid & 0x0000FFFF);
        }
        else
        {  // data service ID
            m_eccsid = sid;
        }
    }
    uint16_t eccc() const { return ((isProgServiceId() ? (m_eccsid >> 12) : (m_eccsid >> 20)) & 0x0FFF); }
    uint32_t countryServiceRef() const { return (isProgServiceId() ? (m_eccsid & 0x0000FFFF) : (m_eccsid & 0x00FFFFFF)); }
    bool isValid() const { return m_eccsid != 0; }
    inline bool operator==(const DabSId &other) const { return m_eccsid == other.m_eccsid; }

private:
    uint32_t m_eccsid;
};
inline size_t qHash(const DabSId &sid, size_t seed = 0)
{
    return sid.value();
}

struct DabProtection
{
    DabProtectionLevel level;
    union
    {
        struct
        {
            uint8_t codeRateLower : 4;
            uint8_t codeRateUpper : 3;
            uint8_t fecScheme : 1;
        };
        uint8_t codeRateFecValue;
        uint8_t uepIndex;
    };
    bool isEEP() const { return level > DabProtectionLevel::UEP_MAX; }
};

struct DabPTy
{
    uint8_t s;  // static
    uint8_t d;  // dynamic
};

struct RadioControlEnsemble
{
    uint32_t frequency = 0;
    uint32_t ueid = RADIO_CONTROL_UEID_INVALID;
    int8_t LTO;        // Ensemble LTO (Local Time Offset): Local Time Offset (LTO) for the ensemble.
                       // It is expressed in multiples of half hours in the range -15,5 hours to +15,5 hours
    uint8_t intTable;  // Inter. (International) Table Id: this 8-bit field shall be used to select an international table
    uint8_t alarm;
    QString label;
    QString labelShort;

    uint16_t eid() const { return uint16_t(ueid); }
    uint8_t ecc() const { return (ueid >> 16); }

    bool isValid() const { return (0 != frequency) && (RADIO_CONTROL_UEID_INVALID != ueid); }
    void reset()
    {
        frequency = 0;
        ueid = RADIO_CONTROL_UEID_INVALID;
    }
};
Q_DECLARE_METATYPE(RadioControlEnsemble)

struct RadioControlUserApp
{
    QString label;                  // Service label
    QString labelShort;             // Short label
    DabUserApplicationType uaType;  // User application type
    struct
    {
        DabAudioDataSCty DScTy;
        int8_t xpadAppTy;
        bool dgFlag;
        bool CAOrgFlag;
        bool CAflag;
    } xpadData;
    QList<uint8_t> uaData;  // optional user application data
};

struct RadioControlServiceComponent
{
    // Each service component shall be uniquely identified by the combination of the
    // SId and the Service Component Identifier within the Service (SCIdS).
    DabSId SId;
    uint8_t SCIdS;       // Service Component Identifier within the Service (SCIdS)
    uint8_t SubChId;     // SubChId (Sub-channel Identifier)
    uint16_t SubChAddr;  // address 0-863
    uint16_t SubChSize;  // subchannel size
    int8_t ps;           // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one
    int8_t lang;         // language [8.1.2] this 8-bit field shall indicate the language of the audio or data service component
    DabPTy pty;          // Static and dynamic programme type [8.1.5]
    uint8_t CAId;        // CAId (Conditional Access Identifier): this 3-bit field shall identify the
                         // Access Control System (ACS) used for the service
    bool CAflag;         // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component

    // int8_t numUserApps;   // Number of user applications
    bool userAppsValid;  // flag that indicates that UA list was read (used to check is all UA were loaded already)
    QHash<DabUserApplicationType, RadioControlUserApp> userApps;

    QString label;       // Service label
    QString labelShort;  // Short label

    struct DabProtection protection;  // Protection infomation EEP/UEP/FEC scheme

    // TMStreamAudio = 0,
    // TMStreamData  = 1,
    // TMPacketData  = 3
    DabTMId TMId;  // TMId (Transport Mechanism Identifier)
    union
    {
        struct  // TMId == 0 or 1
        {
            DabAudioDataSCty scType;
            uint16_t bitRate;
        } streamAudioData;
        struct  // TMId == 3
        {
            DabAudioDataSCty DSCTy;  // DSCTy (Data Service Component Type)
            uint16_t SCId;           // SCId (Service Component Identifier): this 12-bit field shall uniquely
                                     // identify the service component within the ensemble
            bool DGflag;  // DG flag: this 1-bit flag shall indicate whether data groups are used to transport the service component as follows:
                          //   0: data groups are used to transport the service component;
                          //   1: data groups are not used to transport the service component.
            uint16_t packetAddress;  // this 10-bit field shall define the address of the packet in which the service component is carried.
        } packetData;
    };

    // this is used to track automatic enabling of data services (like SLS in secondary data service)
    bool autoEnabled;

    bool isAudioService() const { return DabTMId::StreamAudio == TMId; }
    bool isDataStreamService() const { return DabTMId::StreamData == TMId; }
    bool isDataPacketService() const { return DabTMId::PacketData == TMId; }
};
Q_DECLARE_METATYPE(RadioControlServiceComponent)

typedef QMap<uint8_t, RadioControlServiceComponent> RadioControlServiceCompList;

struct RadioControlService
{
    // Each service shall be identified by a Service Identifier (SId)
    // Data services use 32bit IDs, Programme services use 16 bits
    DabSId SId;
    QString label;
    QString labelShort;
    DabPTy pty;

    // CAId (Conditional Access Identifier): this 3-bit field shall identify the
    // Access Control System (ACS) used for the service
    uint8_t CAId;

    // Announcements support
    uint16_t ASu;

    // cluster ids
    QList<uint8_t> clusterIds;

    RadioControlServiceCompList serviceComponents;
};

typedef QMap<uint32_t, RadioControlService> RadioControlServiceList;

struct RadioControlDataDL
{
    dabsdrDecoderId_t id;
    QByteArray data;
};

struct RadioControlUserAppData
{
    dabsdrDecoderId_t id;
    uint16_t SCId;
    DabUserApplicationType userAppType;
    QByteArray data;
};

struct RadioControlAudioData
{
    dabsdrDecoderId_t id;
    DabAudioDataSCty ASCTy;
    dabsdrAudioFrameHeader_t header;
    std::vector<uint8_t> data;
};

struct RadioControlTIIData
{
    RadioControlTIIData() : spectrum(384) {}
    QList<dabsdrTii_t> idList;
    std::vector<float> spectrum;
};

enum class RadioControlEventType
{
    SYNC_STATUS = 0,
    TUNE,
    ENSEMBLE_INFO,
    SERVICE_LIST,
    SERVICE_COMPONENT_LIST,
    USER_APP_UPDATE,
    USER_APP_LIST,
    ANNOUNCEMENT_SUPPORT,
    SERVICE_SELECTION,
    STOP_SERVICE,
    XPAD_APP_START_STOP,
    AUTO_NOTIFICATION,
    DATAGROUP_DL,
    USERAPP_DATA,
    AUDIO_DATA,
    RECONFIGURATION,
    RESET,
    ANNOUNCEMENT_SWITCHING,
    PROGRAMME_TYPE,
    TII
};

enum class RadioControlAnnouncementState
{
    None,
    OnCurrentService,
    OnOtherService,
    Suspended
};

struct RadioControlEvent
{
    RadioControlEventType type;
    dabsdrNotificationStatus_t status;
    // service identification where needed
    uint32_t SId;
    uint8_t SCIdS;
    union
    {  // this is data payload transferred from DABSDR
        // sync status
        dabsdrNtfSyncStatus_t syncStatus;
        // tuned frequency
        uint32_t frequency;
        // ensemble information
        dabsdrNtfEnsemble_t *pEnsembleInfo;
        // service list
        QList<dabsdrServiceListItem_t> *pServiceList;
        // service component list
        QList<dabsdrServiceCompListItem_t> *pServiceCompList;
        // user applications list
        QList<dabsdrUserAppListItem_t> *pUserAppList;
        // supported announcements
        dabsdrNtfAnnouncementSupport_t *pAnnouncementSupport;
        // xpad application started / stopped
        dabsdrNtfXpadAppStartStop_t *pXpadAppStartStopInfo;
        // periodic notification
        dabsdrNtfPeriodic_t *pNotifyData;
        // reset occured
        dabsdrNtfResetFlags_t resetFlag;
        // user application data group
        RadioControlUserAppData *pUserAppData;
        // dynamic labale data group
        RadioControlDataDL *pDynamicLabelData;
        // announcement switching
        dabsdrNtfAnnouncementSwitching_t *pAnnouncement;
        // audio service instance
        dabsdrDecoderId_t decoderId;
        // programme type change
        dabsdrNtfPTy_t *pPty;
        // TII data
        RadioControlTIIData *pTII;
    };
};

class RadioControl : public QObject
{
    Q_OBJECT
public:
    explicit RadioControl(QObject *parent = nullptr);
    ~RadioControl();

    bool init();
    void start(uint32_t freq);
    void exit();
    void tuneService(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void getEnsembleConfiguration();
    void getEnsembleCSV();
    void getEnsembleCSV_FMLIST();
    void getEnsembleConfigurationAndCSV();
    void getEnsembleInformation();
    void startUserApplication(DabUserApplicationType uaType, bool start, bool singleChannel = true);
    uint32_t getEnsembleUEID() const { return m_ensemble.ueid; }
    void onAudioOutputRestart();
    void setupAnnouncements(uint16_t enaFlags);
    void suspendResumeAnnouncement();
    void onSpiApplicationEnabled(bool enabled);
    void startTii(bool ena);
    void setTii(int mode);
    void setSignalSpectrum(bool ena);

signals:
    void dabEvent(RadioControlEvent *pEvent);
    void signalState(uint8_t sync, float snr);
    void freqOffset(float f);
    void fibCounter(int expected, int errors);
    void mscCounter(int correct, int errors);
    void tuneInputDevice(uint32_t freq);
    void tuneDone(uint32_t freq);
    void stopAudio();
    void serviceListEntry(const RadioControlEnsemble &ens, const RadioControlServiceComponent &slEntry);
    void serviceListComplete(const RadioControlEnsemble &ens);
    void dlDataGroup_Service(const QByteArray &dg);
    void dlDataGroup_Announcement(const QByteArray &dg);
    void userAppData_Service(const RadioControlUserAppData &data);
    void userAppData_Announcement(const RadioControlUserAppData &data);
    void audioServiceSelection(const RadioControlServiceComponent &s);
    void audioServiceReconfiguration(const RadioControlServiceComponent &s);
    void audioData(RadioControlAudioData *pData);
    void signalSpectrum(std::shared_ptr<std::vector<float>> data);
    void dabTime(const QDateTime &dateAndTime);
    void ensembleInformation(const RadioControlEnsemble &ens);
    void ensembleConfiguration(const QString &);
    void ensembleCSV(const QString &);
    void ensembleCSV_FMLIST(const RadioControlEnsemble &ens, const QString &, bool);
    void ensembleConfigurationAndCSV(const QString &, const QString &);
    void ensembleReconfiguration(const RadioControlEnsemble &ens);
    void ensembleRemoved(const RadioControlEnsemble &ens);
    void announcement(DabAnnouncement id, const RadioControlAnnouncementState state, const RadioControlServiceComponent &s);
    void announcementAudioAvailable();
    void programmeTypeChanged(const DabSId &sid, const DabPTy &pty);
    void tiiData(const RadioControlTIIData &data);

private:
    enum class AnnouncementSwitchState
    {
        NoAnnouncement,
        WaitForAnnouncement,
        OngoingAnnouncement
    };

    static const uint8_t EEPCoderate[];

    dabsdrHandle_t m_dabsdrHandle;
    dabsdrSyncLevel_t m_syncLevel;
    bool m_enaAutoNotification = false;
    uint32_t m_frequency;
    struct
    {
        uint32_t SId;
        uint8_t SCIdS;
    } m_serviceRequest;

    struct
    {
        uint32_t SId;
        uint8_t SCIdS;
        struct
        {  // announcement support
            uint16_t ASu;
            QList<uint8_t> clusterIds;
            uint8_t activeCluster = 0;
            DabAnnouncement id;
            QTimer *timeoutTimer;
            std::atomic<AnnouncementSwitchState> switchState;
            uint32_t SId;
            uint8_t SCIdS;

            uint16_t enaFlags;
            bool isOtherService;
            bool suspendRequest;
            RadioControlAnnouncementState state;
        } announcement;
    } m_currentService;

    // this is a counter of requests to check
    // when the service list is complete
    int m_numReqPendingServiceList = 0;

    // set when ensemble information is complete
    QTimer *m_ensembleConfigurationTimer;
    bool m_ensembleConfigurationUpdateRequest = false;
    bool m_ensembleConfigurationSentCSV = false;

    bool m_isReconfigurationOngoing = false;
    bool m_spiAppEnabled = false;

    int m_tiiEna = 0;
    dabsdrTiiMode_t m_tiiMode = DABSDR_TII_MODE_DEFAULT;

    RadioControlEnsemble m_ensemble;
    RadioControlServiceList m_serviceList;

    typedef RadioControlServiceCompList::iterator serviceComponentIterator;
    typedef RadioControlServiceCompList::const_iterator serviceComponentConstIterator;
    typedef RadioControlServiceList::iterator serviceIterator;
    typedef RadioControlServiceList::const_iterator serviceConstIterator;

    bool getCurrentAudioServiceComponent(serviceComponentIterator &scIt);
    bool cgetCurrentAudioServiceComponent(serviceComponentConstIterator &scIt) const;

    QString toShortLabel(QString &label, uint16_t charField) const;

    void clearEnsemble();
    QString ensembleConfigurationString() const;
    QString ensembleConfigurationCSV() const;
    QString ensembleConfigurationCSV_FMLIST() const;
    void ensembleConfigurationUpdate();
    void ensembleConfigurationDispatch();
    bool isCurrentService(uint32_t sid, uint8_t scids) { return ((sid == m_currentService.SId) && (scids == m_currentService.SCIdS)); }
    void resetCurrentService();
    void updateSignalState(dabsdrSyncLevel_t s, int16_t snr10);
    void setCurrentServiceAnnouncementSupport();
    void onAnnouncementTimeout();
    void onAnnouncementAudioAvailable();
    void announcementHandler(dabsdrAsw_t *pAnnouncement);
    bool startAnnouncement(uint8_t subChId);
    void stopAnnouncement();
    inline QString removeTrailingSpaces(QString &s) const;

    // event handlers
    void eventHandler_ensembleInfo(RadioControlEvent *pEvent);
    void eventHandler_serviceList(RadioControlEvent *pEvent);
    void eventHandler_serviceComponentList(RadioControlEvent *pEvent);
    void eventHandler_userAppList(RadioControlEvent *pEvent);
    void eventHandler_serviceSelection(RadioControlEvent *pEvent);
    void eventHandler_serviceStop(RadioControlEvent *pEvent);
    void eventHandler_announcementSupport(RadioControlEvent *pEvent);
    void eventHandler_announcementSwitching(RadioControlEvent *pEvent);
    void eventHandler_programmeType(RadioControlEvent *pEvent);

    // wrapper functions for dabsdr API
    void dabTune(uint32_t freq) { dabsdrRequest_Tune(m_dabsdrHandle, freq); }
    void dabGetEnsembleInfo() { dabsdrRequest_GetEnsemble(m_dabsdrHandle); }
    void dabGetServiceList() { dabsdrRequest_GetServiceList(m_dabsdrHandle); }
    void dabGetServiceComponents(uint32_t SId) { dabsdrRequest_GetServiceComponents(m_dabsdrHandle, SId); }
    void dabGetUserApps(uint32_t SId, uint8_t SCIdS) { dabsdrRequest_GetUserAppList(m_dabsdrHandle, SId, SCIdS); }
    void dabGetAnnouncementSupport(uint32_t SId) { dabsdrRequest_GetAnnouncementSupport(m_dabsdrHandle, SId); }
    void dabEnableAutoNotification() { dabsdrRequest_SetPeriodicNotify(m_dabsdrHandle, RADIO_CONTROL_NOTIFICATION_PERIOD, 0); }
    void dabServiceSelection(uint32_t SId, uint8_t SCIdS, dabsdrDecoderId_t decoderId)
    {
        dabsdrRequest_ServiceSelection(m_dabsdrHandle, SId, SCIdS, decoderId);
    }
    void dabServiceStop(uint32_t SId, uint8_t SCIdS, dabsdrDecoderId_t decoderId)
    {
        dabsdrRequest_ServiceStop(m_dabsdrHandle, SId, SCIdS, decoderId);
    }
    void dabXPadAppStart(uint8_t appType, bool start, dabsdrDecoderId_t decoderId)
    {
        dabsdrRequest_XPadAppStart(m_dabsdrHandle, appType, start, decoderId);
    }
    void dabSetTii(bool ena, dabsdrTiiMode_t mode) { dabsdrRequest_SetTII(m_dabsdrHandle, ena, mode); }
    void dabEnableSignalSpectrum(bool ena) { dabsdrRequest_SignalSpectrum(m_dabsdrHandle, ena); }

    // wrappers used in callback functions (emit requires class instance)
    void emit_dabEvent(RadioControlEvent *pEvent) { emit dabEvent(pEvent); }
    void emit_audioData(RadioControlAudioData *pData) { emit audioData(pData); }
    void emit_announcementAudioAvailable() { emit announcementAudioAvailable(); }
    void emit_spectrum(std::shared_ptr<std::vector<float>> data) { emit signalSpectrum(data); }

    // static methods used as dabsdr library callbacks
    static void dabNotificationCb(dabsdrNotificationCBData_t *p, void *ctx);
    static void dynamicLabelCb(dabsdrDynamicLabelCBData_t *p, void *ctx);
    static void dataGroupCb(dabsdrDataGroupCBData_t *p, void *ctx);
    static void audioDataCb(dabsdrAudioCBData_t *p, void *ctx);
    static void signalSpectrumCb(const float *p, void *ctx);
private slots:
    void onDabEvent(RadioControlEvent *pEvent);
};

#endif  // RADIOCONTROL_H
