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
    m_ensembleInfoTimeoutTimer = new QTimer(this);
    m_ensembleInfoTimeoutTimer->setSingleShot(true);
    m_ensembleInfoTimeoutTimer->setInterval(RADIO_CONTROL_ENSEMBLE_TIMEOUT_SEC*1000);
    connect(m_ensembleInfoTimeoutTimer, &QTimer::timeout, this, &RadioControl::onEnsembleInfoFinished);

    m_currentService.announcement.timeoutTimer = new QTimer(this);
    m_currentService.announcement.timeoutTimer->setSingleShot(true);
    m_currentService.announcement.timeoutTimer->setInterval(RADIO_CONTROL_ANNOUNCEMENT_TIMEOUT_SEC*1000);
    connect(m_currentService.announcement.timeoutTimer, &QTimer::timeout, this, &RadioControl::onAnnouncementTimeout);
    connect(this, &RadioControl::announcementAudioAvailable, this, &RadioControl::onAnnouncementAudioAvailable, Qt::QueuedConnection);

    connect(this, &RadioControl::dabEvent, this, &RadioControl::onDabEvent, Qt::QueuedConnection);
}

RadioControl::~RadioControl()
{
    m_ensembleInfoTimeoutTimer->stop();
    delete m_ensembleInfoTimeoutTimer;
    m_currentService.announcement.timeoutTimer->stop();
    delete m_currentService.announcement.timeoutTimer;

    // this cancels dabsdr thread
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
        switch (pEvent->resetFlag)
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
#if RADIO_CONTROL_VERBOSE
        qDebug("Sync = %d", pEvent->syncLevel);
#endif

        if ((DABSDR_SYNC_LEVEL_FIC == pEvent->syncLevel) && (RADIO_CONTROL_UEID_INVALID == m_ensemble.ueid))
        { // ask for ensemble info if not available
            dabGetEnsembleInfo();
        }
        updateSyncLevel(pEvent->syncLevel);
    }
        break;
    case RadioControlEventType::TUNE:
    {
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE > 1
            qDebug() << "Tune success" << pEvent->frequency;
#endif
            if ((pEvent->frequency != m_frequency) || (0 == m_frequency))
            {   // in first step, DAB is tuned to 0 that is != desired frequency
                // or if we want to tune to 0 by purpose (tgo to IDLE)
                // this tunes input device to desired frequency
                emit tuneInputDevice(m_frequency);
            }
            else
            {   // tune is finished , notify HMI
                emit tuneDone(pEvent->frequency);

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
        dabsdrNtfEnsemble_t * pInfo = pEvent->pEnsembleInfo;

#if RADIO_CONTROL_VERBOSE
        qDebug("RadioControlEvent::ENSEMBLE_INFO 0x%8.8X '%s'", pInfo->ueid, pInfo->label.str);
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

        delete pEvent->pEnsembleInfo;

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
        QList<dabsdrServiceListItem_t> * pServiceList = pEvent->pServiceList;
        if (0 == pServiceList->size())
        {   // no service list received (invalid probably)
            m_serviceList.clear();

            // send new request after some timeout
            QTimer::singleShot(100, this, &RadioControl::dabGetServiceList);
        }
        else
        {
            m_numReqPendingServiceList = 0;
            m_numReqPendingEnsemble = 0;
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
                newService.ASu = 0;
                m_serviceList.insert(sid.value(), newService);
                m_numReqPendingServiceList++;
                dabGetServiceComponent(sid.value());

                if (sid.isProgServiceId())
                {   // only programme services support announcements
                    // get supported announcements -> delay request by 1 second
                    uint32_t sidVal = sid.value();
                    m_numReqPendingEnsemble += 1;
                    QTimer::singleShot(1000, this, [this, sidVal](){dabGetAnnouncementSupport(sidVal); } );
                }
            }
        }
        delete pEvent->pServiceList;
    }
        break;
    case RadioControlEventType::SERVICE_COMPONENT_LIST:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEvent::SERVICE_COMPONENT_LIST";
#endif
        QList<dabsdrServiceCompListItem_t> * pList = pEvent->pServiceCompList;
        if (!pList->isEmpty() && m_ensemble.isValid())
        {   // all service components belong to the same SId
            DabSId sid(pEvent->SId, m_ensemble.ecc());
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

                        if (isCurrentService(serviceIt->SId.value(), newServiceComp.SCIdS) && (newServiceComp.isAudioService()))
                        {   // inform HMI about possible new service configuration
                            emit audioServiceReconfiguration(newServiceComp);
                        }
                    }
                    else
                    {
                        if ((m_serviceRequest.SId == serviceIt->SId.value()) && (m_serviceRequest.SCIdS == newServiceComp.SCIdS))
                        {
                            dabServiceSelection(m_serviceRequest.SId, m_serviceRequest.SCIdS, DABSDR_ID_AUDIO_PRIMARY);
                            m_serviceRequest.SId = 0;    // clear request
                        }
                    }

                    emit serviceListEntry(m_ensemble, newServiceComp);
                }
                if (requestUpdate)
                {
                    serviceIt->serviceComponents.clear();
                    uint32_t sidVal = sid.value();
                    QTimer::singleShot(100, this, [this, sidVal](){ dabGetServiceComponent(sidVal); } );
                }
                else
                {  // service list item information is complete
                   for (auto & serviceComp : serviceIt->serviceComponents)
                   {
                       serviceComp.userApps.clear();
                       m_numReqPendingEnsemble += 1;
                       dabGetUserApps(serviceIt->SId.value(), serviceComp.SCIdS);
                   }
                   if (--m_numReqPendingServiceList == 0)
                   {   // service list is complete
                       if (m_isReconfigurationOngoing)
                       {
                           m_isReconfigurationOngoing = false;

                           // find current service selection
                       }

                       emit serviceListComplete(m_ensemble);

                       // start timeout timer (10 sec) for getting info about uuser apps and announcements
                       m_ensembleInfoTimeoutTimer->start();
                   }
                }
            }
            else
            {  // SId not found
                qDebug("Service SID %8.8X not in service list", sid.value());
            }
        }

        delete pEvent->pServiceCompList;
    }
        break;
    case RadioControlEventType::USER_APP_LIST:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("RadioControlEvent::USER_APP_LIST SID %8.8X SCIdS %d", pEvent->SId, pEvent->SCIdS);
#endif
        m_numReqPendingEnsemble -= 1;
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
            QList<dabsdrUserAppListItem_t> * pList = pEvent->pUserAppList;
            if (!pList->isEmpty())
            {   // all user apps belong to the same SId
                DabSId sid(pEvent->SId, m_ensemble.ecc());

                // find service ID
                serviceIterator serviceIt = m_serviceList.find(sid.value());
                if (serviceIt != m_serviceList.end())
                {   // SId found
                    // all user apps belong to the same SId+SCIdS
                    serviceComponentIterator scIt = serviceIt->serviceComponents.find(pEvent->SCIdS);
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

                            if (isCurrentService(pEvent->SId, pEvent->SCIdS))
                            {   // if is is current service ==> start user applications
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

                            emit ensembleConfiguration(ensembleConfigurationString());
                        }
                    }
                    else { /* SC not found - this should not happen */ }
                }
                else { /* service not found - this should not happen */ }
            }
            else
            {   // no user apps
                // TSI EN 300 401 V2.1.1 [6.1]
                // FIG 0/13 may be signalled at a slower rate but not less frequently than once per second.
                // Lets try again in 1 second -> we do not know if there are really no user apps or we missed FIG0/13
                if (!m_isEnsembleInfoFinished)
                {
                    m_numReqPendingEnsemble += 1;
                    uint32_t sid = pEvent->SId;
                    uint8_t scids = pEvent->SCIdS;
                    QTimer::singleShot(1000, this, [this, sid, scids](){ dabGetUserApps(sid, scids); } );
                }
                else { /* timeout occured */ }
            }
        }
        if (m_numReqPendingEnsemble <= 0)
        {
            onEnsembleInfoFinished();
        }

        delete pEvent->pUserAppList;
    }
        break;
    case RadioControlEventType::SERVICE_SELECTION:
    {
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_SELECTION success";
#endif
            if (pEvent->decoderId == DABSDR_ID_AUDIO_PRIMARY)
            {
                serviceConstIterator serviceIt = m_serviceList.constFind(pEvent->SId);
                if (serviceIt != m_serviceList.cend())
                {   // service is in the list
                    serviceComponentConstIterator scIt = serviceIt->serviceComponents.constFind(pEvent->SCIdS);
                    if (scIt != serviceIt->serviceComponents.end())
                    {   // service components exists in service
                        if (!scIt->autoEnabled)
                        {   // if not data service that is autoimatically enabled
                            // store current service
                            m_currentService.SId = pEvent->SId;
                            m_currentService.SCIdS = pEvent->SCIdS;
                            emit audioServiceSelection(*scIt);

                            // discovery point -> request UserApps and Announcements information update
                            m_numReqPendingEnsemble += 2;
                            dabGetUserApps(m_currentService.SId, m_currentService.SCIdS);
                            dabGetAnnouncementSupport(m_currentService.SId);

                            // announcements and user apps are enabled when updated info is received
                            // setup announcements
                            setCurrentServiceAnnouncementSupport();

                            // enable SLS automatically - if already available
                            startUserApplication(DabUserApplicationType::SlideShow, true);

//#warning "Remove automatic Journaline - this is for debug only"
//                          startUserApplication(DabUserApplicationType::Journaline, true);
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
                qDebug() << "RadioControlEvent::SERVICE_SELECTION success instance" << int(pEvent->decoderId);
                m_currentService.announcement.switchState = AnnouncementSwitchState::WaitForAnnouncement;

                m_currentService.announcement.SId = pEvent->SId;
                m_currentService.announcement.SCIdS = pEvent->SCIdS;
            }
        }        
        else
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_SELECTION error" << pEvent->status;
#endif
            if (pEvent->decoderId == DABSDR_ID_AUDIO_PRIMARY)
            {
                if (m_isReconfigurationOngoing)
                {
                    if (isCurrentService(pEvent->SId, pEvent->SCIdS))
                    {   // current service is no longer available -> playback will be stopped => emit dummy service component
                        emit audioServiceReconfiguration(RadioControlServiceComponent());
                    }
                }
                resetCurrentService();
            }
            else
            {   // this is error when starting announcement
#if 1 // RADIO_CONTROL_VERBOSE
                qDebug() << "RadioControlEvent::SERVICE_SELECTION error" << pEvent->status << "on announcement start";
#endif
                // try to stop announcement
                onAnnouncementTimeout();
            }
        }
    }
        break;

    case RadioControlEventType::SERVICE_STOP:
    {
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_STOP success";
#endif

            serviceIterator serviceIt = m_serviceList.find(pEvent->SId);
            if (serviceIt != m_serviceList.end())
            {   // service is in the list
                serviceComponentIterator scIt = serviceIt->serviceComponents.find(pEvent->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // found service component
                    scIt->autoEnabled = false;
                    qDebug() << "RadioControlEvent::SERVICE_STOP" << pEvent->SId << pEvent->SCIdS;
                }
            }
        }
        else
        {
            qDebug() << "RadioControlEvent::SERVICE_STOP error" << pEvent->status;
        }
    }
        break;
    case RadioControlEventType::XPAD_APP_START_STOP:
    {
        //dabsdrXpadAppStartStop_t * pData = pEvent->pXpadAppStartStopInfo;
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
        delete pEvent->pXpadAppStartStopInfo;
     }
        break;
    case RadioControlEventType::AUTO_NOTIFICATION:
    {
         dabsdrNtfPeriodic_t * pData = pEvent->pNotifyData;

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

        delete pEvent->pNotifyData;
    }
    break;
    case RadioControlEventType::ANNOUNCEMENT_SUPPORT:
    {
        m_numReqPendingEnsemble -= 1;
        if (DABSDR_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug("RadioControlEventType::ANNOUNCEMENT_SUPPORT SId %8.8X", pEvent->SId);
#endif
            serviceIterator serviceIt = m_serviceList.find(pEvent->SId);
            if (serviceIt != m_serviceList.end())
            {   // service is in the list
                if (serviceIt->SId.isProgServiceId())
                {
                    serviceIt->ASu = pEvent->pAnnouncementSupport->ASu;
                    serviceIt->clusterIds.clear();
                    if (0 == pEvent->pAnnouncementSupport->numClusterIds)
                    {   // no announcement support -> request update
                        // TSI EN 300 401 V2.1.1 [8.1.6.1]
                        // The FIG 0/18 has a repetition rate of once per second.
                        // Lets try again in 1 second -> we do not know if there are really no announcements supported or we missed FIG0/18
                        if (!m_isEnsembleInfoFinished)
                        {
                             m_numReqPendingEnsemble += 1;
                             uint32_t sidVal = serviceIt->SId.value();
                             QTimer::singleShot(1000, this, [this, sidVal](){ dabGetAnnouncementSupport(sidVal); } );
                        }
                        else { /* timeout occured */ }
                    }
                    else
                    {   // announcements supported
                        for (int c = 0; c < pEvent->pAnnouncementSupport->numClusterIds; ++c)
                        {
                             serviceIt->clusterIds.append(pEvent->pAnnouncementSupport->clusterIds[c]);
                        }

                        if (pEvent->SId == m_currentService.SId)
                        {   // update current service
                             setCurrentServiceAnnouncementSupport();
                        }

                        emit ensembleConfiguration(ensembleConfigurationString());
                    }
                }
                else
                {   // data services do not support announcements
                    serviceIt->ASu = 0;
                }
            }
            else
            { /* service is not in the list - this should not happen */ }
        }
        if (m_numReqPendingEnsemble <= 0)
        {
            onEnsembleInfoFinished();
        }

        delete pEvent->pAnnouncementSupport;
    }
    break;
    case RadioControlEventType::ANNOUNCEMENT_SWITCHING:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEventType::ANNOUNCEMENT";
#endif
        dabsdrNtfAnnouncementSwitching_t * pAnnouncement = pEvent->pAnnouncement;

        // go through asw array
        dabsdrAsw_t * pAsw = pAnnouncement->asw;
        while (0 != pAsw->clusterId)
        {
            // check that announcement belongs to current service
            if (m_currentService.announcement.clusterIds.contains(pAsw->clusterId))
            {   // current service is member of announcement cluster and not alarm (already handled)
                announcementHandler(pAsw);
            }
            else
            {   // ignoring -> cluster not relevant for current service
#if RADIO_CONTROL_VERBOSE > 1
                qDebug() << "Ignoring announcement cluster ID" << pAsw->clusterId;
#endif
            }
            pAsw += 1;
        }
        delete pEvent->pAnnouncement;                
    }
        break;
    case RadioControlEventType::DATAGROUP_DL:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEvent::DATAGROUP_DL";
#endif
        if (DABSDR_ID_AUDIO_PRIMARY == pEvent->pDynamicLabelData->id)
        {
            emit dlDataGroup_Service(pEvent->pDynamicLabelData->data);
        }
        else
        {
            emit dlDataGroup_Announcement(pEvent->pDynamicLabelData->data);
        }

        delete pEvent->pDynamicLabelData;
    }
        break;
    case RadioControlEventType::USERAPP_DATA:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEvent::DATAGROUP_MSC";
#endif
        switch (pEvent->pUserAppData->id)
        {
        case DABSDR_ID_AUDIO_PRIMARY:
            emit userAppData_Service(*(pEvent->pUserAppData));
            break;
        case DABSDR_ID_AUDIO_SECONDARY:
            emit userAppData_Announcement(*(pEvent->pUserAppData));
            break;
        default:
            emit userAppData(*(pEvent->pUserAppData));
            break;
        }

        delete pEvent->pUserAppData;
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

            // stop any service running in secondary instance (announcment)
            dabsdrRequest_ServiceStop(m_dabsdrHandle, 0, 0, DABSDR_ID_AUDIO_SECONDARY);

            // remove automatically enabled data services
            serviceConstIterator serviceIt = m_serviceList.constFind(m_currentService.SId);
            if (serviceIt != m_serviceList.cend())
            {   // service is in the list
                for (auto & sc : serviceIt->serviceComponents)
                {
                    if (sc.autoEnabled)
                    {   // request stop
                        dabServiceStop(m_currentService.SId, sc.SCIdS, DABSDR_ID_DATA);
                    }
                }
            }
            else
            { /* this should not happen */ }

            // reset current service
            resetCurrentService();

            dabServiceSelection(SId, SCIdS, DABSDR_ID_AUDIO_PRIMARY);
        }
    }
    else
    {   // service selection will be done after tune
        m_enaAutoNotification = false;
        m_frequency = freq;

        // reset current service - tuning resets dab process
        resetCurrentService();

        m_serviceRequest.SId = SId;
        m_serviceRequest.SCIdS = SCIdS;

        m_serviceList.clear();

        dabTune(0);

        // request audio stop
        emit stopAudio();
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
            dabXPadAppStart(uaIt->xpadData.xpadAppTy, 1, DABSDR_ID_AUDIO_PRIMARY);
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
                        dabServiceSelection(sc.SId.value(), sc.SCIdS, DABSDR_ID_DATA);
                    }
                    else
                    {
                        dabServiceStop(sc.SId.value(), sc.SCIdS, DABSDR_ID_DATA);
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
                                dabServiceSelection(sc.SId.value(), sc.SCIdS, DABSDR_ID_DATA);
                            }
                            else
                            {
                                dabServiceStop(sc.SId.value(), sc.SCIdS, DABSDR_ID_DATA);
                            }
                            return;
                        }
                    }
                }
            }
        }
    }
}

void RadioControl::onAudioOutputRestart()
{   // restart can be caused by announcement or audio service reconfig
    serviceComponentConstIterator scIt;
    if (RadioControlAnnouncementState::None == m_currentService.announcement.state)
    {   // no announcement ongoing -> send current service
        if (cgetCurrentAudioServiceComponent(scIt))
        {
            emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, *scIt);
        }
    }
    else
    {   // ongoing announcement - find announcement service component
        serviceConstIterator sIt = m_serviceList.constFind(m_currentService.announcement.SId);
        if (m_serviceList.cend() != sIt)
        {   // found
            serviceComponentConstIterator scIt = sIt->serviceComponents.constFind(m_currentService.announcement.SCIdS);
            if (sIt->serviceComponents.cend() != scIt)
            {   // found
                emit announcement(m_currentService.announcement.id, m_currentService.announcement.state, *scIt);
            }
        }
    }
}

void RadioControl::setupAnnouncements(uint16_t enaFlags)
{
    m_currentService.announcement.enaFlags = enaFlags;
}

void RadioControl::suspendResumeAnnouncement()
{
    m_currentService.announcement.suspendRequest = !m_currentService.announcement.suspendRequest;

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
                strOut << QString(" (dynamic %1), ").arg(s.pty.d);
            }
            else
            {
                strOut << " (static), ";
            }
            if (0 == s.ASu)
            {
                strOut << "Announcements: No";
            }
            else
            {
                strOut << "Announcements: ";
                for (int b = 0; b < 16; ++b)
                {
                    if ((1 << b) & s.ASu)
                    {
                        strOut << DabTables::getAnnouncementName(static_cast<DabAnnouncement>(b)) << ", ";
                    }
                }
                strOut << QString("Cluster IDs [");
                strOut << QString("%1").arg(s.clusterIds.at(0), 2, 16, QLatin1Char('0')).toUpper();
                for (int d = 1; d < s.clusterIds.size(); ++d)
                {
                    strOut << QString(" %1").arg(s.clusterIds.at(d), 2, 16, QLatin1Char('0')).toUpper();
                }
                strOut << "]";
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
    m_ensemble.alarm = 0;
    m_ensembleInfoTimeoutTimer->stop();
    m_isEnsembleInfoFinished = false;

    emit ensembleConfiguration("");
}

void RadioControl::onEnsembleInfoFinished()
{
    m_ensembleInfoTimeoutTimer->stop();
    m_isEnsembleInfoFinished = true;
    if (m_numReqPendingEnsemble <= 0)
    {
#if 1 //RADIO_CONTROL_VERBOSE
        qDebug() << "=== Ensemble information should be complete";
#endif
        emit ensembleConfiguration(ensembleConfigurationString());
    }
}

void RadioControl::resetCurrentService()
{
    m_currentService.SId = 0;
    m_currentService.announcement.ASu = 0;
    m_currentService.announcement.clusterIds.clear();
    m_currentService.announcement.timeoutTimer->stop();
    m_currentService.announcement.activeCluster = 0;
    m_currentService.announcement.SId = 0;
    m_currentService.announcement.switchState = AnnouncementSwitchState::NoAnnouncement;
    m_currentService.announcement.state = RadioControlAnnouncementState::None;
    m_currentService.announcement.id = DabAnnouncement::Undefined;
}

void RadioControl::setCurrentServiceAnnouncementSupport()
{
    serviceConstIterator serviceIt = m_serviceList.constFind(m_currentService.SId);
    if (serviceIt != m_serviceList.cend())
    {
        m_currentService.announcement.ASu = serviceIt->ASu;
        m_currentService.announcement.clusterIds = serviceIt->clusterIds;
        if (m_ensemble.alarm)
        {   // alarm supported by ensemble
            // ETSI EN 300 401 V2.1.1 [8.1.6.2]
            // Cluster Id = "1111 1111" shall be used exclusively for all Alarm announcements.
            m_currentService.announcement.clusterIds.append(0xFF);

            // test alarm
            // ETSI TS 103 176 V2.4.1 [Annex G]
            m_currentService.announcement.clusterIds.append(0xFE);

            // enable alarm announcement
            m_currentService.announcement.ASu |= (1 << static_cast<int>(DabAnnouncement::Alarm));
        }
    }
    else
    {   // not found - this should not happen
        m_currentService.announcement.ASu = 0;
        m_currentService.announcement.clusterIds.clear();
    }
}

void RadioControl::onAnnouncementTimeout()
{
    // this can only happen when announcement starts and is not correctly finished
    m_currentService.announcement.timeoutTimer->stop();
    m_currentService.announcement.activeCluster = 0;
    m_currentService.announcement.id = DabAnnouncement::Undefined;
    m_currentService.announcement.state = RadioControlAnnouncementState::None;

    // forcing end of announcement
    if (m_currentService.announcement.isOtherService)
    {
        stopAnnouncement();
        // HMI will be notified when audio switches
    }
    else
    {   // current service or suspended
        // notify HMI
        serviceComponentIterator scIt;
        if (getCurrentAudioServiceComponent(scIt))
        {   // immediate notification
            emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, *scIt);
        }
    }
}

void RadioControl::onAnnouncementAudioAvailable()
{
    m_currentService.announcement.switchState = AnnouncementSwitchState::OngoingAnnouncement;
}

bool RadioControl::startAnnouncement(uint8_t subChId)
{
    // 1. check that subchId is not current service
    serviceComponentIterator scIt;
    if (getCurrentAudioServiceComponent(scIt) && (scIt->SubChId == subChId))
    {   // nothing to be done -> return value (false) indicates announcement in current subChannel
        m_currentService.announcement.SId = m_currentService.SId;
        m_currentService.announcement.SCIdS = m_currentService.SCIdS;
        m_currentService.announcement.isOtherService = false;
        return true;
    }

    // 2. find SId and SCIdS that match subChId
    DabSId sid;
    uint8_t scids;
    serviceConstIterator serviceIt = m_serviceList.cbegin();
    while ((serviceIt != m_serviceList.cend()) && !sid.isValid())
    {
        if (serviceIt->SId.isProgServiceId())
        {   // check service components
            serviceComponentConstIterator serviceCompIt = serviceIt->serviceComponents.cbegin();
            while (serviceCompIt != serviceIt->serviceComponents.cend())
            {
                if (serviceCompIt->SubChId == subChId)
                {   // found
                    sid = serviceIt->SId;   // valid sid stop outer loop
                    scids = serviceCompIt->SCIdS;

                    // breaking inner loop
                    break;
                }
                serviceCompIt++;
            }
        }
        serviceIt++;
    }

    // check if found
    if (sid.isValid())
    {
        // request secondary audio service
        dabsdrRequest_ServiceSelection(m_dabsdrHandle, sid.value(), scids, DABSDR_ID_AUDIO_SECONDARY);

        QHash<DabUserApplicationType,RadioControlUserApp>::const_iterator uaIt = scIt->userApps.constFind(DabUserApplicationType::SlideShow);
        if (scIt->userApps.cend() != uaIt)
        {
            dabXPadAppStart(uaIt->xpadData.xpadAppTy, 1, DABSDR_ID_AUDIO_SECONDARY);
        }

    }
    else
    {   // not found -> no audio service belongs to subChId (it should not happen)
        // ETSI TS 103 176 V2.4.1 [7.2.5 Signalling announcement switching]
        // The sub-channel indicated in the SubChId field of FIG 0/19 shall be a sub-channel
        // that belongs to an audio service component in the ensemble
        // ...
        // Service components that do not provide full current MCI and labels shall not serve as targets for announcements.
        m_currentService.announcement.isOtherService = false;
        return false;
    }

    m_currentService.announcement.SId = sid.value();
    m_currentService.announcement.SCIdS = scids;
    m_currentService.announcement.isOtherService = true;

    return true;
}

void RadioControl::stopAnnouncement()
{
    if (m_currentService.announcement.isOtherService)
    {   // stop announcement - sid, scids and subChId are not relevant, simply stopping secondary audio service
        dabsdrRequest_ServiceStop(m_dabsdrHandle, 0, 0, DABSDR_ID_AUDIO_SECONDARY);
        m_currentService.announcement.switchState = AnnouncementSwitchState::NoAnnouncement;
    }
    else
    {  /* do nothing */ }
}

void RadioControl::announcementHandler(dabsdrAsw_t *pAnnouncement)
{
#if 1
    switch (m_currentService.announcement.state)
    {
    case RadioControlAnnouncementState::None:
    {   // no announcement is active -> let check if announcement belong to current service
        //
        // ETSI TS 103 176 V2.4.1 [7.4.3]
        // The ASw flags field shall have one bit (bit flags b1 to b15) set to 1
        // corresponding to the announcement type while the announcement is active
        // ...
        // An ASw flag field with more than one bit flag set to 1 is invalid and shall be ignored.
        int announcementId = DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags);
        if ((static_cast<int>(DabAnnouncement::Alarm) == announcementId) && (0xFE == pAnnouncement->clusterId))
        {   // this is test mode
            // ETSI TS 103 176 V2.4.1 [Annex G]
            announcementId = static_cast<int>(DabAnnouncement::AlarmTest);
        }
        if ((static_cast<int>(DabAnnouncement::Alarm) == announcementId) && (0xFF != pAnnouncement->clusterId))
        {   // ETSI TS 103 176 V2.4.1 [7.3.4]
            // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
            // the ASw field shall be ignored.
            return;
        }
        if ((announcementId >= 0) && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
        {   // valid ASw
#if 1 // RADIO_CONTROL_VERBOSE > 1
            qDebug() << DabTables::getAnnouncementName(static_cast<DabAnnouncement>(announcementId))
                     << "announcement in subchannel" <<  pAnnouncement->subChId
                     << "cluster ID" << pAnnouncement->clusterId;
#endif
            m_currentService.announcement.suspendRequest = false;
            if (startAnnouncement(pAnnouncement->subChId))
            {   // found subchannel
                m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                m_currentService.announcement.timeoutTimer->start();
                m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                if (!m_currentService.announcement.isOtherService)
                {   // announcement in current service
                    m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;

                    // delay to compensate audio delay somehow
                    serviceComponentIterator scIt;
                    if (getCurrentAudioServiceComponent(scIt))
                    {
                        // Inform HMI -> announcement on current service
                        RadioControlServiceComponent sc = *scIt;
                        QTimer::singleShot(1000, this, [this, sc](){
                            emit announcement(m_currentService.announcement.id, RadioControlAnnouncementState::OnCurrentService, sc);
                        });
                    }
                }
                else
                {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                    m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                }
            }
            else
            { /* subchannel not found -> ignoring */ }
        }
        else
        { /* invalid or not enabled for current service */ }
    }
    break;
    case RadioControlAnnouncementState::OnCurrentService:
    {   // some announcement is ongoing
        if (m_currentService.announcement.activeCluster == pAnnouncement->clusterId)
        {   // this specific announcement cluster is currently active
            if (0 == (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // end of announcement
                // the expression evaluates to 0 if:
                //      pAnnouncement->ASwFlags == 0  (end of announcment)
                // or
                //      pAnnouncement->ASwFlags & m_announcementEnaFlags == 0 (disabled announcment)

                // notify HMI
                serviceComponentIterator scIt;
                if (getCurrentAudioServiceComponent(scIt))
                {   // delay to compensate audio delay somehow
                    // Inform HMI -> announcement on current service
                    RadioControlServiceComponent sc = *scIt;
                    QTimer::singleShot(1000, this, [this, sc](){
                        emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, sc);
                    });
                }
                m_currentService.announcement.timeoutTimer->stop();
                m_currentService.announcement.activeCluster = 0;
                m_currentService.announcement.state = RadioControlAnnouncementState::None;
                m_currentService.announcement.id = DabAnnouncement::Undefined;
            }
            else
            {   // announcement continues - restart timer
                // ETSI TS 103 176 V2.4.1 [7.2.5]
                // the value of the ASw flags field - shall not change during an announcement
                // ==> ignoring any potential change
                m_currentService.announcement.timeoutTimer->start();
            }
        }
        else
        {   /* During the announcement the receiver shall monitor ASw information for only the active Cluster Id */
            // monitor for alarm announcement
            DabAnnouncement announcementId = static_cast<DabAnnouncement>(DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags));
            if ((DabAnnouncement::Alarm == announcementId) && (0xFE == pAnnouncement->clusterId))
            {   // this is test mode
                // ETSI TS 103 176 V2.4.1 [Annex G]
                announcementId = DabAnnouncement::AlarmTest;
            }
            if ((DabAnnouncement::Alarm == announcementId) && (0xFF != pAnnouncement->clusterId))
            {   // ETSI TS 103 176 V2.4.1 [7.3.4]
                // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
                // the ASw field shall be ignored.
                return;
            }
            if (((DabAnnouncement::Alarm == announcementId) || (DabAnnouncement::AlarmTest == announcementId))
                && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // ETSI TS 103 176 V2.4.1 [7.6.3.2]
                // The response to alarm announcements shall not be subject to any user setting;
                // receivers shall switch to the target subchannel unconditionally,
                // any active regular announcements shall be terminated.

                // DabAnnouncement::Alarm or enabled DabAnnouncement::AlarmTest
                //qDebug() << "OnCurrentService - alarm";

                // stopping currnet announcement
                stopAnnouncement();
                // HMI will be notified when audio switches

                m_currentService.announcement.suspendRequest = false;
                if (startAnnouncement(pAnnouncement->subChId))
                {   // found subchannel
                    m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                    m_currentService.announcement.timeoutTimer->start();
                    m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                    if (!m_currentService.announcement.isOtherService)
                    {   // announcement in current service
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;
                    }
                    else
                    {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                    }
                }
                else
                { /* subchannel not found -> ignoring */ }
            }
        }
    }
    break;
    case RadioControlAnnouncementState::OnOtherService:
    {   // some announcement is ongoing
        if (m_currentService.announcement.activeCluster == pAnnouncement->clusterId)
        {   // this specific announcement cluster is currently active
            if (0 == (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // end of announcement
                // the expression evaluates to 0 if:
                //      pAnnouncement->ASwFlags == 0  (end of announcment)
                // or
                //      pAnnouncement->ASwFlags & m_announcementEnaFlags == 0 (disabled announcment)

                // stops announcement audio, does nothing announcement is on current service or suspended
                stopAnnouncement();
                // HMI will be notified when audio switches

                m_currentService.announcement.timeoutTimer->stop();
                m_currentService.announcement.activeCluster = 0;
                m_currentService.announcement.state = RadioControlAnnouncementState::None;
                m_currentService.announcement.id = DabAnnouncement::Undefined;
            }
            else
            {   // announcement continues - restart timer
                // ETSI TS 103 176 V2.4.1 [7.2.5]
                // the value of the ASw flags field - shall not change during an announcement
                // ==> ignoring any potential change
                m_currentService.announcement.timeoutTimer->start();

                // we can suspend announcement here
                if (m_currentService.announcement.suspendRequest)
                {
                    m_currentService.announcement.state = RadioControlAnnouncementState::Suspended;

                    // stops announcement audio, does nothing announcement is on current service or suspended
                    stopAnnouncement();
                    // HMI will be notified when audio switches
                }
            }
        }
        else
        {   /* During the announcement the receiver shall monitor ASw information for only the active Cluster Id */
            // monitor for alarm announcement
            DabAnnouncement announcementId = static_cast<DabAnnouncement>(DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags));
            if ((DabAnnouncement::Alarm == announcementId) && (0xFE == pAnnouncement->clusterId))
            {   // this is test mode
                // ETSI TS 103 176 V2.4.1 [Annex G]
                announcementId = DabAnnouncement::AlarmTest;
            }
            if ((DabAnnouncement::Alarm == announcementId) && (0xFF != pAnnouncement->clusterId))
            {   // ETSI TS 103 176 V2.4.1 [7.3.4]
                // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
                // the ASw field shall be ignored.
                return;
            }
            if (((DabAnnouncement::Alarm == announcementId) || (DabAnnouncement::AlarmTest == announcementId))
                && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // ETSI TS 103 176 V2.4.1 [7.6.3.2]                
                // The response to alarm announcements shall not be subject to any user setting;
                // receivers shall switch to the target subchannel unconditionally,
                // any active regular announcements shall be terminated.

                // DabAnnouncement::Alarm or enabled DabAnnouncement::AlarmTest
                //qDebug() << "OnOtherService - alarm";

                // stopping currnet announcement
                stopAnnouncement();
                // HMI will be notified when audio switches

                m_currentService.announcement.suspendRequest = false;
                if (startAnnouncement(pAnnouncement->subChId))
                {   // found subchannel
                    m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                    m_currentService.announcement.timeoutTimer->start();
                    m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                    if (!m_currentService.announcement.isOtherService)
                    {   // announcement in current service
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;
                    }
                    else
                    {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                    }
                }
                else
                { /* subchannel not found -> ignoring */ }
            }
        }
    }
    break;
    case RadioControlAnnouncementState::Suspended:
    {   // some announcement is ongoing but it is supended
        if (m_currentService.announcement.activeCluster == pAnnouncement->clusterId)
        {   // this specific announcement cluster is currently active
            if (0 == (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // end of announcement
                // the expression evaluates to 0 if:
                //      pAnnouncement->ASwFlags == 0  (end of announcment)
                // or
                //      pAnnouncement->ASwFlags & m_announcementEnaFlags == 0 (disabled announcment)

                serviceConstIterator sIt = m_serviceList.constFind(m_currentService.announcement.SId);
                if (m_serviceList.cend() != sIt)
                {   // found
                    serviceComponentConstIterator scIt = sIt->serviceComponents.constFind(m_currentService.announcement.SCIdS);
                    if (sIt->serviceComponents.cend() != scIt)
                    {   // found
                        emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, *scIt);
                    }
                }

                m_currentService.announcement.timeoutTimer->stop();
                m_currentService.announcement.activeCluster = 0;
                m_currentService.announcement.state = RadioControlAnnouncementState::None;
                m_currentService.announcement.id = DabAnnouncement::Undefined;
            }
            else
            {   // announcement continues - restart timer
                // ETSI TS 103 176 V2.4.1 [7.2.5]
                // the value of the ASw flags field - shall not change during an announcement
                // ==> ignoring any potential change
                m_currentService.announcement.timeoutTimer->start();

                // we can suspend announcement here
                if (!m_currentService.announcement.suspendRequest)
                {
                    // start announcement
                    startAnnouncement(pAnnouncement->subChId);
                    // HMI will be notified when audio switches

                    m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                }
            }
        }
        else
        {   /* During the announcement the receiver shall monitor ASw information for only the active Cluster Id */
            // monitor for alarm announcement
            DabAnnouncement announcementId = static_cast<DabAnnouncement>(DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags));
            if ((DabAnnouncement::Alarm == announcementId) && (0xFE == pAnnouncement->clusterId))
            {   // this is test mode
                // ETSI TS 103 176 V2.4.1 [Annex G]
                announcementId = DabAnnouncement::AlarmTest;
            }
            if ((DabAnnouncement::Alarm == announcementId) && (0xFF != pAnnouncement->clusterId))
            {   // ETSI TS 103 176 V2.4.1 [7.3.4]
                // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
                // the ASw field shall be ignored.
                return;
            }
            if (((DabAnnouncement::Alarm == announcementId) || (DabAnnouncement::AlarmTest == announcementId))
                && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // ETSI TS 103 176 V2.4.1 [7.6.3.2]
                // The response to alarm announcements shall not be subject to any user setting;
                // receivers shall switch to the target subchannel unconditionally,
                // any active regular announcements shall be terminated.

                // DabAnnouncement::Alarm or enabled DabAnnouncement::AlarmTest
                //qDebug() << "Suspended - alarm";

                // stopping currnet announcement
                stopAnnouncement();
                // HMI will be notified when audio switches

                m_currentService.announcement.suspendRequest = false;
                if (startAnnouncement(pAnnouncement->subChId))
                {   // found subchannel
                    m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                    m_currentService.announcement.timeoutTimer->start();
                    m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                    if (!m_currentService.announcement.isOtherService)
                    {   // announcement in current service
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;

                        //                        // delay to compensate audio delay somehow
                        //                        serviceComponentIterator scIt;
                        //                        if (getCurrentAudioServiceComponent(scIt))
                        //                        {
                        //                            // Inform HMI -> announcement on current service
                        //                            RadioControlServiceComponent sc = *scIt;
                        //                            QTimer::singleShot(1000, this, [this, sc](){
                        //                                emit announcement(m_currentService.announcement.id, RadioControlAnnouncementState::OnCurrentService, sc);
                        //                            });
                        //                        }
                    }
                    else
                    {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                    }
                }
                else
                { /* subchannel not found -> ignoring */ }
            }
        }
    }
    break;
    }
#else
    if (RadioControlAnnouncementState::None == m_currentService.announcement.state)
    {   // no announcement is active -> let check if announcement belong to current service
        //
        // ETSI TS 103 176 V2.4.1 [7.4.3]
        // The ASw flags field shall have one bit (bit flags b1 to b15) set to 1
        // corresponding to the announcement type while the announcement is active
        // ...
        // An ASw flag field with more than one bit flag set to 1 is invalid and shall be ignored.
        int announcementId = DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags);
        if ((static_cast<int>(DabAnnouncement::Alarm) == announcementId) && (0xFE == pAnnouncement->clusterId))
        {   // this is test mode
            // ETSI TS 103 176 V2.4.1 [Annex G]
            announcementId = static_cast<int>(DabAnnouncement::AlarmTest);
        }
        if ((static_cast<int>(DabAnnouncement::Alarm) == announcementId) && (0xFF != pAnnouncement->clusterId))
        {   // ETSI TS 103 176 V2.4.1 [7.3.4]
            // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
            // the ASw field shall be ignored.
            return;
        }
        if ((announcementId >= 0) && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
        {   // valid ASw
    #if 1 // RADIO_CONTROL_VERBOSE > 1
            qDebug() << DabTables::getAnnouncementName(static_cast<DabAnnouncement>(announcementId))
                     << "announcement in subchannel" <<  pAnnouncement->subChId
                     << "cluster ID" << pAnnouncement->clusterId;
    #endif
            m_currentService.announcement.suspendRequest = false;
            if (startAnnouncement(pAnnouncement->subChId))
            {   // found subchannel
                m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                m_currentService.announcement.timeoutTimer->start();
                m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                if (!m_currentService.announcement.isOtherService)
                {   // announcement in current service
                    m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;

                    // delay to compensate audio delay somehow
                    serviceComponentIterator scIt;
                    if (getCurrentAudioServiceComponent(scIt))
                    {
                        // Inform HMI -> announcement on current service
                        RadioControlServiceComponent sc = *scIt;
                        QTimer::singleShot(1000, this, [this, sc](){
                            emit announcement(m_currentService.announcement.id, RadioControlAnnouncementState::OnCurrentService, sc);
                        });
                    }
                }
                else
                {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                    m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                }
            }
            else
            { /* subchannel not found -> ignoring */ }
        }
        else
        { /* invalid or not enabled for current service */ }
    }
    else
    {   // some announcement is ongoing
        if (m_currentService.announcement.activeCluster == pAnnouncement->clusterId)
        {   // this specific announcement cluster is currently active
            if (0 == (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // end of announcement
                // the expression evaluates to 0 if:
                //      pAnnouncement->ASwFlags == 0  (end of announcment)
                // or
                //      pAnnouncement->ASwFlags & m_announcementEnaFlags == 0 (disabled announcment)

                switch (m_currentService.announcement.state)
                {
                case RadioControlAnnouncementState::OnCurrentService:
                {
                    // notify HMI
                    serviceComponentIterator scIt;
                    if (getCurrentAudioServiceComponent(scIt))
                    {   // delay to compensate audio delay somehow
                        // Inform HMI -> announcement on current service
                        RadioControlServiceComponent sc = *scIt;
                        QTimer::singleShot(1000, this, [this, sc](){
                            emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, sc);
                        });
                    }
                }
                    break;
                case RadioControlAnnouncementState::Suspended:
                {
                    serviceConstIterator sIt = m_serviceList.constFind(m_currentService.announcement.SId);
                    if (m_serviceList.cend() != sIt)
                    {   // found
                        serviceComponentConstIterator scIt = sIt->serviceComponents.constFind(m_currentService.announcement.SCIdS);
                        if (sIt->serviceComponents.cend() != scIt)
                        {   // found
                            emit announcement(DabAnnouncement::Undefined, RadioControlAnnouncementState::None, *scIt);
                        }
                    }
                }
                    break;
                default:
                    // OnOtherService, None is not possible here

                    // stops announcement audio, does nothing announcement is on current service or suspended
                    stopAnnouncement();
                    // HMI will be notified when audio switches
                    break;
                }

                m_currentService.announcement.timeoutTimer->stop();
                m_currentService.announcement.activeCluster = 0;
                m_currentService.announcement.state = RadioControlAnnouncementState::None;
                m_currentService.announcement.id = DabAnnouncement::Undefined;

            }
            else
            {   // announcement continues - restart timer
                // ETSI TS 103 176 V2.4.1 [7.2.5]
                // the value of the ASw flags field - shall not change during an announcement
                // ==> ignoring any potential change
                m_currentService.announcement.timeoutTimer->start();

                // we can suspend announcement here
                switch (m_currentService.announcement.state)
                {
                case RadioControlAnnouncementState::OnOtherService:
                    // announcement is on other service
                    if (m_currentService.announcement.suspendRequest)
                    {   // suspend
                        m_currentService.announcement.state = RadioControlAnnouncementState::Suspended;

                        // stops announcement audio, does nothing announcement is on current service or suspended
                        stopAnnouncement();
                        // HMI will be notified when audio switches
                    }
                    break;
                case RadioControlAnnouncementState::Suspended:
                    // OnOtherService but suspended
                    if (!m_currentService.announcement.suspendRequest)
                    {   // resume
                        // start announcement
                        startAnnouncement(pAnnouncement->subChId);
                        // HMI will be notified when audio switches

                        m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                    }
                    break;
                default:
                    // do nothing
                    break;
                }
            }
        }
        else
        {   // During the announcement the receiver shall monitor ASw information for only the active Cluster Id
            // monitor for alarm announcement
            DabAnnouncement announcementId = static_cast<DabAnnouncement>(DabTables::ASwValues.indexOf(pAnnouncement->ASwFlags));
            if ((DabAnnouncement::Alarm == announcementId) && (0xFE == pAnnouncement->clusterId))
            {   // this is test mode
                // ETSI TS 103 176 V2.4.1 [Annex G]
                announcementId = DabAnnouncement::AlarmTest;
            }
            if ((DabAnnouncement::Alarm == announcementId) && (0xFF != pAnnouncement->clusterId))
            {   // ETSI TS 103 176 V2.4.1 [7.3.4]
                // If the ASw flags field has set bit b0 to 1 with a Cluster Id or OE Cluster Id other than "1111 1111" (0xFF),
                // the ASw field shall be ignored.
                return;
            }
            if (((DabAnnouncement::Alarm == announcementId) || (DabAnnouncement::AlarmTest == announcementId))
                && (pAnnouncement->ASwFlags & m_currentService.announcement.enaFlags))
            {   // ETSI TS 103 176 V2.4.1 [7.6.3.2]
                // The response to alarm announcements shall not be subject to any user setting;
                // receivers shall switch to the target subchannel unconditionally,
                // any active regular announcements shall be terminated.

                // DabAnnouncement::Alarm or enabled DabAnnouncement::AlarmTest

                // stopping current announcement
                stopAnnouncement();
                // HMI will be notified when audio switches

                m_currentService.announcement.suspendRequest = false;
                if (startAnnouncement(pAnnouncement->subChId))
                {   // found subchannel
                    m_currentService.announcement.activeCluster = pAnnouncement->clusterId;
                    m_currentService.announcement.timeoutTimer->start();
                    m_currentService.announcement.id = static_cast<DabAnnouncement>(announcementId);
                    if (m_currentService.announcement.isOtherService)
                    {   /* signal will be sent when audio changes from onAudioOutputRestart() */
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnOtherService;
                    }
                    else
                    {   // announcement in current service
                        m_currentService.announcement.state = RadioControlAnnouncementState::OnCurrentService;
                    }
                }
                else
                { /* subchannel not found -> ignoring */ }
            }
        }
    }
#endif
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
        pEvent->syncLevel = pInfo->syncLevel;
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
        pEvent->frequency = static_cast<uint32_t>(*((uint32_t*) p->pData));
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_ENSEMBLE_INFO:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABSDR_NID_ENSEMBLE_INFO: status %d", p->status);
#endif
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::ENSEMBLE_INFO;
        pEvent->status = p->status;        
        pEvent->pEnsembleInfo = new dabsdrNtfEnsemble_t;
        memcpy(pEvent->pEnsembleInfo, p->pData, sizeof(dabsdrNtfEnsemble_t));
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
        pEvent->pServiceList = pServiceList;
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
            pEvent->SId = pInfo->SId;
            pEvent->type = RadioControlEventType::SERVICE_COMPONENT_LIST;
            pEvent->status = p->status;
            pEvent->pServiceCompList = pList;
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X not found", pInfo->SId);
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
            pEvent->SId = pInfo->SId;
            pEvent->SCIdS = pInfo->SCIdS;
            pEvent->pUserAppList = pList;
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
        const dabsdrNtfServiceSelection_t * pInfo = (const dabsdrNtfServiceSelection_t * ) p->pData;

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_SELECTION;

        pEvent->status = p->status;
        pEvent->SId = pInfo->SId;
        pEvent->SCIdS = pInfo->SCIdS;
        pEvent->decoderId = pInfo->id;
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_SERVICE_STOP:
    {
        const dabsdrNtfServiceStop_t * pInfo = (const dabsdrNtfServiceStop_t * ) p->pData;

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_STOP;

        pEvent->status = p->status;
        pEvent->SId = pInfo->SId;
        pEvent->SCIdS = pInfo->SCIdS;
        pEvent->decoderId = pInfo->id;
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_XPAD_APP_START_STOP:
    {
        dabsdrNtfXpadAppStartStop_t * pServStopInfo = new dabsdrNtfXpadAppStartStop_t;
        memcpy(pServStopInfo, p->pData, sizeof(dabsdrNtfXpadAppStartStop_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::XPAD_APP_START_STOP;

        pEvent->status = p->status;
        pEvent->pXpadAppStartStopInfo = pServStopInfo;
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_PERIODIC:
    {
        if (p->pData)
        {
            assert(sizeof(dabsdrNtfPeriodic_t) == p->len);

            dabsdrNtfPeriodic_t * pNotifyData = new dabsdrNtfPeriodic_t;
            memcpy((uint8_t*) pNotifyData, p->pData, p->len);

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::AUTO_NOTIFICATION;
            pEvent->status = p->status;
            pEvent->pNotifyData = pNotifyData;
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
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_RESET:
    {
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::RESET;

        pEvent->status = p->status;
        pEvent->resetFlag = static_cast<dabsdrNtfResetFlags_t>(*((dabsdrNtfResetFlags_t*) p->pData));
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_ANNOUNCEMENT_SUPPORT:
    {
        dabsdrNtfAnnouncementSupport_t * pAnnouncementSupport = new dabsdrNtfAnnouncementSupport_t;
        memcpy(pAnnouncementSupport, p->pData, sizeof(dabsdrNtfAnnouncementSupport_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::ANNOUNCEMENT_SUPPORT;
        pEvent->status = p->status;
        pEvent->SId = pAnnouncementSupport->SId;
        pEvent->pAnnouncementSupport = pAnnouncementSupport;
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABSDR_NID_ANNOUNCEMENT_SWITCHING:
    {
        dabsdrNtfAnnouncementSwitching_t * pAnnouncement = new dabsdrNtfAnnouncementSwitching_t;
        memcpy(pAnnouncement, p->pData, sizeof(dabsdrNtfAnnouncementSwitching_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::ANNOUNCEMENT_SWITCHING;
        pEvent->status = p->status;
        pEvent->pAnnouncement = pAnnouncement;
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

    RadioControlEvent * pEvent = new RadioControlEvent;
    pEvent->type = RadioControlEventType::DATAGROUP_DL;
    pEvent->status = DABSDR_NSTAT_SUCCESS;
    RadioControlDataDL * pDynamicLabelData = new RadioControlDataDL;
    pDynamicLabelData->id = p->id;
    pDynamicLabelData->data.append((const char *)p->pData, p->len);
    pEvent->pDynamicLabelData = pDynamicLabelData;
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
    pData->id = p->id;
    // copy data to QByteArray
    pData->data = QByteArray((const char *)p->pDgData, p->dgLen);

    RadioControlEvent * pEvent = new RadioControlEvent;
    pEvent->type = RadioControlEventType::USERAPP_DATA;
    pEvent->status = DABSDR_NSTAT_SUCCESS;
    pEvent->pUserAppData = pData;
    radioCtrl->emit_dabEvent(pEvent);        
}

void RadioControl::audioDataCb(dabsdrAudioCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = static_cast<RadioControl *>(ctx);

    switch (radioCtrl->m_currentService.announcement.switchState)
    {
    case AnnouncementSwitchState::NoAnnouncement:
    {   // no ennouncement ongoing
        if (DABSDR_ID_AUDIO_PRIMARY == p->id)
        {
            RadioControlAudioData * pAudioData = new RadioControlAudioData;
            pAudioData->id = p->id;
            pAudioData->ASCTy = static_cast<DabAudioDataSCty>(p->ASCTy);
            pAudioData->header = p->header;
            pAudioData->data.assign(p->pAuData, p->pAuData+p->auLen);

            radioCtrl->emit_audioData(pAudioData);
        }
        else
        {
            //qDebug() << "Ignoring announcement audio data";
        }
    }
        return;
    case AnnouncementSwitchState::WaitForAnnouncement:
    {   // announcement expected
        RadioControlAudioData * pAudioData = new RadioControlAudioData;
        pAudioData->id = p->id;
        pAudioData->ASCTy = static_cast<DabAudioDataSCty>(p->ASCTy);
        pAudioData->header = p->header;
        pAudioData->data.assign(p->pAuData, p->pAuData+p->auLen);

        radioCtrl->emit_audioData(pAudioData);
        if (DABSDR_ID_AUDIO_SECONDARY == p->id)
        {   // first announcement data increment value
            radioCtrl->emit_announcementAudioAvailable();
        }
        else { /* normal service data */ }
    }
    default:
    {   //
        if (DABSDR_ID_AUDIO_SECONDARY == p->id)
        {
            RadioControlAudioData * pAudioData = new RadioControlAudioData;
            pAudioData->id = p->id;
            pAudioData->ASCTy = static_cast<DabAudioDataSCty>(p->ASCTy);
            pAudioData->header = p->header;
            pAudioData->data.assign(p->pAuData, p->pAuData+p->auLen);

            radioCtrl->emit_audioData(pAudioData);
        }
        else
        {
            //qDebug() << "Ignoring non-announcement audio data";
        }
    }
    }
}
