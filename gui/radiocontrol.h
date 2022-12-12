#ifndef RADIOCONTROL_H
#define RADIOCONTROL_H

#include <QObject>
#include <QMap>
#include <QHash>
#include <QDateTime>
#include <QStringList>
#include <QDebug>
#include <QTimer>
#include <QThread>

#include "dabtables.h"
#include "dabsdr.h"


#define RADIO_CONTROL_UEID_INVALID  0xFF000000
#define RADIO_CONTROL_VERBOSE  0
#define RADIO_CONTROL_N_CHANNELS_ENABLE  0
#define RADIO_CONTROL_NOTIFICATION_PERIOD  3  // 2^3 = 8 DAB frames = 8*96ms = 768ms
// number of FIB expected to be received during noticication period
// there are 12 FIB's in one DAB frame
#define RADIO_CONTROL_NOTIFICATION_FIB_EXPECTED  (12*(1 << RADIO_CONTROL_NOTIFICATION_PERIOD))

#define RADIO_CONTROL_ENSEMBLE_TIMEOUT_SEC (10)
#define RADIO_CONTROL_ANNOUNCEMENT_TIMEOUT_SEC (5)

// this is used for testing of receiver perfomance, it allows ensemble ECC = 0
// and data services without user application
#define RADIO_CONTROL_TEST_MODE 0

#define RADIO_CONTROL_SPI_ENABLE 0

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
    uint8_t ecc() const { return uint8_t(m_eccsid >> 16);}
    void set(uint32_t sid, uint8_t ensEcc = 0)
    {
        if (0 == (sid & 0xFF000000))
        {   // programme service ID
            uint8_t ecc = (sid >> 16);
            if (0 == ecc) { ecc = ensEcc; }
            m_eccsid = (ecc << 16) | (sid & 0x0000FFFF);
        }
        else
        {   // data service ID
            m_eccsid = sid;
        }
    }
    uint32_t countryServiceRef() const { return (isProgServiceId() ? (m_eccsid & 0x0000FFFF) : (m_eccsid & 0x00FFFFFF));  }
    bool isValid() const { return m_eccsid != 0; }
private:
    uint32_t m_eccsid;
};

struct DabProtection
{
    DabProtectionLevel level;
    union {
        struct {
            uint8_t codeRateLower : 4;
            uint8_t codeRateUpper : 3;
            uint8_t fecScheme     : 1;
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
    uint32_t frequency;
    uint32_t ueid;
    int8_t LTO;       // Ensemble LTO (Local Time Offset): Local Time Offset (LTO) for the ensemble.
                      // It is expressed in multiples of half hours in the range -15,5 hours to +15,5 hours
    uint8_t intTable;  // Inter. (International) Table Id: this 8-bit field shall be used to select an international table
    uint8_t alarm;
    QString label;
    QString labelShort;

    uint16_t eid() const { return uint16_t(ueid); }
    uint8_t ecc() const { return (ueid >> 16); }

    bool isValid() const { return (0 != frequency) && (RADIO_CONTROL_UEID_INVALID != ueid); }
};
Q_DECLARE_METATYPE(RadioControlEnsemble)

struct RadioControlUserApp
{
    QString label;        // Service label
    QString labelShort;   // Short label
    DabUserApplicationType uaType;      // User application type
    struct {
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
    uint8_t SCIdS;        // Service Component Identifier within the Service (SCIdS)
    uint8_t SubChId;      // SubChId (Sub-channel Identifier)
    uint16_t SubChAddr;   // address 0-863
    uint16_t SubChSize;   // subchannel size
    int8_t ps;            // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one
    int8_t lang;          // language [8.1.2] this 8-bit field shall indicate the language of the audio or data service component
    DabPTy pty;           // Static and dynamic programme type [8.1.5]
    uint8_t CAId;         // CAId (Conditional Access Identifier): this 3-bit field shall identify the
                          // Access Control System (ACS) used for the service
    bool CAflag;          // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component

    //int8_t numUserApps;   // Number of user applications
    QHash<DabUserApplicationType,RadioControlUserApp> userApps;

    QString label;        // Service label
    QString labelShort;   // Short label

    struct DabProtection protection;  // Protection infomation EEP/UEP/FEC scheme

    // TMStreamAudio = 0,
    // TMStreamData  = 1,
    // TMPacketData  = 3
    DabTMId TMId;       // TMId (Transport Mechanism Identifier)
    union
    {
        struct  // TMId == 0 or 1
        {
            DabAudioDataSCty scType;
            uint16_t bitRate;
        } streamAudioData;
        struct  // TMId == 3
        {
            DabAudioDataSCty DSCTy; // DSCTy (Data Service Component Type)
            uint16_t SCId;          // SCId (Service Component Identifier): this 12-bit field shall uniquely
                                    // identify the service component within the ensemble
            bool DGflag;            // DG flag: this 1-bit flag shall indicate whether data groups are used to transport the service component as follows:
                                    //   0: data groups are used to transport the service component;
                                    //   1: data groups are not used to transport the service component.
            uint16_t packetAddress; // this 10-bit field shall define the address of the packet in which the service component is carried.
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

struct RadioControlUserAppData
{
    DabUserApplicationType userAppType;
    QByteArray data;
};

enum class RadioControlEventType
{
    SYNC_STATUS = 0,
    TUNE,
    ENSEMBLE_INFO,
    SERVICE_LIST,
    SERVICE_COMPONENT_LIST,
    USER_APP_LIST,
    ANNOUNCEMENT_SUPPORT,
    SERVICE_SELECTION,
    SERVICE_STOP,
    XPAD_APP_START_STOP,
    AUTO_NOTIFICATION,
    DATAGROUP_DL,
    USERAPP_DATA,
    AUDIO_DATA,
    RECONFIGURATION,
    RESET,
    ANNOUNCEMENT,
};

struct RadioControlEvent
{
    RadioControlEventType type;
    dabsdrNotificationStatus_t status;
    // service identification where needed
    uint32_t SId;
    uint8_t SCIdS;
    union
    {   // this is data payload transferred from DABSDR
        // sync level
        dabsdrSyncLevel_t syncLevel;
        // tuned frequency
        uint32_t frequency;
        // ensemble information
        dabsdrNtfEnsemble_t * pEnsembleInfo;
        // service list
        QList<dabsdrServiceListItem_t> * pServiceList;
        // service component list
        QList<dabsdrServiceCompListItem_t> * pServiceCompList;
        // user applications list
        QList<dabsdrUserAppListItem_t> * pUserAppList;
        // supported announcements
        dabsdrNtfAnnouncementSupport_t * pAnnouncementSupport;
        // xpad application started / stopped
        dabsdrXpadAppStartStop_t * pXpadAppStartStopInfo;
        // periodic notification
        dabsdrNtfPeriodic_t * pNotifyData;
        // reset occured
        dabsdrNtfResetFlags_t resetFlag;
        // user application data group
        RadioControlUserAppData * pUserAppData;
        // dynamic labale data group
        QByteArray * pDataGroupDL;
        // announcement switching
        dabsdrNtfAnnouncement_t * pAnnouncement;
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
    void startUserApplication(DabUserApplicationType uaType, bool start);
    uint32_t getEnsembleUEID() const { return m_ensemble.ueid; }

signals:
    void dabEvent(RadioControlEvent * pEvent);
    void syncStatus(uint8_t sync);
    void snrLevel(float snr);
    void freqOffset(float f);
    void fibCounter(int expected, int errors);
    void mscCounter(int correct, int errors);
    void tuneInputDevice(uint32_t freq);
    void tuneDone(uint32_t freq);    
    void serviceListEntry(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void serviceListComplete(const RadioControlEnsemble & ens);
    void dlDataGroup(const QByteArray & dg);
    void userAppData(const RadioControlUserAppData & data);
    void audioServiceSelection(const RadioControlServiceComponent & s);
    void audioServiceReconfiguration(const RadioControlServiceComponent & s);
    void audioData(QByteArray * pData);
    void dabTime(const QDateTime & dateAndTime);   
    void ensembleInformation(const RadioControlEnsemble & ens);
    void ensembleConfiguration(const QString &);
    void ensembleReconfiguration(const RadioControlEnsemble & ens);
    void ensembleRemoved(const RadioControlEnsemble & ens);
    void announcement(DabAnnouncement id, bool ongoing);
private:
    static const uint8_t EEPCoderate[];

    dabsdrHandle_t m_dabsdrHandle;
    dabsdrSyncLevel_t m_syncLevel;
    bool m_enaAutoNotification = false;
    uint32_t m_frequency;
    struct {
        uint32_t SId;
        uint8_t SCIdS;
    } m_serviceRequest;
    struct {
        uint32_t SId;
        uint8_t SCIdS;

        // announcement support
        uint16_t ASu;
        QList<uint8_t> clusterIds;

        uint8_t activeCluster = 0;
        QTimer * announcementTimeoutTimer;
    } m_currentService;

    // this is a counter of requests to check
    // when the service list is complete
    int m_numReqPendingServiceList = 0;

    // this is a counter of requests to check
    // when the ensemble information is complete
    int m_numReqPendingUserApp = 0;

    // set when ensemble information is complete
    bool m_isEnsembleComplete = false;
    QTimer * m_ensembleInfoTimeoutTimer;

    bool m_isReconfigurationOngoing = false;

    RadioControlEnsemble m_ensemble;
    RadioControlServiceList m_serviceList;

    typedef RadioControlServiceCompList::iterator serviceComponentIterator;
    typedef RadioControlServiceCompList::const_iterator serviceComponentConstIterator;
    typedef RadioControlServiceList::iterator serviceIterator;
    typedef RadioControlServiceList::const_iterator serviceConstIterator;

    bool getCurrentAudioServiceComponent(serviceComponentIterator & scIt);
    bool cgetCurrentAudioServiceComponent(serviceComponentConstIterator & scIt) const;

    void updateSyncLevel(dabsdrSyncLevel_t s);
    QString toShortLabel(QString & label, uint16_t charField) const;
    QString ensembleConfigurationString() const;
    void clearEnsemble();
    void onEnsembleInfoTimeout();
    bool isCurrentService(uint32_t sid, uint8_t scids) { return ((sid == m_currentService.SId) && (scids == m_currentService.SCIdS)); }
    void resetCurrentService();
    void setCurrentServiceAnnouncementSupport();
    void onAnnouncementTimeout();

    // wrapper functions for dabsdr API
    void dabTune(uint32_t freq) { dabsdrRequest_Tune(m_dabsdrHandle, freq); }
    void dabGetEnsembleInfo() { dabsdrRequest_GetEnsemble(m_dabsdrHandle); }
    void dabGetServiceList() { dabsdrRequest_GetServiceList(m_dabsdrHandle); }
    void dabGetServiceComponent(uint32_t SId) { dabsdrRequest_GetServiceComponents(m_dabsdrHandle, SId); }    
    void dabGetUserApps(uint32_t SId, uint8_t SCIdS) { dabsdrRequest_GetUserAppList(m_dabsdrHandle, SId, SCIdS); }
    void dabGetAnnouncementSupport(uint32_t SId) { dabsdrRequest_GetAnnouncementSupport(m_dabsdrHandle, SId); }
    void dabEnableAutoNotification() { dabsdrRequest_SetPeriodicNotify(m_dabsdrHandle, RADIO_CONTROL_NOTIFICATION_PERIOD, 0); }
    void dabServiceSelection(uint32_t SId, uint8_t SCIdS) { dabsdrRequest_ServiceSelection(m_dabsdrHandle, SId, SCIdS); }
    void dabServiceStop(uint32_t SId, uint8_t SCIdS) { dabsdrRequest_ServiceStop(m_dabsdrHandle, SId, SCIdS); }
    void dabXPadAppStart(uint8_t appType, bool start) { dabsdrRequest_XPadAppStart(m_dabsdrHandle, appType, start); }

    // wrappers unsed in callback functions (emit requires class instance)
    void emit_dabEvent(RadioControlEvent * pEvent) { emit dabEvent(pEvent); }
    void emit_audioData(QByteArray * pData) { emit audioData(pData); }

    // static methods used as dabsdr library callbacks
    static void dabNotificationCb(dabsdrNotificationCBData_t * p, void * ctx);
    static void dynamicLabelCb(dabsdrDynamicLabelCBData_t * p, void * ctx);
    static void dataGroupCb(dabsdrDataGroupCBData_t * p, void * ctx);
    static void audioDataCb(dabsdrAudioCBData_t * p, void * ctx);
private slots:
    void onDabEvent(RadioControlEvent * pEvent);
};


#endif // RADIOCONTROL_H
