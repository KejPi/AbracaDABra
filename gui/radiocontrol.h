#ifndef RADIOCONTROL_H
#define RADIOCONTROL_H

#include <QObject>
#include <QMap>
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

enum class DabSyncLevel
{
    NoSync = 0,
    NullSync = 1,
    FullSync = 2,
};

struct DabSId {
    union {
        uint32_t value;
        struct {
            uint32_t countryServiceRef : 16;
            uint32_t ecc : 8;
            uint32_t zero : 8;
        } prog;
        struct {
            uint32_t countryServiceRef : 24;
            uint32_t ecc : 8;
        } data;
    };   
    DabSId(uint32_t sid = 0) { value = sid; }
    bool isProgServiceId() const { return prog.zero == 0; }
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
    union
    {
        uint32_t ueid;
        struct
        {
            uint32_t eid : 16;
            uint32_t ecc : 8;
        };
    };
    int8_t LTO;       // Ensemble LTO (Local Time Offset): Local Time Offset (LTO) for the ensemble.
                      // It is expressed in multiples of half hours in the range -15,5 hours to +15,5 hours
    uint8_t intTable;  // Inter. (International) Table Id: this 8-bit field shall be used to select an international table
    uint8_t alarm;
    QString label;
    QString labelShort;
};
Q_DECLARE_METATYPE(RadioControlEnsemble)

struct RadioControlUserApp
{
    QString label;        // Service label
    QString labelShort;   // Short label
    uint16_t uaType;      // User application type
    union
    {
        uint16_t value;
        struct             // X-PAD data for audio components
        {
            uint16_t DScTy : 6;
            uint16_t rfu2 : 1;
            uint16_t dgFlag : 1;
            uint16_t xpadAppTy : 5;
            uint16_t rfu1 : 1;
            uint16_t CAOrgFlag : 1;
            uint16_t CAflag : 1;
        } bits;
    } xpadData;
    QVector<uint8_t> uaData;  // optional user application data
};

struct RadioControlServiceComponent
{
    // Each service component shall be uniquely identified by the combination of the
    // SId and the Service Component Identifier within the Service (SCIdS).
    DabSId SId;
    int8_t SCIdS;         // Service Component Identifier within the Service (SCIdS)
    int8_t SubChId;       // SubChId (Sub-channel Identifier)
    uint16_t SubChAddr;   // address 0-863
    uint16_t SubChSize;   // subchannel size
    int8_t ps;            // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one
    int8_t lang;          // language [8.1.2] this 8-bit field shall indicate the language of the audio or data service component
    DabPTy pty;           // Static and dynamic programme type [8.1.5]
    uint8_t CAId;         // CAId (Conditional Access Identifier): this 3-bit field shall identify the
                          // Access Control System (ACS) used for the service
    int8_t CAflag;        // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component

    //int8_t numUserApps;   // Number of user applications
    QList<RadioControlUserApp> userApps;

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
            int8_t DGflag;          // DG flag: this 1-bit flag shall indicate whether data groups are used to transport the service component as follows:
                                    //   0: data groups are used to transport the service component;
                                    //   1: data groups are not used to transport the service component.
            uint16_t packetAddress; // this 10-bit field shall define the address of the packet in which the service component is carried.
        } packetData;
    };
    bool isAudioService() const { return DabTMId::StreamAudio == TMId; }
    bool isDataStreamService() const { return DabTMId::StreamData == TMId; }
    bool isDataPacketService() const { return DabTMId::PacketData == TMId; }
};
Q_DECLARE_METATYPE(RadioControlServiceComponent)

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

    QList<RadioControlServiceComponent> serviceComponents;
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
    AUTO_NOTIFICATION,
    DATAGROUP_DL,
    DATAGROUP_MSC,
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

public slots:
    bool init();
    void start(uint32_t freq);
    void exit();
    void tuneService(uint32_t freq, uint32_t SId, uint8_t SCIdS);
    void getEnsembleConfiguration();

signals:
    void dabEvent(RadioControlEvent * pEvent);
    void syncStatus(uint8_t sync);
    void snrLevel(float snr);
    void freqOffset(float f);
    void fibCounter(int expected, int errors);
    void tuneDone(uint32_t freq);
    void ensembleInformation(const RadioControlEnsemble & ens);
    void serviceListEntry(const RadioControlEnsemble & ens, const RadioControlServiceComponent & slEntry);
    void dlDataGroup(const QByteArray & dg);
    void mscDataGroup(const QByteArray & dg);
    void serviceChanged();
    void newServiceSelection(const RadioControlServiceComponent & s);
    void audioData(QByteArray * pData);
    void dabTime(const QDateTime & dateAndTime);
    void tuneInputDevice(uint32_t freq);
    void ensembleConfiguration(const QString &);

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

    // this is a counter of requests to check when the ensemble information is complete
    int requestsPending = 0;

    RadioControlEnsemble ensemble;
    QList<RadioControlService> serviceList;
    QList<RadioControlService>::iterator findService(DabSId SId);
    QList<RadioControlServiceComponent>::iterator findServiceComponent(const QList<RadioControlService>::iterator & sIt, uint8_t SCIdS);

    void updateSyncLevel(dabProcSyncLevel_t s);
    QString toShortLabel(QString & label, uint16_t charField) const;
    QString ensembleConfigurationString() const;

    void emit_dabEvent(RadioControlEvent * pEvent) { emit dabEvent(pEvent); }
    void emit_audioData(QByteArray * pData) { emit audioData(pData); }

    void dabTune(uint32_t freq) { dabProcRequest_Tune(dabProcHandle, freq); }
    void dabGetEnsembleInfo() { dabProcRequest_GetEnsemble(dabProcHandle); }
    void dabGetServiceList() { dabProcRequest_GetServiceList(dabProcHandle); }
    void dabGetServiceComponent(uint32_t SId) { dabProcRequest_GetServiceComponents(dabProcHandle, SId); }
    void dabGetUserApps(uint32_t SId, uint8_t SCIdS) { dabProcRequest_GetUserAppList(dabProcHandle, SId, SCIdS); }
    void dabEnableAutoNotification() { dabProcRequest_AutoNotify(dabProcHandle, RADIO_CONTROL_NOTIFICATION_PERIOD, 0); }
    void dabServiceSelection(uint32_t SId, uint8_t SCIdS) { dabProcRequest_ServiceSelection(dabProcHandle, SId, SCIdS); }

    friend void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx);
    friend void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx);
    friend void audioDataCb(dabProcAudioCBData_t * p, void * ctx);        
private slots:
    void eventFromDab(RadioControlEvent * pEvent);
};


#endif // RADIOCONTROL_H
