#include <QThread>
#include <QDate>
#include <QTime>

#include <QDebug>
#include <QIODevice>
#include "radiocontrol.h"
#include "inputdevice.h"

const uint8_t RadioControl::EEPCoderate[] =
{ // ETSI EN 300 401 V2.1.1 [6.2.1 Basic sub-channel organization] table 9 & 10
    0x14, 0x38, 0x12, 0x34,   // EEP 1-A..4-A : 1/4 3/8 1/2 3/4
    0x49, 0x47, 0x46, 0x45    // EEP 1-B..4-B : 4/9 4/7 4/6 4/5
};


void dabNotificationCb(dabsdrNotificationCBData_t * p, void * ctx);
void dynamicLabelCb(dabsdrDynamicLabelCBData_t * p, void * ctx);
void dataGroupCb(dabsdrDataGroupCBData_t * p, void * ctx);
void audioDataCb(dabsdrAudioCBData_t * p, void * ctx);

RadioControl::RadioControl(QObject *parent) : QObject(parent)
{
    m_dabsdrHandle = nullptr;
    m_frequency = 0;
    m_serviceList.clear();
    m_serviceRequest.SId = m_serviceRequest.SCIdS = 0;

    connect(this, &RadioControl::dabEvent, this, &RadioControl::onDabEvent, Qt::QueuedConnection);
}

RadioControl::~RadioControl()
{   // this cancels dabsdr thread
    dabsdrDeinit(&m_dabsdrHandle);
}

// returns false if not successfull
bool RadioControl::init()
{
    if (EXIT_SUCCESS == dabsdrInit(&m_dabsdrHandle))
    {
        dabsdrRegisterInputFcn(m_dabsdrHandle, getSamples);
        dabsdrRegisterDummyInputFcn(m_dabsdrHandle, skipSamples);
        dabsdrRegisterNotificationCb(m_dabsdrHandle, dabNotificationCb, (void *) this);
        dabsdrRegisterDynamicLabelCb(m_dabsdrHandle, dynamicLabelCb, (void*) this);
        dabsdrRegisterDataGroupCb(m_dabsdrHandle, dataGroupCb, (void*) this);
        dabsdrRegisterAudioCb(m_dabsdrHandle, audioDataCb, this);
        // this starts dab processing thread and retunrs
        dabsdr(m_dabsdrHandle);
    }
    else
    {
        qDebug() << "DAB processing init failed";
        m_dabsdrHandle = nullptr;

        return false;
    }

    return true;
}

void RadioControl::onDabEvent(RadioControlEvent * pEvent)
{
    switch (pEvent->type)
    {
    case RadioControlEventType::AUDIO_DATA:
        break;
    case RadioControlEventType::RESET:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEventType::RESET";
#endif
        dabsdrNtfResetFlags_t flags = static_cast<dabsdrNtfResetFlags_t>(pEvent->pData);
        switch (flags)
        {
        case DABSDR_RESET_INIT:
            m_serviceList.clear();
            clearEnsemble();
            break;
        case DABSDR_RESET_NEW_EID:
            emit ensembleRemoved(m_ensemble);
            start(m_frequency);

            break;
        }
    }
        break;
    case RadioControlEventType::SYNC_STATUS:
    {
        dabsdrSyncLevel_t s = static_cast<dabsdrSyncLevel_t>(pEvent->pData);
#if RADIO_CONTROL_VERBOSE
        qDebug("Sync = %d", s);
#endif

        if ((DABSDR_SYNC_LEVEL_FIC == s) && (RADIO_CONTROL_UEID_INVALID == m_ensemble.ueid))
        { // ask for ensemble info if not available
            dabGetEnsembleInfo();
        }
        updateSyncLevel(s);
    }
        break;
    case RadioControlEventType::TUNE:
    {
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
            uint32_t freq = static_cast<uint32_t>(pEvent->pData);
#if RADIO_CONTROL_VERBOSE > 1
            qDebug() << "Tune success" << freq;
#endif
            if ((freq != m_frequency) || (0 == m_frequency))
            {   // in first step, DAB is tuned to 0 that is != desired frequency
                // or if we want to tune to 0 by purpose (tgo to IDLE)
                // this tunes input device to desired frequency
                emit tuneInputDevice(m_frequency);
            }
            else
            {   // tune is finished , notify HMI
                emit tuneDone(freq);

                // this is to request autontf when EID changes
                m_enaAutoNotification = false;
            }
        }
        else
        {
            qDebug() << "Tune error" << pEvent->status;
        }
    }
        break;
    case RadioControlEventType::ENSEMBLE_INFO:
    {
        // process ensemble info
        dabsdrNtfEnsemble_t * pInfo = reinterpret_cast<dabsdrNtfEnsemble_t *>(pEvent->pData);

#if RADIO_CONTROL_VERBOSE
        qDebug("RadioControlEvent::ENSEMBLE_INFO 0x%8.8X", pInfo->ueid);
#endif

#if (RADIO_CONTROL_TEST_MODE)
        if (pInfo->label.str[0] != '\0')  // allow ECC == 0 in test mode
#else
        if (((pInfo->ueid & 0x00FF0000) != 0) && (pInfo->label.str[0] != '\0'))
#endif
        {
            //serviceList.clear();
            m_ensemble.ueid = pInfo->ueid;
            m_ensemble.frequency = pInfo->frequency;
            m_ensemble.LTO = pInfo->LTO;
            m_ensemble.intTable = pInfo->intTable;
            m_ensemble.alarm = pInfo->alarm;
            m_ensemble.label = DabTables::convertToQString(pInfo->label.str, pInfo->label.charset).trimmed();
            m_ensemble.labelShort = toShortLabel(m_ensemble.label, pInfo->label.charField).trimmed();
            emit ensembleInformation(m_ensemble);

            // request service list            
            // ETSI EN 300 401 V2.1.1 (2017-01) [6.1]
            // The complete MCI for one configuration shall normally be signalled in a 96ms period;
            // the exceptions are that the FIG 0/8 for primary service components containing data applications and for data secondary service components,
            // and the FIG 0/13 may be signalled at a slower rate but not less frequently than once per second.
            // When the slower rate is used, the FIG 0/8 and FIG 0/13 for the same service component should be signalled in the FIBs corresponding to the same CIF.
            QTimer::singleShot(200, this, &RadioControl::dabGetServiceList);
        }

        delete (dabsdrNtfEnsemble_t *) pEvent->pData;

        if ((RADIO_CONTROL_UEID_INVALID == m_ensemble.ueid) && (DABSDR_SYNC_LEVEL_FIC == m_syncLevel))
        {   // valid ensemble sill not received
            // wait some time and send new request
            QTimer::singleShot(200, this, &RadioControl::dabGetEnsembleInfo);
        }
    }
        break;
    case RadioControlEventType::RECONFIGURATION:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEventType::RECONFIGURATION";
#endif
        m_isReconfigurationOngoing = true;

        emit ensembleReconfiguration(m_ensemble);

        m_serviceList.clear();

        // request service list
        // ETSI EN 300 401 V2.1.1 (2017-01) [6.1]
        QTimer::singleShot(10, this, &RadioControl::dabGetServiceList);
    }
        break;

    case RadioControlEventType::SERVICE_LIST:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEvent::SERVICE_LIST";
#endif
        QList<dabsdrServiceListItem_t> * pServiceList = reinterpret_cast<QList<dabsdrServiceListItem_t> *>(pEvent->pData);
        if (0 == pServiceList->size())
        {   // no service list received (invalid probably)
            m_serviceList.clear();

            // send new request after some timeout
            QTimer::singleShot(100, this, &RadioControl::dabGetServiceList);
        }
        else
        {
            m_numRequestsPending = 0;
            for (auto const & dabService : *pServiceList)
            {
                DabSId sid(dabService.sid, m_ensemble.ecc());
                serviceConstIterator servIt = m_serviceList.constFind(sid.value());
                if (servIt != m_serviceList.cend())
                {   // delete existing service
                    m_serviceList.erase(servIt);
                }
                RadioControlService newService;
                newService.SId = sid;
                newService.label = DabTables::convertToQString(dabService.label.str, dabService.label.charset).trimmed();
                newService.labelShort = toShortLabel(newService.label, dabService.label.charField).trimmed();
                newService.pty.s = dabService.pty.s;
                newService.pty.d = dabService.pty.d;
                newService.CAId = dabService.CAId;
                m_serviceList.insert(sid.value(), newService);
                m_numRequestsPending++;
                dabGetServiceComponent(sid.value());
            }
        }
        delete pServiceList;
    }
        break;
    case RadioControlEventType::SERVICE_COMPONENT_LIST:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEvent::SERVICE_COMPONENT_LIST";
#endif
        QList<dabsdrServiceCompListItem_t> * pList = reinterpret_cast<QList<dabsdrServiceCompListItem_t> *>(pEvent->pData);
        if (!pList->isEmpty() && m_ensemble.isValid())
        {   // all service components belong to the same SId, reading sid from the first            
            DabSId sid(pList->at(0).SId, m_ensemble.ecc());
#if RADIO_CONTROL_VERBOSE
            qDebug("RadioControlEvent::SERVICE_COMPONENT_LIST %8.8X", sid.value());
#endif

            // find service ID
            serviceIterator serviceIt = m_serviceList.find(sid.value());
            if (serviceIt != m_serviceList.end())
            {   // SId found
                serviceIt->serviceComponents.clear();

                bool requestUpdate = false;
                // ETSI EN 300 401 V2.1.1 (2017-01) [8.1.1]
                // The type 1 and 2 FIGs, which define the various labels, are also in the unique information category.
                // They shall be signalled for all services and service components that require selection by a user.
                // ==> audio components
                for (auto const & dabServiceComp : *pList)
                {
                    // first checking validity of data
                    if ((dabServiceComp.SubChAddr < 0)
                       || ((DabTMId::StreamAudio == DabTMId(dabServiceComp.TMId)) && (!dabServiceComp.ps) && (dabServiceComp.label.str[0] == '\0'))
                       || ((DabTMId::PacketData == DabTMId(dabServiceComp.TMId)) && (dabServiceComp.packetData.packetAddress < 0))
#if !(RADIO_CONTROL_TEST_MODE)  // data service without user application is allowed in test mode, timeout coud be implemented insted
                       || ((DabTMId::StreamAudio != DabTMId(dabServiceComp.TMId)) && (dabServiceComp.numUserApps <= 0))  // user app for data services
#endif
                        )
                    {
                        requestUpdate = true;
                        break;
                    }

                    RadioControlServiceComponent newServiceComp;
                    newServiceComp.SId = serviceIt->SId;
                    newServiceComp.SCIdS = dabServiceComp.SCIdS;
                    newServiceComp.SubChId = dabServiceComp.SubChId;
                    newServiceComp.SubChAddr = dabServiceComp.SubChAddr;
                    newServiceComp.SubChSize = dabServiceComp.SubChSize;
                    newServiceComp.CAflag = (0 != dabServiceComp.CAflag);
                    newServiceComp.CAId = serviceIt->CAId;
                    newServiceComp.lang = dabServiceComp.lang;
                    newServiceComp.pty = serviceIt->pty;
                    newServiceComp.protection.level = DabProtectionLevel(dabServiceComp.protectionLevel);
                    if (DabProtectionLevel::UEP_MAX < newServiceComp.protection.level)
                    {  // eep protection - store coderate
                        newServiceComp.protection.codeRateFecValue = EEPCoderate[int(newServiceComp.protection.level)-int(DabProtectionLevel::EEP_MIN)];
                        newServiceComp.protection.fecScheme = (dabServiceComp.fecScheme != 0);
                    }
                    else
                    {
                        newServiceComp.protection.uepIndex = dabServiceComp.uepIdx;       // UEP index or FEC scheme flag (union)
                    }

                    //newServiceComp.numUserApps = dabServiceComp.numUserApps;
                    newServiceComp.ps = dabServiceComp.ps;
                    if (dabServiceComp.ps)
                    {  // primary: [8.1.14.3] The service component label shall not be signalled for primary service component
                        newServiceComp.label = serviceIt->label;
                        newServiceComp.labelShort = serviceIt->labelShort;
                    }
                    else
                    {
                        newServiceComp.label = DabTables::convertToQString(dabServiceComp.label.str, dabServiceComp.label.charset).trimmed();
                        newServiceComp.labelShort = toShortLabel(newServiceComp.label, dabServiceComp.label.charField).trimmed();
                    }

                    newServiceComp.autoEnabled = false;
                    newServiceComp.TMId = DabTMId(dabServiceComp.TMId);
                    switch (newServiceComp.TMId)
                    {
                    case DabTMId::StreamAudio:
                        newServiceComp.streamAudioData.scType = DabAudioDataSCty(dabServiceComp.streamAudio.ASCTy);
                        newServiceComp.streamAudioData.bitRate = dabServiceComp.streamAudio.bitRate;
                        break;
                    case DabTMId::StreamData:
                        newServiceComp.streamAudioData.scType = DabAudioDataSCty(dabServiceComp.streamData.DSCTy);
                        newServiceComp.streamAudioData.bitRate = dabServiceComp.streamData.bitRate;
                        break;
                    case DabTMId::PacketData:
                        newServiceComp.packetData.DGflag = (0 != dabServiceComp.packetData.DGflag);
                        newServiceComp.packetData.DSCTy = DabAudioDataSCty(dabServiceComp.packetData.DSCTy);
                        newServiceComp.packetData.SCId = dabServiceComp.packetData.SCId;
                        newServiceComp.packetData.packetAddress = dabServiceComp.packetData.packetAddress;
                        break;
                    }
                    serviceIt->serviceComponents.insert(newServiceComp.SCIdS, newServiceComp);

                    if (m_isReconfigurationOngoing)
                    {
                        if (((m_currentService.SId == serviceIt->SId.value()) && (m_currentService.SCIdS == newServiceComp.SCIdS)
                             && (newServiceComp.isAudioService())))
                        {   // inform HMI about possible new service configuration
                            emit audioServiceReconfiguration(newServiceComp);
                        }
                    }
                    else
                    {
                        if ((m_serviceRequest.SId == serviceIt->SId.value()) && (m_serviceRequest.SCIdS == newServiceComp.SCIdS))
                        {
                            dabServiceSelection(m_serviceRequest.SId, m_serviceRequest.SCIdS);
                            m_serviceRequest.SId = 0;    // clear request
                        }
                    }

                    emit serviceListEntry(m_ensemble, newServiceComp);
                }
                if (requestUpdate)
                {
                    serviceIt->serviceComponents.clear();
                    dabGetServiceComponent(sid.value());
                }
                else
                {  // service list item information is complete
                   m_numRequestsPending--;
                   for (auto & serviceComp : serviceIt->serviceComponents)
                   {
                       serviceComp.userApps.clear();
                       m_numRequestsPending++;
                       dabGetUserApps(serviceIt->SId.value(), serviceComp.SCIdS);
                   }
                }
            }
            else
            {  // SId not found
                qDebug("Service SID %8.8X not in service list", sid.value());
            }
        }

        delete pList;
    }
        break;
    case RadioControlEventType::USER_APP_LIST:
    {
        QList<dabsdrUserAppListItem_t> * pList = reinterpret_cast<QList<dabsdrUserAppListItem_t> *>(pEvent->pData);
        if (!pList->isEmpty())
        {   // all user apps belong to the same SId, reading sid from the first
            DabSId sid(pList->at(0).SId, m_ensemble.ecc());

            //qDebug("RadioControlEventType::USER_APP_LIST SID %6.6X : %d requestPending = %d", pList->at(0).SId, pList->at(0).SCIdS, requestsPending);

            // find service ID
            serviceIterator serviceIt = m_serviceList.find(sid.value());
            if (serviceIt != m_serviceList.end())
            {   // SId found
                // all user apps belong to the same SId+SCIdS, reading SCIdS from the first
                serviceComponentIterator scIt = serviceIt->serviceComponents.find(pList->at(0).SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service component found
                    scIt->userApps.clear();
                    for (auto const & userApp : *pList)
                    {
                        RadioControlUserApp newUserApp;
                        newUserApp.label = DabTables::convertToQString(userApp.label.str, userApp.label.charset).trimmed();
                        newUserApp.labelShort = toShortLabel(newUserApp.label, userApp.label.charField).trimmed();
                        newUserApp.uaType = DabUserApplicationType(userApp.type);
                        for (int n = 0; n < userApp.dataLen; ++n)
                        {
                            newUserApp.uaData.append(userApp.data[n]);
                        }
                        if ((scIt->isAudioService()) && (userApp.dataLen >= 2))
                        {
                            //newUserApp.xpadData.value = (userApp.data[0] << 8) | userApp.data[1];
                            newUserApp.xpadData.CAflag = (0 != (userApp.data[0] & 0x80));
                            newUserApp.xpadData.CAOrgFlag = (0!= (userApp.data[0] & 0x40));
                            newUserApp.xpadData.xpadAppTy = userApp.data[0]  & 0x1F;
                            newUserApp.xpadData.dgFlag = (0 != (userApp.data[1] & 0x80));
                            newUserApp.xpadData.DScTy = DabAudioDataSCty(userApp.data[1] & 0x3F);
                        }

                        scIt->userApps.insert(newUserApp.uaType, newUserApp);
                    }
                }
                else
                { /* SC not found - this should not happen */ }
            }
            else
            { /* service nod found - this should not happen */ }
        }
        else
        {   // no user apps for some service
        }

        if (--m_numRequestsPending == 0)
        {
            // enable SLS automatically - if available
            startUserApplication(DabUserApplicationType::SlideShow, true);

            qDebug() << "=== MCI complete";
            emit ensembleConfiguration(ensembleConfigurationString());
            if (m_isReconfigurationOngoing)
            {
                m_isReconfigurationOngoing = false;

                // find current service selection
            }
            emit ensembleComplete(m_ensemble);
        }

        delete pList;
    }
        break;
    case RadioControlEventType::SERVICE_SELECTION:
    {
        dabsdrNtfServiceSelection_t * pData = reinterpret_cast<dabsdrNtfServiceSelection_t *>(pEvent->pData);
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_SELECTION success";
#endif
            serviceConstIterator serviceIt = m_serviceList.constFind(pData->SId);
            if (serviceIt != m_serviceList.cend())
            {   // service is in the list
                serviceComponentConstIterator scIt = serviceIt->serviceComponents.constFind(pData->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service components exists in service                                                           
                    if (!scIt->autoEnabled)
                    {   // if not data service that is autoimatically enabled
                        // store current service
                        m_currentService.SId = pData->SId;
                        m_currentService.SCIdS = pData->SCIdS;
                        emit audioServiceSelection(*scIt);

                        // enable SLS automatically - if available
                        startUserApplication(DabUserApplicationType::SlideShow, true);

//#warning "Remove automatic Journaline - this is for debug only"
//                        startUserApplication(DabUserApplicationType::Journaline, true);
#if RADIO_CONTROL_SPI_ENABLE
#warning "Remove automatic SPI - this is for debug only"
                         startUserApplication(DabUserApplicationType::SPI, true);
#endif
//#warning "Remove automatic TPEG - this is for debug only"
                         //startUserApplication(DabUserApplicationType::TPEG, true);
                    }
                }
            }
        }        
        else
        {
            if (m_isReconfigurationOngoing)
            {
                if ((m_currentService.SId == pData->SId) && (m_currentService.SCIdS == pData->SCIdS))
                {   // current service is no longer available -> playback will be stopped => emit dummy service component
                    emit audioServiceReconfiguration(RadioControlServiceComponent());
                }
            }

            qDebug() << "RadioControlEvent::SERVICE_SELECTION error" << pEvent->status;
            m_currentService.SId = 0;
        }
        delete pData;
    }
        break;

    case RadioControlEventType::SERVICE_STOP:
    {
        dabsdrNtfServiceStop_t * pData = reinterpret_cast<dabsdrNtfServiceStop_t *>(pEvent->pData);
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_STOP success";
#endif

            serviceIterator serviceIt = m_serviceList.find(pData->SId);
            if (serviceIt != m_serviceList.end())
            {   // service is in the list
                serviceComponentIterator scIt = serviceIt->serviceComponents.find(pData->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // found service component
                    scIt->autoEnabled = false;
                    qDebug() << "RadioControlEvent::SERVICE_STOP" << pData->SId << pData->SCIdS;
                }
            }
        }
        else
        {
            qDebug() << "RadioControlEvent::SERVICE_STOP error" << pEvent->status;
        }
        delete pData;
    }
        break;
    case RadioControlEventType::XPAD_APP_START_STOP:
    {
        dabsdrXpadAppStartStop_t * pData = reinterpret_cast<dabsdrXpadAppStartStop_t *>(pEvent->pData);
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::XPAD_APP_START_STOP success";
#endif
        }
        else
        {
            qDebug() << "RadioControlEvent::XPAD_APP_START_STOP error" << pEvent->status;
        }
        delete pData;
     }
        break;
    case RadioControlEventType::AUTO_NOTIFICATION:
    {
        dabsdrNtfPeriodic_t * pData = reinterpret_cast<dabsdrNtfPeriodic_t *>(pEvent->pData);

        if (pData->dateHoursMinutes != 0)
        {
            // decode time
            // bits 30-14 MJD
            int32_t mjd = (pData->dateHoursMinutes >> 14) & 0x01FFFF;
            // bit 13 LSI
            // uint8_t lsi = (pData->dateHoursMinutes >> 13) & 0x01;
            // bits 10-6 Hours
            uint8_t h = (pData->dateHoursMinutes >> 6) & 0x1F;
            // bits 5-0 Minutes
            uint8_t minute = pData->dateHoursMinutes & 0x3F;

            // Convert Modified Julian Date (according to wikipedia)
            int32_t J   = mjd + 2400001;
            int32_t j   = J + 32044;
            int32_t g   = j / 146097;
            int32_t dg  = j % 146097;
            int32_t c   = ((dg / 36524) + 1) * 3 / 4;
            int32_t dc  = dg - c * 36524;
            int32_t b   = dc / 1461;
            int32_t db  = dc%1461;
            int32_t a   = ((db / 365) + 1) * 3 / 4;
            int32_t da  = db - a * 365;
            int32_t y   = g * 400 + c * 100 + b * 4 + a;
            int32_t m   = ((da * 5 + 308) / 153) - 2;
            int32_t d   = da - ((m + 4) * 153 / 5) + 122;
            int32_t Y   = y - 4800 + ((m + 2) / 12);
            int32_t M   = ((m + 2) % 12) + 1;
            int32_t D   = d + 1;

            int32_t sec = pData->secMsec >> 10;
            int32_t msec = pData->secMsec & 0x3FF;

            QDateTime dateAndTime = QDateTime(QDate(Y, M, D), QTime(h, minute, sec, msec), Qt::UTC).toOffsetFromUtc(60*(m_ensemble.LTO * 30));

            emit dabTime(dateAndTime);
        }
        updateSyncLevel(pData->syncLevel);
        if (pData->snr10 > 0)
        {
            emit snrLevel(pData->snr10/10.0);
        }
        else
        {
            emit snrLevel(0);
        }

        emit freqOffset(pData->freqOffset*0.1);
        emit fibCounter(RADIO_CONTROL_NOTIFICATION_FIB_EXPECTED, pData->fibErrorCntr);
        emit mscCounter(pData->mscCrcOkCntr, pData->mscCrcErrorCntr);

#if RADIO_CONTROL_VERBOSE > 0
        qDebug("AutoNotify: sync %d, freq offset = %.1f Hz, SNR = %.1f dB",
               pData->syncLevel, pData->freqOffset*0.1, pData->snr10/10.0);
#endif

        delete pData;
    }
        break;
    case RadioControlEventType::DATAGROUP_DL:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEvent::DATAGROUP_DL";
#endif
        QByteArray * pData = reinterpret_cast<QByteArray *>(pEvent->pData);
        emit dlDataGroup(*pData);
        delete pData;
    }
        break;
    case RadioControlEventType::USERAPP_DATA:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEvent::DATAGROUP_MSC";
#endif
        RadioControlUserAppData * pData = reinterpret_cast<RadioControlUserAppData *>(pEvent->pData);
        emit userAppData(*pData);

        delete pData;
    }
        break;

    default:
        qDebug() << Q_FUNC_INFO << "ERROR: Unsupported event" << int(pEvent->type);
    }

    delete pEvent;
}

void RadioControl::exit()
{
    dabsdrRequest_Exit(m_dabsdrHandle);
}

void RadioControl::start(uint32_t freq)
{
    if (freq)
    {   // when input device tuned, freq is passed to be set in SDR
        // when frequency is 0 then we are done (input device is in idle)
        m_enaAutoNotification = false;
        m_frequency = freq;
        m_syncLevel = DABSDR_SYNC_LEVEL_NO_SYNC;
        emit syncStatus(uint8_t(DabSyncLevel::NoSync));
        m_serviceList.clear();
        clearEnsemble();
        dabTune(freq);
    }
    else
    {
        // tune is finished , notify HMI
        emit tuneDone(0);
    }
}

void RadioControl::tuneService(uint32_t freq, uint32_t SId, uint8_t SCIdS)
{
    if (freq == m_frequency)
    {
        if (SId)
        {
            if (m_serviceRequest.SId != 0)
            {   // this can happen when service is selected while still acquiring ensemble infomation
                // ignoring the request
                return;
            }

            // clear request
            m_serviceRequest.SId = 0;

            // remove automatically enabled data services
            serviceConstIterator serviceIt = m_serviceList.constFind(m_currentService.SId);
            if (serviceIt != m_serviceList.cend())
            {   // service is in the list
                for (auto & sc : serviceIt->serviceComponents)
                {
                    if (sc.autoEnabled)
                    {   // request stop
                        dabServiceStop(m_currentService.SId, sc.SCIdS);
                    }
                }
            }
            else
            { /* this should not happen */ }

            // reset current service
            m_currentService.SId = 0;

            dabServiceSelection(SId, SCIdS);
        }
    }
    else
    {   // service selection will be done after tune
        m_enaAutoNotification = false;
        m_frequency = freq;

        // reset current service - tuning resets dab process
        m_currentService.SId = 0;

        m_serviceRequest.SId = SId;
        m_serviceRequest.SCIdS = SCIdS;

        m_serviceList.clear();

        dabTune(0);
     }
}

void RadioControl::updateSyncLevel(dabsdrSyncLevel_t s)
{   
    if (s != m_syncLevel)
    {   // new sync level => emit signal
        m_syncLevel = s;
        switch (m_syncLevel)
        {
        case DABSDR_SYNC_LEVEL_NO_SYNC:
            emit syncStatus(uint8_t(DabSyncLevel::NoSync));
            emit snrLevel(0);
            break;
        case DABSDR_SYNC_LEVEL_ON_NULL:
            emit syncStatus(uint8_t(DabSyncLevel::NullSync));
            break;
        case DABSDR_SYNC_LEVEL_FIC:
            emit syncStatus(uint8_t(DabSyncLevel::FullSync));
            break;
        }
    }
    if ((m_syncLevel > DABSDR_SYNC_LEVEL_NO_SYNC) && (!m_enaAutoNotification))
    {
        // enable auto notifications
        dabEnableAutoNotification();
        m_enaAutoNotification = true;
    }
}

bool RadioControl::getCurrentAudioServiceComponent(serviceComponentIterator &scIt)
{
    for (auto & service : m_serviceList)
    {
        if (service.SId.value() == m_currentService.SId)
        {
            scIt = service.serviceComponents.find(m_currentService.SCIdS);
            return (service.serviceComponents.end() != scIt);
        }
    }
    return false;
}

bool RadioControl::cgetCurrentAudioServiceComponent(serviceComponentConstIterator &scIt) const
{
    for (const auto & service : m_serviceList)
    {
        if (service.SId.value() == m_currentService.SId)
        {
            scIt = service.serviceComponents.constFind(m_currentService.SCIdS);
            return (service.serviceComponents.cend() != scIt);
        }
    }
    return false;
}

void RadioControl::getEnsembleConfiguration()
{
    emit ensembleConfiguration(ensembleConfigurationString());
}

void RadioControl::startUserApplication(DabUserApplicationType uaType, bool start)
{
    serviceComponentConstIterator scIt;
    if (cgetCurrentAudioServiceComponent(scIt))
    {   // found
        // first check XPAD applications
        QHash<DabUserApplicationType,RadioControlUserApp>::const_iterator uaIt = scIt->userApps.constFind(uaType);
        if (scIt->userApps.cend() != uaIt)
        {
            qDebug() << "Found XPAD app" << int(uaType);
            dabXPadAppStart(uaIt->xpadData.xpadAppTy, 1);
            return;
        }

        // not found in XPAD - try secondary data services
        serviceIterator serviceIt = m_serviceList.find(m_currentService.SId);
        for (auto & sc : serviceIt->serviceComponents)
        {
            if (!sc.isAudioService())
            {   // service component is not audio service
                uaIt = sc.userApps.constFind(uaType);
                if (sc.userApps.cend() != uaIt)
                {
                    sc.autoEnabled = start;
                    if (start)
                    {
                        dabServiceSelection(sc.SId.value(), sc.SCIdS);
                    }
                    else
                    {
                        dabServiceStop(sc.SId.value(), sc.SCIdS);
                    }
                    return;
                }
            }
        }

        // TODO: not found - try ensemble
        // #warning "Debug - remove or make implementation clean"
        for (auto & service : m_serviceList)
        {
            if (!service.SId.isProgServiceId())
            {   // data service
                for (auto & sc : service.serviceComponents)
                {
                    if (!sc.isAudioService())
                    {   // service component is not audio service
                        uaIt = sc.userApps.constFind(uaType);
                        if (sc.userApps.cend() != uaIt)
                        {
                            sc.autoEnabled = start;
                            if (start)
                            {
                                dabServiceSelection(sc.SId.value(), sc.SCIdS);
                            }
                            else
                            {
                                dabServiceStop(sc.SId.value(), sc.SCIdS);
                            }
                            return;
                        }
                    }
                }
            }
        }
    }
}

QString RadioControl::ensembleConfigurationString() const
{
    if (0 == m_serviceList.size())
    {
        return QString("");
    }

    QString output;
    QTextStream strOut(&output, QIODevice::Text);

    strOut << "<dl>";
    strOut << "<dt>Ensemble:</dt>";
    strOut << QString("<dd>0x%1 <b>%2</b> [ <i>%3</i> ]  ECC = 0x%4, UTC %5 min, INT = %6, alarm announcements = %7</dd>")
              .arg(QString("%1").arg(m_ensemble.eid(), 4, 16, QChar('0')).toUpper())
              .arg(m_ensemble.label)
              .arg(m_ensemble.labelShort)
              .arg(QString("%1").arg(m_ensemble.ecc(), 2, 16, QChar('0')).toUpper())
              .arg(m_ensemble.LTO*30)
              .arg(m_ensemble.intTable)
              .arg(m_ensemble.alarm);
    strOut << "</dl>";

    strOut << "<dl>";
    strOut << "<dt>";
    strOut << QString("Services (%1):").arg(m_serviceList.size());
    strOut << "</dt>";

    for (auto const & s : m_serviceList)
    {
        strOut << "<dd>";

        strOut << "<dl>";
        strOut << "<dt>";
        if (s.SId.isProgServiceId())
        {   // programme service
            strOut << QString("0x%1 <b>%2</b> [ <i>%3</i> ] ECC = 0x%4,")
                              .arg(QString("%1").arg(s.SId.progSId(), 4, 16, QChar('0')).toUpper())
                              .arg(s.label)
                              .arg(s.labelShort)
                              .arg(QString("%1").arg(s.SId.ecc(), 2, 16, QChar('0')).toUpper());

            strOut << QString(" PTY: %1").arg(s.pty.s);
            if (s.pty.d != 0)
            {
                strOut << QString(" (dynamic %1)").arg(s.pty.d);
            }
            else
            {
                strOut << " (static)";
            }
        }
        else
        {   // data service
            strOut << QString("0x%1 <b>%2</b> [ <i>%3</i> ]")
                   .arg(QString("%1").arg(s.SId.value(), 8, 16, QChar('0')).toUpper())
                   .arg(s.label)
                   .arg(s.labelShort);
        }
        if (s.CAId)
        {
            strOut << QString(", CAId %1").arg(s.CAId);
        }
        strOut << "</dt>";

        for (auto const & sc : s.serviceComponents)
        {
            strOut << "<dd>";
            if (sc.isDataPacketService())
            {
                strOut << "DataComponent (MSC Packet Data)";
                strOut << ((sc.ps) ? " (primary)," : " (secondary),");
                strOut << QString(" SCIdS: %1,").arg(sc.SCIdS);
                strOut << QString(" SCId: %1,").arg(sc.packetData.SCId);
                strOut << QString(" Language: %1,").arg(DabTables::getLangName(sc.lang));
            }
            else
            {
                strOut << ((sc.isDataStreamService()) ? "DataComponent (MSC Stream Data)" : "AudioComponent");
                strOut << ((sc.ps) ? " (primary)," : " (secondary),");
                strOut << QString(" SCIdS: %1,").arg(sc.SCIdS);
            }

            strOut << QString(" Label: '%1' [ '%2' ], ").arg(sc.label).arg(sc.labelShort);

            DabAudioDataSCty scType;
            if (sc.isDataPacketService())
            {
                scType = sc.packetData.DSCTy;
            }
            else
            {
                scType = sc.streamAudioData.scType;
            }

            switch (scType)
            {
            case DabAudioDataSCty::DAB_AUDIO:
                strOut << QString("ASCTy: 0x%2 (MP2)").arg(QString::number(int(scType), 16).toUpper());
                break;
            case DabAudioDataSCty::DABPLUS_AUDIO:
                strOut << QString("ASCTy: 0x%2 (AAC)").arg(QString::number(int(scType), 16).toUpper());
                break;
            case DabAudioDataSCty::TDC:
                strOut << QString("DSCTy: 0x%2 (TDC)").arg(QString::number(int(scType), 16).toUpper());
                break;
            case DabAudioDataSCty::MPEG2TS:
                strOut << QString("DSCTy: 0x%2 (MPEG2TS)").arg(QString::number(int(scType), 16).toUpper());
                break;
            case DabAudioDataSCty::MOT:
                strOut << QString("DSCTy: 0x%2 (MOT)").arg(QString::number(int(scType), 16).toUpper());
                break;
            case DabAudioDataSCty::PROPRIETARY_SERVICE:
                strOut << QString("DSCTy: 0x%2 (Proprietary)").arg(QString::number(int(scType), 16).toUpper());
                break;
            default:
                strOut << QString("DSCTy: 0x%2 (unknown)").arg(QString::number(int(scType), 16).toUpper());
                break;
            }

            if (sc.isDataPacketService())
            {
                strOut << QString(", DG: %1, PacketAddr: %2")
                          .arg(sc.packetData.DGflag)
                          .arg(sc.packetData.packetAddress);
            }
            else
            {  /* do nothing */ }
            strOut << "<br>";

            strOut << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
            strOut << QString("SubChId: %1, Language: %2, StartCU: %3, NumCU: %4,")
                      .arg(sc.SubChId)
                      .arg(sc.lang)
                      .arg(sc.SubChAddr)
                      .arg(sc.SubChSize);
            if (sc.protection.isEEP())
            {   // EEP
                if (sc.protection.level < DabProtectionLevel::EEP_1B)
                {  // EEP x-A
                    strOut << QString(" EEP %1-%2").arg(int(sc.protection.level) - int(DabProtectionLevel::EEP_1A) + 1).arg("A");
                }
                else
                {  // EEP x+B
                    strOut << QString(" EEP %1-%2").arg(int(sc.protection.level) - int(DabProtectionLevel::EEP_1B) + 1).arg("B");
                }
                if (sc.isDataPacketService())
                {
                    if (sc.protection.fecScheme)
                    {
                        strOut << " [FEC scheme applied]";
                    }
                }
                strOut << QString(", Coderate: %1/%2").arg(sc.protection.codeRateUpper).arg(sc.protection.codeRateLower);
            }
            else
            {  // UEP
                strOut << QString(" UEP #%1, Protection level: %2").arg(sc.protection.uepIndex).arg(int(sc.protection.level));
            }
            if (!sc.isDataPacketService())
            {
                strOut << QString(", Bitrate: %1kbps").arg(sc.streamAudioData.bitRate);
            }
            int uaCntr = 1;
            for (const auto & ua : sc.userApps)
            {
                strOut << "<br>";
                strOut << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
                strOut << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
                strOut << QString("UserApp %1/%2: Label: '%3' [ '%4' ], ")
                          .arg(uaCntr++).arg(sc.userApps.size())
                          .arg(ua.label)
                          .arg(ua.labelShort);

                switch (ua.uaType)
                {
                case DabUserApplicationType::SlideShow:
                    strOut << QString("UAType: 0x%1 (SlideShow)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::TPEG:
                    strOut << QString("UAType: 0x%1 (TPEG)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::SPI:
                    strOut << QString("UAType: 0x%1 (SPI)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::DMB:
                    strOut << QString("UAType: 0x%1 (DMB)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::Filecasting:
                    strOut << QString("UAType: 0x%1 (Filecasting)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::FIS:
                    strOut << QString("UAType: 0x%1 (FIS)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                case DabUserApplicationType::Journaline:
                    strOut << QString("UAType: 0x%1 (Journaline)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                default:
                    strOut << QString("UAType: 0x%1 (unknown)").arg(QString::number(int(ua.uaType), 16).toUpper());
                    break;
                }

                if (sc.isAudioService())
                {
                    strOut << QString(", X-PAD AppTy: %1, ").arg(ua.xpadData.xpadAppTy);
                    switch (ua.xpadData.DScTy)
                    {
                    case DabAudioDataSCty::TDC:
                        strOut << QString("DSCTy: 0x%2 (TDC)").arg(QString::number(int(ua.xpadData.DScTy), 16).toUpper());
                        break;
                    case DabAudioDataSCty::MPEG2TS:
                        strOut << QString("DSCTy: 0x%2 (MPEG2TS)").arg(QString::number(int(ua.xpadData.DScTy), 16).toUpper());
                        break;
                    case DabAudioDataSCty::MOT:
                        strOut << QString("DSCTy: 0x%2 (MOT)").arg(QString::number(int(ua.xpadData.DScTy), 16).toUpper());
                        break;
                    case DabAudioDataSCty::PROPRIETARY_SERVICE:
                        strOut << QString("DSCTy: 0x%2 (Proprietary)").arg(QString::number(int(ua.xpadData.DScTy), 16).toUpper());
                        break;
                    default:
                        strOut << QString("DSCTy: 0x%2 (unknown)").arg(QString::number(int(ua.xpadData.DScTy), 16).toUpper());
                        break;
                    }
                    strOut << QString(", DG: %3").arg(ua.xpadData.dgFlag);
                }
                strOut << QString(", Data (%1) [").arg(ua.uaData.size());
                for (int d = 0; d < ua.uaData.size(); ++d)
                {
                    strOut << QString("%1").arg(ua.uaData.at(d), 2, 16, QLatin1Char('0')).toUpper();
                }
                strOut << "]";
            }
            strOut << "</dd>";
        }
        strOut << "</dl>";
        strOut << "</dd>";
    }
    strOut << "</dl>";

    strOut.flush();

    return output;
}

void RadioControl::clearEnsemble()
{    
    m_ensemble.ueid = RADIO_CONTROL_UEID_INVALID;
    m_ensemble.label.clear();
    m_ensemble.labelShort.clear();
    m_ensemble.frequency = 0;

    emit ensembleConfiguration("");
}

QString RadioControl::toShortLabel(QString & label, uint16_t charField) const
{
    QString out("");
    uint16_t movingOne = 1 << 15;
    for (int n = 0; n< label.size(); ++n)
    {
        if (charField & movingOne)
        {
            out.append(label.at(n));
        }
        movingOne >>= 1;
    }
    return out;
}

void RadioControl::dabNotificationCb(dabsdrNotificationCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = static_cast<RadioControl *>(ctx);
    switch (p->nid)
    {
    case DABSDR_NID_SYNC_STATUS:
    {
        RadioControlEvent * pEvent = new RadioControlEvent;

        const dabsdrNtfSyncStatus_t * pInfo = static_cast<const dabsdrNtfSyncStatus_t *>(p->pData);
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_SYNC_STATUS: %d", pInfo->syncLevel);
#endif

        pEvent->type = RadioControlEventType::SYNC_STATUS;
        pEvent->status = p->status;
        pEvent->pData = static_cast<intptr_t>(pInfo->syncLevel);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_TUNE:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_TUNE: status %d", p->status);
#endif
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::TUNE;
        pEvent->status = p->status;
        pEvent->pData = static_cast<intptr_t>(*((uint32_t*) p->pData));
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_ENSEMBLE_INFO:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_ENSEMBLE_INFO: status %d", p->status);
#endif
        dabsdrNtfEnsemble_t * pEnsembleInfo = new dabsdrNtfEnsemble_t;
        memcpy(pEnsembleInfo, p->pData, sizeof(dabsdrNtfEnsemble_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::ENSEMBLE_INFO;
        pEvent->status = p->status;
        //pEvent->pData = intptr_t(pEnsembleInfo);
        pEvent->pData = reinterpret_cast<intptr_t>(pEnsembleInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;                
    case DABSDR_NID_SERVICE_LIST:
    {
        const dabsdrNtfServiceList_t * pInfo = (const dabsdrNtfServiceList_t *) p->pData;
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_SERVICE_LIST: num services %d", pInfo->numServices);
#endif
        dabsdrServiceListItem_t item;
        QList<dabsdrServiceListItem_t> *pServiceList = new QList<dabsdrServiceListItem_t>;
        for (int s = 0; s < pInfo->numServices; ++s)
        {
            pInfo->getServiceListItem(radioCtrl->m_dabsdrHandle, s, &item);
            pServiceList->append(item);
        }

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_LIST;
        pEvent->status = p->status;
        pEvent->pData = reinterpret_cast<intptr_t>(pServiceList);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_SERVICE_COMPONENT_LIST:
    {
        const dabsdrNtfServiceComponentList_t * pInfo = (const dabsdrNtfServiceComponentList_t * ) p->pData;
        if (DABSDR_NSTAT_SUCCESS == p->status)
        {
            QList<dabsdrServiceCompListItem_t> * pList = new QList<dabsdrServiceCompListItem_t>;

            dabsdrServiceCompListItem_t item;
            for (int s = 0; s < pInfo->numServiceComponents; ++s)
            {
                pInfo->getServiceComponentListItem(radioCtrl->m_dabsdrHandle, s, &item);
                pList->append(item);
            }

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::SERVICE_COMPONENT_LIST;
            pEvent->status = p->status;
            pEvent->pData = reinterpret_cast<intptr_t>(pList);
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X not found", pInfo->sid);
        }
    }
        break;
    case DABSDR_NID_USER_APP_LIST:
    {
        const dabsdrNtfUserAppList_t * pInfo = (const dabsdrNtfUserAppList_t * ) p->pData;
        if (DABSDR_NSTAT_SUCCESS == p->status)
        {
            QList<dabsdrUserAppListItem_t> * pList = new QList<dabsdrUserAppListItem_t>;

            dabsdrUserAppListItem_t item;
            for (int s = 0; s < pInfo->numUserApps; ++s)
            {
                pInfo->getUserAppListItem(radioCtrl->m_dabsdrHandle, s, &item);
                pList->append(item);
            }

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::USER_APP_LIST;
            pEvent->status = p->status;
            pEvent->pData = reinterpret_cast<intptr_t>(pList);
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X, SCIdS %2.2X not found", pInfo->SId, pInfo->SCIdS);
        }
    }
    break;
    case DABSDR_NID_SERVICE_SELECTION:
    {
        dabsdrNtfServiceSelection_t * pServSelectionInfo = new dabsdrNtfServiceSelection_t;
        memcpy(pServSelectionInfo, p->pData, sizeof(dabsdrNtfServiceSelection_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_SELECTION;

        pEvent->status = p->status;
        pEvent->pData = reinterpret_cast<intptr_t>(pServSelectionInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_SERVICE_STOP:
    {
        dabsdrNtfServiceStop_t * pServStopInfo = new dabsdrNtfServiceStop_t;
        memcpy(pServStopInfo, p->pData, sizeof(dabsdrNtfServiceStop_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_STOP;

        pEvent->status = p->status;
        pEvent->pData = reinterpret_cast<intptr_t>(pServStopInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_XPAD_APP_START_STOP:
    {
        dabsdrXpadAppStartStop_t * pServStopInfo = new dabsdrXpadAppStartStop_t;
        memcpy(pServStopInfo, p->pData, sizeof(dabsdrXpadAppStartStop_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::XPAD_APP_START_STOP;

        pEvent->status = p->status;
        pEvent->pData = reinterpret_cast<intptr_t>(pServStopInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_PERIODIC:
    {
        if (p->pData)
        {
            assert(sizeof(dabsdrNtfPeriodic_t) == p->len);

            dabsdrNtfPeriodic_t * notifyData = new dabsdrNtfPeriodic_t;
            memcpy((uint8_t*)notifyData, p->pData, p->len);

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::AUTO_NOTIFICATION;
            pEvent->status = p->status;
            pEvent->pData = reinterpret_cast<intptr_t>(notifyData);
            radioCtrl->emit_dabEvent(pEvent);
        }
    }
        break;
    case DABSDR_NID_RECONFIGURATION:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_RECONFIGURATION: status %d", p->status);
#endif
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::RECONFIGURATION;

        pEvent->status = p->status;
        pEvent->pData = reinterpret_cast<intptr_t>(nullptr);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_RESET:
    {
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::RESET;

        pEvent->status = p->status;
        pEvent->pData = static_cast<intptr_t>(*((dabsdrNtfResetFlags_t*) p->pData));
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    default:
        qDebug("Unexpected notification %d", p->nid);
    }
}

void RadioControl::dynamicLabelCb(dabsdrDynamicLabelCBData_t * p, void * ctx)
{
    if (0 == p->len)
    {   // do nothing - empty data group
        qDebug("Empty DL data received\n");
        return;
    }
    RadioControl * radioCtrl = static_cast<RadioControl *>(ctx);

    QByteArray * data = new QByteArray((const char *)p->pData, p->len);

    RadioControlEvent * pEvent = new RadioControlEvent;
    pEvent->type = RadioControlEventType::DATAGROUP_DL;
    pEvent->status = DABSDR_NSTAT_SUCCESS;
    pEvent->pData = reinterpret_cast<intptr_t>(data);
    radioCtrl->emit_dabEvent(pEvent);
}

void RadioControl::dataGroupCb(dabsdrDataGroupCBData_t * p, void * ctx)
{
    if (0 == p->dgLen)
    {   // do nothing - empty data group
        qDebug("Empty data group type %d received\n", p->userAppType);
        return;
    }

    RadioControl * radioCtrl = static_cast<RadioControl *>(ctx);
    RadioControlUserAppData * pData = new RadioControlUserAppData;
    pData->userAppType = DabUserApplicationType(p->userAppType);

    // copy data to QByteArray
    pData->data = QByteArray((const char *)p->pDgData, p->dgLen);

    RadioControlEvent * pEvent = new RadioControlEvent;
    pEvent->type = RadioControlEventType::USERAPP_DATA;
    pEvent->status = DABSDR_NSTAT_SUCCESS;
    pEvent->pData = reinterpret_cast<intptr_t>(pData);
    radioCtrl->emit_dabEvent(pEvent);        
}

void RadioControl::audioDataCb(dabsdrAudioCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = static_cast<RadioControl *>(ctx);

    QByteArray * pData = new QByteArray;
    pData->clear();
    pData->append(p->header.raw);
    pData->append((const char *) p->pAuData, p->auLen);

    radioCtrl->emit_audioData(pData);
}
