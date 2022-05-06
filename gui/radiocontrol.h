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
#include "rawfileinput.h"
#include "dabProc.h"


#define RADIO_CONTROL_UEID_INVALID  0xFF000000
#define RADIO_CONTROL_VERBOSE  0
#define RADIO_CONTROL_N_CHANNELS_ENABLE  0
#define RADIO_CONTROL_NOTIFICATION_PERIOD  3  // 2^3 = 8 DAB frames = 8*96ms = 768ms
// number of FIB expeted to be received during noticication period
// there are 12 FIB's in one DAB frame
#define RADIO_CONTROL_NOTIFICATION_FIB_EXPECTED  (12*(1 << RADIO_CONTROL_NOTIFICATION_PERIOD))

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

class DabSId {
public:
    DabSId() : eccsid(0) {};
    DabSId(uint32_t sid, uint8_t ensEcc = 0) { set(sid, ensEcc); }
    bool isProgServiceId() const { return 0 == (eccsid & 0xFF000000); }
    uint32_t value() const { return eccsid; }
    uint16_t progSId() const { return uint16_t(eccsid); }
    uint8_t ecc() const { return uint8_t(eccsid >> 16);}
    void set(uint32_t sid, uint8_t ensEcc = 0)
    {
        if (0 == (sid & 0xFF000000))
        {   // programme service ID
            uint8_t ecc = (sid >> 16);
            if (0 == ecc) { ecc = ensEcc; }
            eccsid = (ecc << 16) | (sid & 0x0000FFFF);
        }
        else
        {   // data service ID
            eccsid = sid;
        }
    }
    uint32_t countryServiceRef() const { return (isProgServiceId() ? (eccsid & 0x0000FFFF) : (eccsid & 0x00FFFFFF));  }
    bool isValid() const { return eccsid != 0; }
private:
    uint32_t eccsid;
};

struct DabProtection {
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

struct DabPTy {
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
    SERVICE_SELECTION,
    SERVICE_STOP,
    XPAD_APP_START_STOP,
    AUTO_NOTIFICATION,
    DATAGROUP_DL,
    USERAPP_DATA,
    AUDIO_DATA,
};

struct RadioControlEvent
{
    RadioControlEventType type;
    dabProcNotificationStatus_t status;
    intptr_t pData;
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
    uint32_t getEnsembleUEID() const { return ensemble.ueid; }

signals:
    void dabEvent(RadioControlEvent * pEvent);
    void syncStatus(uint8_t sync);
    void snrLevel(float snr);
    void freqOffset(float f);
    void fibCounter(int expected, int errors);
    void mscCounter(int correct, int errors);
    void tuneDone(uint32_t freq);
    void ensembleInformation(const RadioControlEnsemble & ens);
    void serviceListEntry(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void dlDataGroup(const QByteArray & dg);
    void userAppData(const RadioControlUserAppData & data);
    void audioServiceSelection(const RadioControlServiceComponent & s);
    void audioServiceReconfiguration(const RadioControlServiceComponent & s);
    void audioServiceStopped();
    void audioData(QByteArray * pData);
    void dabTime(const QDateTime & dateAndTime);
    void tuneInputDevice(uint32_t freq);
    void ensembleConfiguration(const QString &);
    void ensembleComplete(const RadioControlEnsemble & ens);
    void ensembleReconfiguration(const RadioControlEnsemble & ens);
private:
    static const uint8_t EEPCoderate[];

    dabProcHandle_t dabProcHandle;    
    dabProcSyncLevel_t sync;
    bool autoNotificationEna = false;
    uint32_t frequency;
    struct {
        uint32_t SId;
        uint8_t SCIdS;
    } serviceRequest;
    struct {
        uint32_t SId;
        uint8_t SCIdS;
    } currentService;

    // this is a counter of requests to check when the ensemble information is complete
    int requestsPending = 0;

    bool reconfigurationOngoing = false;

    RadioControlEnsemble ensemble;
    RadioControlServiceList serviceList;

    typedef RadioControlServiceCompList::iterator serviceComponentIterator;
    typedef RadioControlServiceCompList::const_iterator serviceComponentConstIterator;
    typedef RadioControlServiceList::iterator serviceIterator;
    typedef RadioControlServiceList::const_iterator serviceConstIterator;

    bool getCurrentAudioServiceComponent(serviceComponentIterator & scIt);
    bool cgetCurrentAudioServiceComponent(serviceComponentConstIterator & scIt) const;

    void updateSyncLevel(dabProcSyncLevel_t s);
    QString toShortLabel(QString & label, uint16_t charField) const;
    QString ensembleConfigurationString() const;
    void clearEnsemble();

    void emit_dabEvent(RadioControlEvent * pEvent) { emit dabEvent(pEvent); }
    void emit_audioData(QByteArray * pData) { emit audioData(pData); }

    void dabTune(uint32_t freq) { dabProcRequest_Tune(dabProcHandle, freq); }
    void dabGetEnsembleInfo() { dabProcRequest_GetEnsemble(dabProcHandle); }
    void dabGetServiceList() { dabProcRequest_GetServiceList(dabProcHandle); }
    void dabGetServiceComponent(uint32_t SId) { dabProcRequest_GetServiceComponents(dabProcHandle, SId); }
    void dabGetUserApps(uint32_t SId, uint8_t SCIdS) { dabProcRequest_GetUserAppList(dabProcHandle, SId, SCIdS); }
    void dabEnableAutoNotification() { dabProcRequest_AutoNotify(dabProcHandle, RADIO_CONTROL_NOTIFICATION_PERIOD, 0); }
    void dabServiceSelection(uint32_t SId, uint8_t SCIdS) { dabProcRequest_ServiceSelection(dabProcHandle, SId, SCIdS); }
    void dabServiceStop(uint32_t SId, uint8_t SCIdS) { dabProcRequest_ServiceStop(dabProcHandle, SId, SCIdS); }
    void dabXPadAppStart(uint8_t appType, bool start) { dabProcRequest_XPadAppStart(dabProcHandle, appType, start); }

    friend void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx);
    friend void dynamicLabelCb(dabProcDynamicLabelCBData_t * p, void * ctx);
    friend void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx);
    friend void audioDataCb(dabProcAudioCBData_t * p, void * ctx);        
private slots:
    void eventFromDab(RadioControlEvent * pEvent);
};


#endif // RADIOCONTROL_H
