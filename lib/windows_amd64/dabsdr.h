#ifndef DABSDR_H
#define DABSDR_H

#include <stdint.h>
#include <stdlib.h>
#include "dabsdr_export.h"

#if defined(_MSC_VER)
  #include <complex.h>
  #undef I
#else
  #define _Fcomplex float _Complex
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dabsdr_s * dabsdrHandle_t;

typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t flags;
} dabsdrVersion_t;

typedef union
{
    uint8_t raw;
    uint8_t mp2DRC;
    struct
    {
        uint8_t mpeg_surr_cfg : 3;  // [A decoder that does not support MPEG Surround shall ignore this parameter.]
        uint8_t ps_flag : 1;
        uint8_t aac_channel_mode : 1;
        uint8_t sbr_flag : 1;
        uint8_t dac_rate : 1;
        uint8_t conceal : 1;        // concealment requested, means that fram is not valid, if decoder does not support, it should be discarded
    } bits;
} dabsdrAudioFrameHeader_t;

// callback data types
// audio callback
typedef struct
{
   dabsdrAudioFrameHeader_t header;
   uint16_t auLen;
   const uint8_t * pAuData;
} dabsdrAudioCBData_t;

// dynamic label (DL) callback
typedef struct
{
   uint16_t len;
   const uint8_t * pData;
} dabsdrDynamicLabelCBData_t;

// data group callback
typedef struct
{
   int16_t userAppType;
   uint16_t dgLen;
   const uint8_t * pDgData;
} dabsdrDataGroupCBData_t;

typedef enum dabsdrNotificationId_e
{
    DABSDR_NID_SYNC_STATUS = 1,
    DABSDR_NID_TUNE,
    DABSDR_NID_ENSEMBLE_INFO,
    DABSDR_NID_SERVICE_LIST,
    DABSDR_NID_SERVICE_COMPONENT_LIST,
    DABSDR_NID_USER_APP_LIST,
    DABSDR_NID_SERVICE_SELECTION,
    DABSDR_NID_SERVICE_STOP,
    DABSDR_NID_PERIODIC,
    DABSDR_NID_XPAD_APP_START_STOP,
    DABSDR_NID_RECONFIGURATION,
    DABSDR_NID_RESET,
} dabsdrNotificationId_t;

typedef enum dabsdrNotificationStatus_e
{
    DABSDR_NSTAT_SUCCESS = 0,
    DABSDR_NSTAT_GENERIC_ERROR = 1,
    DABSDR_NSTAT_SERVICE_NOT_FOUND,
    DABSDR_NSTAT_SERVICE_NOT_READY,
    DABSDR_NSTAT_SERVICE_NOT_SUPPORTED,
} dabsdrNotificationStatus_t;

typedef struct
{
   dabsdrNotificationId_t nid;
   dabsdrNotificationStatus_t status;
   uint16_t len;
   const void * pData;
} dabsdrNotificationCBData_t;


// notifications typedefs
typedef enum
{
    DABSDR_SYNC_LEVEL_NO_SYNC = 0b00,
    DABSDR_SYNC_LEVEL_ON_NULL = 0b01,
    DABSDR_SYNC_LEVEL_FIC     = 0b11,
} dabsdrSyncLevel_t;

typedef struct
{
    dabsdrSyncLevel_t syncLevel;
} dabsdrNtfSyncStatus_t;

typedef struct
{
    dabsdrSyncLevel_t syncLevel;  // DAB synchronization level
    int32_t freqOffset;            // frequency offset * 10
    uint32_t dateHoursMinutes;     // DAB time date + hours + minutes
    uint16_t secMsec;              // DAB time sec + msec
    int16_t snr10;                 // 10*SNR
    uint16_t fibErrorCntr;         // number of FIB CRC errors during notification period
    uint8_t mscCrcOkCntr;          // number of correct CRC [after RS (DAB+)]
    uint8_t mscCrcErrorCntr;       // number of CRC errors [after RS (DAB+)]
} dabsdrNtfPeriodic_t;

#define DAB_LABEL_MAX_LENGTH   (16)
typedef struct
{
    char str[DAB_LABEL_MAX_LENGTH + 1];  // label is zero terminated C string
    uint16_t charField;
    uint8_t charset;
} dabsdrLabel_t;

typedef struct
{
    // Each service shall be identified by a Service Identifier (SId)
    // Data services use 32bit IDs, Programme services use 16 bits
    uint32_t sid;
    dabsdrLabel_t label;
    struct {
        uint8_t s;  // static
        uint8_t d;  // dynamic
    } pty;

    // CAId (Conditional Access Identifier): this 3-bit field shall identify the
    // Access Control System (ACS) used for the service
    uint8_t CAId;

    uint8_t numComponents;
} dabsdrServiceListItem_t;


typedef struct
{
    // Each service component shall be uniquely identified by the combination of the
    // SId and the Service Component Identifier within the Service (SCIdS).
    uint32_t SId;
    uint8_t SCIdS;         // Service Component Identifier within the Service (SCIdS)
    uint8_t SubChId;       // SubChId (Sub-channel Identifier)
    int16_t SubChAddr;     // address 0-863
    uint16_t SubChSize;    // subchannel size
    uint8_t protectionLevel;
    union
    {
        uint8_t uepIdx;
        uint8_t fecScheme;
    };
    uint8_t ps;            // P/S (Primary/Secondary): this 1-bit flag shall indicate whether the service component is the primary one
    uint8_t lang;          // language [8.1.2] this 8-bit field shall indicate the language of the audio or data service component
    uint8_t CAflag;        // CA flag: this 1-bit field flag shall indicate whether access control applies to the service component

    dabsdrLabel_t label;

    uint8_t numUserApps;

    // TMStreamAudio = 0,
    // TMStreamData  = 1,
    // TMPacketData  = 3
    uint8_t TMId;       // TMId (Transport Mechanism Identifier)

    union
    {
        struct  // TMId == 0
        {
            uint8_t ASCTy;   // ASCTy (Audio Service Component Type)
            uint16_t bitRate;
        } streamAudio;
        struct  // TMId == 1
        {
            uint8_t DSCTy;   // DSCTy (Data Service Component Type)
            uint16_t bitRate;
        } streamData;
        struct  // TMId == 3
        {
            uint8_t DSCTy;         // DSCTy (Data Service Component Type)
            uint16_t SCId;         // SCId (Service Component Identifier): this 12-bit field shall uniquely
                                   // identify the service component within the ensemble
            uint8_t DGflag;        // DG flag: this 1-bit flag shall indicate whether data groups are used to transport the service component as follows:
                                   //   0: data groups are used to transport the service component;
                                   //   1: data groups are not used to transport the service component.
            int16_t packetAddress; // this 10-bit field shall define the address of the packet in which the service component is carried.
        } packetData;
    };
} dabsdrServiceCompListItem_t;

typedef struct
{
    // Service identification
    uint32_t SId;
    uint8_t SCIdS;         // Service Component Identifier within the Service (SCIdS)

    // User Application Type: this 11-bit field identifies the user application that shall be used to decode the data in the
    // channel identified by SId and SCIdS. The interpretation of this field shall be as defined in ETSI TS 101 756 [3],
    // table 16.
    uint16_t type;

    // User Application Label
    dabsdrLabel_t label;

    // User application data
    uint8_t dataLen;    // 0 - 23
    uint8_t data[23];   // raw data
} dabsdrUserAppListItem_t;

typedef struct
{
    uint8_t numServices;
    int (*getServiceListItem)(dabsdrHandle_t handle, uint8_t idx, dabsdrServiceListItem_t * pServiceListItem);
} dabsdrNtfServiceList_t;

typedef struct
{
    uint32_t sid;
    uint8_t numServiceComponents;
    int (*getServiceComponentListItem)(dabsdrHandle_t handle, uint8_t scIdx, dabsdrServiceCompListItem_t * pServiceCompListItem);
} dabsdrNtfServiceComponentList_t;

typedef struct
{
    uint32_t SId;
    uint8_t SCIdS;  // Service Component Identifier within the Service (SCIdS)
    uint8_t numUserApps;
    int (*getUserAppListItem)(dabsdrHandle_t handle, uint8_t uaIdx, dabsdrUserAppListItem_t * pUserAppListItem);
} dabsdrNtfUserAppList_t;

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
    uint8_t intTable; // Inter. (International) Table Id: this 8-bit field shall be used to select an international table
    uint8_t alarm;
    dabsdrLabel_t label;
} dabsdrNtfEnsemble_t;

typedef struct
{
    uint32_t SId;
    uint8_t SCIdS;
} dabsdrNtfServiceSelection_t;

typedef enum dabsdrNtfResetFlags_e
{
    DABSDR_RESET_INIT = 0,
    DABSDR_RESET_NEW_EID = 1
} dabsdrNtfResetFlags_t;

typedef dabsdrNtfServiceSelection_t dabsdrNtfServiceStop_t;

typedef struct
{
    uint8_t appType;
    uint8_t start;
} dabsdrXpadAppStartStop_t;

// input functions
typedef void (*dabsdrInputFunc_t)(_Fcomplex *, uint16_t);

// callback types
typedef void (*dabsdrAudioCBFunc_t)(dabsdrAudioCBData_t * p, void * ctx);
typedef void (*dabsdrDynamicLabelCBFunc_t)(dabsdrDynamicLabelCBData_t * p, void * ctx);
typedef void (*dabsdrDataGroupCBFunc_t)(dabsdrDataGroupCBData_t * p, void * ctx);
typedef void (*dabsdrNotificationCBFunc_t)(dabsdrNotificationCBData_t * p, void * ctx);


DABSDR_API void dabsdr(dabsdrHandle_t handle);
DABSDR_API uint8_t dabsdrInit(dabsdrHandle_t *handle);
DABSDR_API void dabsdrGetVersion(dabsdrVersion_t * version);

// dabsdrRequest_Exit(dabsdrHandle_t handle) should be called before
DABSDR_API void dabsdrDeinit(dabsdrHandle_t *handle);

// input function registration
DABSDR_API void dabsdrRegisterInputFcn(dabsdrHandle_t handle, dabsdrInputFunc_t fcn);
DABSDR_API void dabsdrRegisterDummyInputFcn(dabsdrHandle_t handle, dabsdrInputFunc_t fcn);

// callback register functions
DABSDR_API void dabsdrRegisterAudioCb(dabsdrHandle_t handle, dabsdrAudioCBFunc_t fcn, void * ctx);
DABSDR_API void dabsdrRegisterDynamicLabelCb(dabsdrHandle_t handle, dabsdrDynamicLabelCBFunc_t fcn, void * ctx);
DABSDR_API void dabsdrRegisterDataGroupCb(dabsdrHandle_t handle, dabsdrDataGroupCBFunc_t fcn, void * ctx);
DABSDR_API void dabsdrRegisterNotificationCb(dabsdrHandle_t handle, dabsdrNotificationCBFunc_t fcn, void * ctx);


// Request functions
DABSDR_API void dabsdrRequest_Tune(dabsdrHandle_t handle, uint32_t frequency);
DABSDR_API void dabsdrRequest_GetEnsemble(dabsdrHandle_t handle);
DABSDR_API void dabsdrRequest_GetServiceList(dabsdrHandle_t handle);
DABSDR_API void dabsdrRequest_GetServiceComponents(dabsdrHandle_t handle, uint32_t SId);
DABSDR_API void dabsdrRequest_GetUserAppList(dabsdrHandle_t handle, uint32_t SId, uint8_t SCIdS);
DABSDR_API void dabsdrRequest_ServiceSelection(dabsdrHandle_t handle, uint32_t SId, uint8_t SCIdS);
DABSDR_API void dabsdrRequest_ServiceStop(dabsdrHandle_t handle, uint32_t SId, uint8_t SCIdS);
DABSDR_API void dabsdrRequest_XPadAppStart(dabsdrHandle_t handle, uint8_t appType, int8_t startRequest);
DABSDR_API void dabsdrRequest_SetPeriodicNotify(dabsdrHandle_t handle, uint8_t period, uint32_t cfg);
DABSDR_API void dabsdrRequest_Exit(dabsdrHandle_t handle);


#ifdef __cplusplus
}
#endif

#endif // DABSDR_H
