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

enum class RadioControlEvent
{
    SYNC_STATUS = 0,
    TUNE,
    ENSEMBLE_INFO,
    SERVICE_LIST,
    SERVICE_COMPONENT_LIST,
    SERVICE_SELECTION,
    AUTO_NOTIFICATION,
    DATAGROUP_DL,
    DATAGROUP_MSC,
    AUDIO_DATA,
};

//typedef QList<dabProcServiceListItem_t> dabProcServiceList_t;

enum class DabSyncLevel
{
    NoSync = 0,
    NullSync = 1,
    FullSync = 2,
};

enum class DabTMId
{
    StreamAudio = 0,
    StreamData  = 1,
    PacketData  = 3
};

typedef struct
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
} radioControlEnsembleInfo_t;

Q_DECLARE_METATYPE(radioControlEnsembleInfo_t)

typedef struct
{
    // Each service component shall be uniquely identified by the combination of the
    // SId and the Service Component Identifier within the Service (SCIdS).
    int8_t SCIdS;         // Service Component Identifier within the Service (SCIdS)
    int8_t SubChId;       // SubChId (Sub-channel Identifier)
    uint16_t SubChAddr;     // address 0-863
    uint16_t SubChSize;    // subchannel size
    int8_t ps;            // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one
    int8_t lang;          // language [8.1.2] this 8-bit field shall indicate the language of the audio or data service component

    struct {
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
    } protection;
    int8_t CAflag;        // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component

    QString label;
    QString labelShort;

    int8_t numUserApps;

    // TMStreamAudio = 0,
    // TMStreamData  = 1,
    // TMPacketData  = 3
    DabTMId TMId;       // TMId (Transport Mechanism Identifier)

    union
    {
        struct  // TMId == 0
        {
            int8_t ASCTy;   // ASCTy (Audio Service Component Type)
            uint16_t bitRate;
        } streamAudio;
        struct  // TMId == 1
        {
            int8_t DSCTy;   // DSCTy (Data Service Component Type)
            uint16_t bitRate;
        } streamData;
        struct  // TMId == 3
        {
            int8_t DSCTy;         // DSCTy (Data Service Component Type)
            uint16_t SCId;         // SCId (Service Component Identifier): this 12-bit field shall uniquely
                                   // identify the service component within the ensemble
            int8_t DGflag;        // DG flag: this 1-bit flag shall indicate whether data groups are used to transport the service component as follows:
                                   //   0: data groups are used to transport the service component;
                                   //   1: data groups are not used to transport the service component.
            uint16_t packetAddress; // this 10-bit field shall define the address of the packet in which the service component is carried.
        } packetData;
    };

    bool isEEP() const { return protection.level > DabProtectionLevel::UEP_MAX; }

} radioControlServiceComponentListItem_t;
typedef QList<radioControlServiceComponentListItem_t> radioControlServiceComponentList_t;
typedef QList<radioControlServiceComponentListItem_t>::iterator radioControlServiceComponentListIterator_t;

//typedef struct {
//    union
//    {
//        uint32_t value;
//        struct {
//            union {
//                uint16_t sid;
//                struct {
//                    uint16_t ref : 12;
//                    uint16_t countryID : 4;
//                };
//            };
//            uint32_t zero : 8;
//            uint32_t ECC : 8;
//        } prog;
//        struct {
//            uint32_t ref : 20;
//            uint32_t countryID : 4;
//            uint32_t ECC : 8;
//        } data;
//    };
//    bool isProgService() { return prog.zero == 0; }
//} radioControlSId_t;

typedef struct
{
    uint32_t frequency;
    // Each service shall be identified by a Service Identifier (SId)
    // Data services use 32bit IDs, Programme services use 16 bits
    uint32_t SId;
    QString label;
    QString labelShort;        
    struct {
        uint8_t s;  // static
        uint8_t d;  // dynamic
    } pty;

    // CAId (Conditional Access Identifier): this 3-bit field shall identify the
    // Access Control System (ACS) used for the service
    uint8_t CAId;

    uint8_t numServiceComponents;
    radioControlServiceComponentList_t serviceComponents;

} radioControlServiceListItem_t;

typedef QList<radioControlServiceListItem_t> radioControlServiceList_t;
typedef QList<radioControlServiceListItem_t>::iterator radioControlServiceListIterator_t;

typedef struct
{
    uint32_t SId;
    QList<dabProcServiceCompListItem_t> list;
} radioControlServiceComponentData_t;

typedef struct {
    QString label;
    uint16_t SId;
    uint8_t SCIdS;
} radioControlAudioService_t;

typedef struct Event
{
    RadioControlEvent type;
    dabProcNotificationStatus_t status;
    intptr_t pData;
} radioControlEvent_t;

class RadioControl : public QObject
{
    Q_OBJECT
public:
    explicit RadioControl(QObject *parent = nullptr);    
    ~RadioControl();
    static bool isAudioService(uint32_t sid) { return sid <= 0x00FFFFFF; }

public slots:
    bool init();
    void tune(uint32_t freq);
    void start(uint32_t freq);
    void exit();
    void tuneService(uint32_t freq, uint32_t SId, uint8_t SCIdS);

signals:
    void dabEvent(radioControlEvent_t * pEvent);
    void syncStatus(uint8_t sync);
    void snrLevel(float snr);
    void tuneDone(uint32_t freq);
    void ensembleInformation(const radioControlEnsembleInfo_t & ens);
    void serviceListItemAvailable(const radioControlServiceListItem_t & serviceListItem);
    //void serviceListItemAvailable(const radioControlAudioService_t & audioService);
    void dlDataGroup(const QByteArray & dg);
    void mscDataGroup(const QByteArray & dg);
    void newService(uint32_t SId, uint8_t SCIdS);
    void newAudioService(const DabAudioMode & m);
    void audioData(QByteArray * pData);
    void dabTime(const QDateTime & dateAndTime);
    void tuneInputDevice(uint32_t freq);

private:
    static const uint8_t EEPCoderate[];

    dabProcHandle_t dabProcHandle;    
    dabProcSyncLevel_t sync;
    bool autoNotificationEna = false;
    uint32_t frequency;
    radioControlEnsembleInfo_t ensemble;
    radioControlServiceList_t serviceList;
    radioControlServiceListIterator_t findService(uint32_t SId);
    radioControlServiceComponentListIterator_t findServiceComponent(const radioControlServiceListIterator_t & sIt, uint8_t SCIdS);

    void updateSyncLevel(dabProcSyncLevel_t s);
    QString toShortLabel(QString & label, uint16_t charField) const;

    void emit_dabEvent(radioControlEvent_t * pEvent) { emit dabEvent(pEvent); }
    void emit_audioData(QByteArray * pData) { emit audioData(pData); }

    void dabTune(uint32_t freq) { dabProcRequest_Tune(dabProcHandle, freq); }
    void dabGetEnsembleInfo() { dabProcRequest_GetEnsemble(dabProcHandle); }
    void dabGetServiceList() { dabProcRequest_GetServiceList(dabProcHandle); }
    void dabGetServiceComponent(uint32_t SId) { dabProcRequest_GetServiceComponents(dabProcHandle, SId); }
    void dabEnableAutoNotification() { dabProcRequest_AutoNotify(dabProcHandle, RADIO_CONTROL_NOTIFICATION_PERIOD, 0); }
    void dabServiceSelection(uint32_t SId, uint8_t SCIdS) { dabProcRequest_ServiceSelection(dabProcHandle, SId, SCIdS); }

    friend void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx);
    friend void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx);
    friend void audioDataCb(dabProcAudioCBData_t * p, void * ctx);        
private slots:
    void eventFromDab(radioControlEvent_t * pEvent);
};


#endif // RADIOCONTROL_H
