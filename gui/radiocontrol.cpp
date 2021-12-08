#include <QThread>
#include <QDate>
#include <QTime>

#include <QDebug>

#include "radiocontrol.h"
#include "dldecoder.h"

const uint8_t RadioControl::EEPCoderate[] =
{ // ETSI EN 300 401 V2.1.1 [6.2.1 Basic sub-channel organization] table 9 & 10
    0x14, 0x38, 0x12, 0x34,   // EEP 1-A..4-A : 1/4 3/8 1/2 3/4
    0x49, 0x47, 0x46, 0x45    // EEP 1-B..4-B : 4/9 4/7 4/6 4/5
};


void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx);
void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx);
void audioDataCb(dabProcAudioCBData_t * p, void * ctx);

RadioControl::RadioControl(QObject *parent) : QObject(parent)
{
    dabProcHandle = NULL;
    frequency = 0;
    serviceList.clear();
    serviceRequest.SId = serviceRequest.SCIdS = 0;

    connect(this, &RadioControl::dabEvent, this, &RadioControl::eventFromDab, Qt::QueuedConnection);
}

RadioControl::~RadioControl()
{   // this cancels dabProc thread
    dabProcDeinit(&dabProcHandle);
}

// returns false if not successfull
bool RadioControl::init()
{
    if (EXIT_SUCCESS == dabProcInit(&dabProcHandle))
    {
        dabProcRegisterInputFcn(dabProcHandle, getSamples);
        dabProcRegisterNotificationCb(dabProcHandle, dabNotificationCb, (void *) this);
        dabProcRegisterDataGroupCb(dabProcHandle, dataGroupCb, (void*) this);
        dabProcRegisterAudioCb(dabProcHandle, audioDataCb, this);
        // this starts dab processing thread and retunrs
        dabProc(dabProcHandle);
    }
    else
    {
        qDebug() << "DAB processing init failed";
        dabProcHandle = NULL;

        return false;
    }

    return true;
}

void RadioControl::eventFromDab(RadioControlEvent * pEvent)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId() << pEvent->type;
    switch (pEvent->type)
    {
    case RadioControlEventType::SYNC_STATUS:
    {
        dabProcSyncLevel_t s = static_cast<dabProcSyncLevel_t>(pEvent->pData);
#if RADIO_CONTROL_VERBOSE
        qDebug("Sync = %d", s);
#endif

        if ((DABPROC_SYNC_LEVEL_FIC == s) && (RADIO_CONTROL_UEID_INVALID == ensemble.ueid))
        { // ask for ensemble info if not available
            dabGetEnsembleInfo();
        }
        updateSyncLevel(s);
    }
        break;
    case RadioControlEventType::TUNE:
    {
        if (DABPROC_NSTAT_SUCCESS == pEvent->status)
        {
            uint32_t freq = uint32_t(pEvent->pData);
#if RADIO_CONTROL_VERBOSE > 1
            qDebug() << "Tune success" << freq;
#endif
            if ((freq != frequency) || (0 == frequency))
            {   // in first step, DAB is tuned to 0 that is != desired frequency
                // or if we want to tune to 0 by purpose (tgo to IDLE)
                // this tunes input device to desired frequency
                emit tuneInputDevice(frequency);
            }
            else
            {   // tune is finished , notify HMI
                emit tuneDone(freq);
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
        dabProc_NID_ENSEMBLE_INFO_t * pInfo = (dabProc_NID_ENSEMBLE_INFO_t *) pEvent->pData;
        if (((pInfo->ueid & 0x00FF0000) != 0) && pInfo->label.str[0] != '\0')
        {
            serviceList.clear();
            ensemble.ueid = pInfo->ueid;
            ensemble.frequency = pInfo->frequency;
            ensemble.LTO = pInfo->LTO;
            ensemble.intTable = pInfo->intTable;
            ensemble.alarm = pInfo->alarm;
            ensemble.label = DabTables::convertToQString(pInfo->label.str, pInfo->label.charset).trimmed();
            ensemble.labelShort = toShortLabel(ensemble.label, pInfo->label.charField).trimmed();
            emit ensembleInformation(ensemble);
            // request service list
            dabGetServiceList();
        }

        delete (dabProc_NID_ENSEMBLE_INFO_t *) pEvent->pData;

        if ((RADIO_CONTROL_UEID_INVALID == ensemble.ueid) && (DABPROC_SYNC_LEVEL_FIC == sync))
        {   // valid ensemble sill not received
            // wait some time and send new request
            QTimer::singleShot(200, this, &RadioControl::dabGetEnsembleInfo);
        }
    }
        break;
    case RadioControlEventType::SERVICE_LIST:
    {
#if 1 // RADIO_CONTROL_VERBOSE
        qDebug() << "RadioControlEvent::SERVICE_LIST";
#endif
        QList<dabProcServiceListItem_t> * pServiceList = (QList<dabProcServiceListItem_t> *) pEvent->pData;
        if (0 == pServiceList->size())
        {   // no service list received (invalid probably)
            serviceList.clear();

            // send new request after some timeout
            QTimer::singleShot(100, this, &RadioControl::dabGetServiceList);
        }
        else
        {
            requestsPending = 0;
            for (auto const & dabService : *pServiceList)
            {
                QList<RadioControlService>::iterator servIt = findService(DabSId(dabService.sid));
                if (servIt != serviceList.end())
                {   // delete existing service
                    serviceList.erase(servIt);
                }
                RadioControlService newService;
                newService.SId.value = dabService.sid;
                newService.label = DabTables::convertToQString(dabService.label.str, dabService.label.charset).trimmed();
                newService.labelShort = toShortLabel(newService.label, dabService.label.charField).trimmed();
                newService.pty.s = dabService.pty.s;
                newService.pty.d = dabService.pty.d;
                newService.CAId = dabService.CAId;
                serviceList.append(newService);
                requestsPending++;
                dabGetServiceComponent(dabService.sid);
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

        QList<dabProcServiceCompListItem_t> * pList = (QList<dabProcServiceCompListItem_t> *) pEvent->pData;
        if (!pList->isEmpty())
        {   // all service components belong to the same SId, reading sid from the first
            DabSId sid(pList->at(0).SId);

            // find service ID
            QList<RadioControlService>::iterator serviceIt = findService(sid);
            if (serviceIt != serviceList.end())
            {   // SId found
                serviceIt->serviceComponents.clear();

                bool requestUpdate = false;
                for (auto const & dabServiceComp : *pList)
                {
                    // first checking validity of data
                    if ((dabServiceComp.SubChAddr < 0)
                            || ((!dabServiceComp.ps) && (dabServiceComp.label.str[0] == '\0'))
                            || ((DabTMId::PacketData == DabTMId(dabServiceComp.TMId)) && (dabServiceComp.packetData.packetAddress < 0)))
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
                    newServiceComp.CAflag = dabServiceComp.CAflag;
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
                        newServiceComp.packetData.DGflag = dabServiceComp.packetData.DGflag;
                        newServiceComp.packetData.DSCTy = DabAudioDataSCty(dabServiceComp.packetData.DSCTy);
                        newServiceComp.packetData.SCId = dabServiceComp.packetData.SCId;
                        newServiceComp.packetData.packetAddress = dabServiceComp.packetData.packetAddress;
                        break;
                    }
                    serviceIt->serviceComponents.append(newServiceComp);

                    if ((serviceRequest.SId == serviceIt->SId.value) && (serviceRequest.SCIdS == newServiceComp.SCIdS))
                    {
                        dabServiceSelection(serviceRequest.SId, serviceRequest.SCIdS);
                        serviceRequest.SId = 0;    // clear request
                    }

                    emit serviceListEntry(ensemble, newServiceComp);
                }
                if (requestUpdate)
                {
                    serviceIt->serviceComponents.clear();
                    dabGetServiceComponent(sid.value);
                }
                else
                {  // service list item information is complete
                   requestsPending--;
                   for (auto const & serviceComp : serviceIt->serviceComponents)
                   {
                       requestsPending++;
                       dabGetUserApps(serviceIt->SId.value, serviceComp.SCIdS);
                   }
                }
            }
            else
            {  // SId not found
                qDebug("Service SID %4.4X not in service list", sid.value);
            }
        }

        delete pList;
    }
        break;
    case RadioControlEventType::USER_APP_LIST:
    {
        QList<dabProcUserAppListItem_t> * pList = (QList<dabProcUserAppListItem_t> *) pEvent->pData;
        if (!pList->isEmpty())
        {   // all user apps belong to the same SId, reading sid from the first
            DabSId sid(pList->at(0).SId);

            // find service ID
            QList<RadioControlService>::iterator serviceIt = findService(sid);
            if (serviceIt != serviceList.end())
            {   // SId found
                // all user apps belong to the same SId+SCIdS, reading SCIdS from the first
                QList<RadioControlServiceComponent>::iterator scIt = findServiceComponent(serviceIt, pList->at(0).SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service component found
                    scIt->userApps.clear();
                    for (auto const & userApp : *pList)
                    {
                        RadioControlUserApp newUserApp;
                        newUserApp.label = DabTables::convertToQString(userApp.label.str, userApp.label.charset).trimmed();
                        newUserApp.labelShort = toShortLabel(newUserApp.label, userApp.label.charField).trimmed();
                        newUserApp.uaType = userApp.type;
                        for (int n = 0; n < userApp.dataLen; ++n)
                        {
                            newUserApp.uaData.append(userApp.data[n]);
                        }
                        newUserApp.xpadData.value = 0;
                        if ((scIt->isAudioService()) && (userApp.dataLen >= 2))
                        {
                            newUserApp.xpadData.value = (userApp.data[0] << 8) | userApp.data[1];
                        }

                        scIt->userApps.append(newUserApp);                        
                        //qDebug() << serviceIt->SId.value << scIt->SCIdS << newUserApp.uaType;
                    }
                    if (--requestsPending == 0)
                    {
                        //qDebug() << "=== MCI complete";
                        emit ensembleConfiguration(ensembleConfigurationString());
                    }
                }                
            }
        }
        delete pList;
    }
        break;
    case RadioControlEventType::SERVICE_SELECTION:
    {
        dabProc_NID_SERVICE_SELECTION_t * pData = (dabProc_NID_SERVICE_SELECTION_t *) pEvent->pData;
        if (DABPROC_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RadioControlEvent::SERVICE_SELECTION success";
#endif
            emit serviceChanged();

            QList<RadioControlService>::iterator serviceIt = findService(pData->SId);
            if (serviceIt != serviceList.end())
            {   // service is in the list
                QList<RadioControlServiceComponent>::iterator scIt = findServiceComponent(serviceIt, pData->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service components exists in service
                    emit newServiceSelection(*scIt);
                }

            }
        }        
        else
        {
            qDebug() << "RadioControlEvent::SERVICE_SELECTION error" << pEvent->status;
        }
        delete pData;
    }
        break;
    case RadioControlEventType::AUTO_NOTIFICATION:
    {
        dabProc_NID_AUTO_NOTIFY_t * pData = (dabProc_NID_AUTO_NOTIFY_t *) pEvent->pData;

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

            QDateTime dateAndTime = QDateTime(QDate(Y, M, D), QTime(h, minute, sec, msec), Qt::UTC).toOffsetFromUtc(60*(ensemble.LTO * 30));

            //qDebug() << dateAndTime;

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
        QByteArray * pData = (QByteArray *) pEvent->pData;
        emit dlDataGroup(*pData);
        delete pData;
    }
        break;
    case RadioControlEventType::DATAGROUP_MSC:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RadioControlEvent::DATAGROUP_MSC";
#endif
        QByteArray * pData = (QByteArray *) pEvent->pData;
        emit mscDataGroup(*pData);
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
#if RADIO_CONTROL_VERBOSE
    qDebug() << Q_FUNC_INFO;
#endif
    dabProcRequest_Exit(dabProcHandle);
}

void RadioControl::start(uint32_t freq)
{
    if (freq)
    {   // when input device tuned, freq is passed to be set in SDR
        // when frequency is 0 then we are done (input device is in idle)
        frequency = freq;
        sync = DABPROC_SYNC_LEVEL_NO_SYNC;
        emit syncStatus(uint8_t(DabSyncLevel::NoSync));
        serviceList.clear();
        ensemble.ueid = RADIO_CONTROL_UEID_INVALID;
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
    qDebug() << Q_FUNC_INFO << freq << frequency << SId << SCIdS;
    if (freq == frequency)
    {
        if (SId)
        {   // clear request
            serviceRequest.SId = 0;
            dabServiceSelection(SId, SCIdS);
        }
    }
    else
    {   // TODO: service selection
        autoNotificationEna = false;
        frequency = freq;
        serviceRequest.SId = SId;
        serviceRequest.SCIdS = SCIdS;
        emit ensembleConfiguration("");

        dabTune(0);
     }
}

void RadioControl::updateSyncLevel(dabProcSyncLevel_t s)
{
    if (s != sync)
    {   // new sync level => emit signal
        sync = s;
        switch (sync)
        {
        case DABPROC_SYNC_LEVEL_NO_SYNC:            
            emit syncStatus(uint8_t(DabSyncLevel::NoSync));
            emit snrLevel(0);
            break;
        case DABPROC_SYNC_LEVEL_ON_NULL:
            emit syncStatus(uint8_t(DabSyncLevel::NullSync));
            break;
        case DABPROC_SYNC_LEVEL_FIC:
            emit syncStatus(uint8_t(DabSyncLevel::FullSync));
            break;
        }
    }
    if ((sync > DABPROC_SYNC_LEVEL_NO_SYNC) && (!autoNotificationEna))
    {
        // enable auto notifications
        dabEnableAutoNotification();
        autoNotificationEna = true;
    }
}

QList<RadioControlService>::iterator RadioControl::findService(DabSId SId)
{
    QList<RadioControlService>::iterator serviceIt;
    for (serviceIt = serviceList.begin(); serviceIt < serviceList.end(); ++serviceIt)
    {
        if (SId.value ==  serviceIt->SId.value)
        {  // found SId
            return serviceIt;
        }
    }
    return serviceIt;
}

QList<RadioControlServiceComponent>::iterator
                RadioControl::findServiceComponent(const QList<RadioControlService>::iterator & sIt, uint8_t SCIdS)
{
    QList<RadioControlServiceComponent>::iterator scIt;
    for (scIt = sIt->serviceComponents.begin(); scIt < sIt->serviceComponents.end(); ++scIt)
    {
        if (SCIdS == scIt->SCIdS)
        {
            return scIt;
        }
    }
    return scIt;
}

void RadioControl::getEnsembleConfiguration()
{
    emit ensembleConfiguration(ensembleConfigurationString());
}

QString RadioControl::ensembleConfigurationString() const
{
    QString output;
    QTextStream strOut(&output, QIODevice::Text);

    strOut << "<dl>";
    strOut << "<dt>Ensemble:</dt>";
    strOut << QString("<dd>0x%1 <b>%2</b> [ <i>%3</i> ]  ECC = 0x%4, UTC %5 min, INT = %6, alarm announcements = %7</dd>")
              .arg(QString("%1").arg(ensemble.eid, 4, 16, QChar('0')).toUpper())
              .arg(ensemble.label)
              .arg(ensemble.labelShort)
              .arg(QString("%1").arg(ensemble.ecc, 2, 16, QChar('0')).toUpper())
              .arg(ensemble.LTO*30)
              .arg(ensemble.intTable)
              .arg(ensemble.alarm);
    strOut << "</dl>";

    strOut << "<dl>";
    strOut << "<dt>";
    strOut << QString("Services (%1):").arg(serviceList.size());
    strOut << "</dt>";

    for (auto const & s : serviceList)
    {
        strOut << "<dd>";

        strOut << "<dl>";
        strOut << "<dt>";
        if (s.SId.isProgServiceId())
        {   // programme service
            strOut << QString("0x%1 <b>%2</b> [ <i>%3</i> ]")
                              .arg(QString("%1").arg(s.SId.value, 4, 16, QChar('0')).toUpper())
                              .arg(s.label)
                              .arg(s.labelShort);

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
                   .arg(QString("%1").arg(s.SId.value, 8, 16, QChar('0')).toUpper())
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

            strOut << QString(" Label: '%1' [ '%2' ],").arg(sc.label).arg(sc.labelShort);
            strOut << (sc.isAudioService() ? " ASCTy:" : " DSCTy:");

            if (sc.isDataPacketService())
            {
                strOut << QString(" 0x%1, DG: %2, PacketAddr: %3")
                          .arg(QString("%1").arg(int(sc.packetData.DSCTy), 2, 16, QChar('0')).toUpper())
                          .arg(sc.packetData.DGflag)
                          .arg(sc.packetData.packetAddress);
            }
            else
            {
                strOut << QString(" 0x%1").arg(QString("%1").arg(int(sc.streamAudioData.scType), 2, 16, QChar('0')).toUpper());
            }
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
            for (int a = 0; a < sc.userApps.size(); ++a)
            {
                strOut << "<br>";
                strOut << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
                strOut << "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
                strOut << QString("UserApp %1/%2: Label: '%3' [ '%4' ], UAType: 0x%5")
                          .arg(a+1).arg(sc.userApps.size())
                          .arg(sc.userApps.at(a).label)
                          .arg(sc.userApps.at(a).labelShort)
                          .arg(QString("%1").arg(sc.userApps.at(a).uaType, 1, 16, QLatin1Char('0')).toUpper());
                if (sc.isAudioService())
                {
                    strOut << QString(", X-PAD AppTy: %1, DSCTy: 0x%2, DG: %3")
                              .arg(sc.userApps.at(a).xpadData.bits.xpadAppTy)
                              .arg(QString("%1").arg(sc.userApps.at(a).xpadData.bits.DScTy, 1, 16, QLatin1Char('0')).toUpper())
                              .arg(sc.userApps.at(a).xpadData.bits.dgFlag);
                }
                strOut << QString(", Data (%1) [").arg(sc.userApps.at(a).uaData.size());
                for (int d = 0; d < sc.userApps.at(a).uaData.size(); ++d)
                {
                    strOut << QString("%1").arg(sc.userApps.at(a).uaData.at(d), 2, 16, QLatin1Char('0')).toUpper();
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

void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = (RadioControl * ) ctx;
    switch (p->nid)
    {
    case DABPROC_NID_SYNC_STATUS:
    {
        RadioControlEvent * pEvent = new RadioControlEvent;

        const dabProc_NID_SYNC_STATUS_t * pInfo = (const dabProc_NID_SYNC_STATUS_t *) p->pData;
        qDebug("DABPROC_NID_SYNC_STATUS: %d", pInfo->syncLevel);

        pEvent->type = RadioControlEventType::SYNC_STATUS;
        pEvent->status = p->status;
        pEvent->pData = intptr_t(pInfo->syncLevel);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABPROC_NID_TUNE:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABPROC_NID_TUNE: status %d", p->status);
#endif
        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::TUNE;
        pEvent->status = p->status;
        pEvent->pData = intptr_t(*((uint32_t*) p->pData));
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABPROC_NID_ENSEMBLE_INFO:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug("DABPROC_NID_ENSEMBLE_INFO: status %d", p->status);
#endif
        dabProc_NID_ENSEMBLE_INFO_t * pEnsembleInfo = new dabProc_NID_ENSEMBLE_INFO_t;
        memcpy(pEnsembleInfo, p->pData, sizeof(dabProc_NID_ENSEMBLE_INFO_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::ENSEMBLE_INFO;
        pEvent->status = p->status;
        pEvent->pData = intptr_t(pEnsembleInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABPROC_NID_SERVICE_LIST:
    {
        const dabProc_NID_SERVICE_LIST_t * pInfo = (const dabProc_NID_SERVICE_LIST_t *) p->pData;
#if RADIO_CONTROL_VERBOSE
        qDebug("DABPROC_NID_SERVICE_LIST: num services %d", pInfo->numServices);
#endif
        dabProcServiceListItem_t item;
        QList<dabProcServiceListItem_t> *pServiceList = new QList<dabProcServiceListItem_t>;
        for (int s = 0; s < pInfo->numServices; ++s)
        {
            pInfo->getServiceListItem(radioCtrl->dabProcHandle, s, &item);
            pServiceList->append(item);
        }

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_LIST;
        pEvent->status = p->status;
        pEvent->pData = intptr_t(pServiceList);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABPROC_NID_SERVICE_COMPONENT_LIST:
    {
        const dabProc_NID_SERVICE_COMPONENT_LIST_t * pInfo = (const dabProc_NID_SERVICE_COMPONENT_LIST_t * ) p->pData;
        if (DABPROC_NSTAT_SUCCESS == p->status)
        {
            QList<dabProcServiceCompListItem_t> * pList = new QList<dabProcServiceCompListItem_t>;

            dabProcServiceCompListItem_t item;
            for (int s = 0; s < pInfo->numServiceComponents; ++s)
            {
                pInfo->getServiceComponentListItem(radioCtrl->dabProcHandle, s, &item);
                pList->append(item);
            }

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::SERVICE_COMPONENT_LIST;
            pEvent->status = p->status;
            pEvent->pData = intptr_t(pList);
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X not found", pInfo->sid);
        }
    }
        break;
    case DABPROC_NID_USER_APP_LIST:
    {
        const dabProc_NID_USER_APP_LIST_t * pInfo = (const dabProc_NID_USER_APP_LIST_t * ) p->pData;
        if (DABPROC_NSTAT_SUCCESS == p->status)
        {
            QList<dabProcUserAppListItem_t> * pList = new QList<dabProcUserAppListItem_t>;

            dabProcUserAppListItem_t item;
            for (int s = 0; s < pInfo->numUserApps; ++s)
            {
                pInfo->getUserAppListItem(radioCtrl->dabProcHandle, s, &item);
                pList->append(item);
            }

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::USER_APP_LIST;
            pEvent->status = p->status;
            pEvent->pData = intptr_t(pList);
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X, SCIdS %2.2X not found", pInfo->SId, pInfo->SCIdS);
        }
    }
    break;
    case DABPROC_NID_SERVICE_SELECTION:
    {
        dabProc_NID_SERVICE_SELECTION_t * pServSelectionInfo = new dabProc_NID_SERVICE_SELECTION_t;
        memcpy(pServSelectionInfo, p->pData, sizeof(dabProc_NID_SERVICE_SELECTION_t));

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::SERVICE_SELECTION;

        pEvent->status = p->status;
        pEvent->pData = intptr_t(pServSelectionInfo);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    case DABPROC_NID_AUTO_NOTIFY:
    {
        if (p->pData)
        {
            //dabProc_NID_AUTO_NOTIFY_t * pData = (dabProc_NID_AUTO_NOTIFY_t *) p->pData;
            //qDebug("AutoNotify: sync %d, freq offset = %f", pData->syncLevel, pData->freqOffset*0.1);

            assert(sizeof(dabProc_NID_AUTO_NOTIFY_t) == p->len);

            dabProc_NID_AUTO_NOTIFY_t * notifyData = new dabProc_NID_AUTO_NOTIFY_t;
            memcpy((uint8_t*)notifyData, p->pData, p->len);

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::AUTO_NOTIFICATION;
            pEvent->status = p->status;
            pEvent->pData = intptr_t(notifyData);
            radioCtrl->emit_dabEvent(pEvent);
        }
    }
        break;
    default:
        qDebug("Unexpected notification %d", p->nid);
    }
}

void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx)
{
    if (0 == p->dgLen)
    {   // do nothing - empty data group
        qDebug("Empty data group type %d received\n", p->dgType);
        return;
    }

    RadioControl * radioCtrl = (RadioControl * ) ctx;
    switch (p->dgType)
    {
    case DABPROC_DATAGROUP_DL:
    {
        QByteArray * data = new QByteArray((const char *)p->pDgData, p->dgLen);

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::DATAGROUP_DL;
        pEvent->status = DABPROC_NSTAT_SUCCESS;
        pEvent->pData = intptr_t(data);
        radioCtrl->emit_dabEvent(pEvent);        
    }
        break;
    case DABPROC_DATAGROUP_MOT:
    {
        QByteArray * data = new QByteArray((const char *)p->pDgData, p->dgLen);

        RadioControlEvent * pEvent = new RadioControlEvent;
        pEvent->type = RadioControlEventType::DATAGROUP_MSC;
        pEvent->status = DABPROC_NSTAT_SUCCESS;
        pEvent->pData = intptr_t(data);
        radioCtrl->emit_dabEvent(pEvent);
    }
        break;
    default:
        qDebug() << "Unsupported data group type:" << p->dgType;
    }
}

void audioDataCb(dabProcAudioCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = (RadioControl * ) ctx;

    QByteArray * pData = new QByteArray;
    pData->clear();
    pData->append(p->header.raw);
    pData->append((const char *) p->pAuData, p->auLen);

    radioCtrl->emit_audioData(pData);
}
