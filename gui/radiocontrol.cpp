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
{

    // this cancels dabProc thread
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
            ensemble.label = DabTables::convertToQString(pInfo->label.str, pInfo->label.charset);
            ensemble.labelShort = toShortLabel(ensemble.label, pInfo->label.charField);
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
//        else
//        {   // enable auto notifications
//            dabEnableAutoNotification();
//        }
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
            for (QList<dabProcServiceListItem_t>::const_iterator it = pServiceList->constBegin(); it < pServiceList->constEnd(); ++it)
            {
                RadioControlServiceListItem item;
                item.SId.value = it->sid;
                item.label = DabTables::convertToQString(it->label.str, it->label.charset);
                item.labelShort = toShortLabel(item.label, it->label.charField);
                item.pty.s = it->pty.s;
                item.pty.d = it->pty.d;
                item.CAId = it->CAId;
                item.numServiceComponents = it->numComponents;
                serviceList.append(item);
                dabGetServiceComponent(it->sid);
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

        RadioControlServiceComponentData * pServiceComp = (RadioControlServiceComponentData *) pEvent->pData;
        // find service ID
        QList<RadioControlServiceListItem>::iterator serviceIt = findService(pServiceComp->SId);
        if (serviceIt < serviceList.end())
        {   // SId found
            serviceIt->serviceComponents.clear();

            bool requestUpdate = false;
            for (QList<dabProcServiceCompListItem_t>::const_iterator it = pServiceComp->list.constBegin(); it < pServiceComp->list.constEnd(); ++it)
            {
                // first checking validity of data
                if ((it->SubChAddr < 0)
                        || ((!it->ps) && (it->label.str[0] == '\0'))
                        || ((DabTMId::PacketData == DabTMId(it->TMId)) && (it->packetData.packetAddress < 0)))
                {
                    requestUpdate = true;
                    break;
                }

                RadioControlServiceComponentListItem item;
                item.SCIdS = it->SCIdS;
                item.SubChId = it->SubChId;
                item.SubChAddr = it->SubChAddr;
                item.SubChSize = it->SubChSize;
                item.CAflag = it->CAflag;
                item.lang = it->lang;
                item.protection.level = DabProtectionLevel(it->protectionLevel);
                if (DabProtectionLevel::UEP_MAX < item.protection.level)
                {  // eep protection - store coderate
                    item.protection.codeRateFecValue = EEPCoderate[int(item.protection.level)-int(DabProtectionLevel::EEP_MIN)];
                    item.protection.fecScheme = (it->fecScheme != 0);
                }
                else
                {
                    item.protection.uepIndex = it->uepIdx;       // UEP index or FEC scheme flag (union)
                }

                item.numUserApps = it->numUserApps;
                item.ps = it->ps;
                if (it->ps)
                {  // primary: [8.1.14.3] The service component label shall not be signalled for primary service component
                    item.label = serviceIt->label;
                    item.labelShort = serviceIt->labelShort;
                }
                else
                {
                    item.label = DabTables::convertToQString(it->label.str, it->label.charset);
                    item.labelShort = toShortLabel(item.label, it->label.charField);
                }
                item.TMId = DabTMId(it->TMId);
                switch (item.TMId)
                {
                case DabTMId::StreamAudio:
                    item.streamAudio.ASCTy = it->streamAudio.ASCTy;
                    item.streamAudio.bitRate = it->streamAudio.bitRate;
                    break;
                case DabTMId::StreamData:
                    item.streamData.DSCTy = it->streamData.DSCTy;
                    item.streamData.bitRate = it->streamData.bitRate;
                    break;
                case DabTMId::PacketData:
                    item.packetData.DGflag = it->packetData.DGflag;
                    item.packetData.DSCTy = it->packetData.DSCTy;
                    item.packetData.SCId = it->packetData.SCId;
                    item.packetData.packetAddress = it->packetData.packetAddress;
                    break;
                }
                serviceIt->serviceComponents.append(item);

                RadioControlServiceListEntry s;
                s.ensemble.frequency = ensemble.frequency;
                s.ensemble.ueid = ensemble.ueid;
                s.ensemble.label = ensemble.label;
                s.ensemble.labelShort = ensemble.labelShort;
                s.SId = serviceIt->SId;
                s.SCIdS = item.SCIdS;
                s.label = item.label;
                s.labelShort = item.labelShort;
                s.pty = serviceIt->pty;
                s.TMId = item.TMId;
                switch (item.TMId)
                {
                case DabTMId::StreamAudio:
                    s.bitRate = item.streamAudio.bitRate;
                    break;
                case DabTMId::StreamData:
                    s.bitRate = item.streamData.bitRate;
                    break;
                case DabTMId::PacketData:
                    s.bitRate = 0;
                    break;
                }

                if ((serviceRequest.SId == serviceIt->SId.value) && (serviceRequest.SCIdS == item.SCIdS))
                {
                    dabServiceSelection(serviceRequest.SId, serviceRequest.SCIdS);
                    serviceRequest.SId = 0;    // clear request
                }
                emit serviceListEntry(s);
            }
            if (requestUpdate)
            {
                serviceIt->serviceComponents.clear();
                dabGetServiceComponent(pServiceComp->SId.value);
            }
            else
            {   // service list item information is complete
            }
        }
        else
        {  // SId not found
            qDebug("Service SID %4.4X not in service list", pServiceComp->SId.value);
        }
        delete pServiceComp;
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

            DabSId sid;
            sid.value = pData->SId;
            QList<RadioControlServiceListItem>::iterator serviceIt = findService(sid);
            if (serviceIt != serviceList.end())
            {   // service is in the list
                QList<RadioControlServiceComponentListItem>::iterator scIt = findServiceComponent(serviceIt, pData->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service components exists in service
                    if (DabTMId::StreamAudio == scIt->TMId)
                    {   // audio service
                        RadioControlAudioService audioService;
                        audioService.SId = serviceIt->SId;
                        audioService.SCIdS = scIt->SCIdS;
                        audioService.label = scIt->label;
                        audioService.labelShort = scIt->labelShort;
                        audioService.pty = serviceIt->pty;
                        audioService.ASCTy = DabAudioMode(scIt->streamAudio.ASCTy);
                        audioService.bitRate = scIt->streamAudio.bitRate;
                        audioService.SubChSize = scIt->SubChSize;
                        audioService.lang = scIt->lang;
                        audioService.protection = scIt->protection;

                        //emit newAudioService(DabAudioMode(scIt->streamAudio.ASCTy));
                        emit newAudioService(audioService);
                    }
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

            QDateTime dateAndTime = QDateTime(QDate(Y, M, D), QTime(h, minute), Qt::UTC).toOffsetFromUtc(60*(ensemble.LTO * 30));

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
#if RADIO_CONTROL_VERBOSE > 0
        qDebug("AutoNotify: sync %d, freq offset = %f Hz, SNR = %f dB",
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

QList<RadioControlServiceListItem>::iterator RadioControl::findService(DabSId SId)
{
    QList<RadioControlServiceListItem>::iterator serviceIt;
    for (serviceIt = serviceList.begin(); serviceIt < serviceList.end(); ++serviceIt)
    {
        if (SId.value ==  serviceIt->SId.value)
        {  // found SId
            return serviceIt;
        }
    }
    return serviceIt;
}

QList<RadioControlServiceComponentListItem>::iterator
                RadioControl::findServiceComponent(const QList<RadioControlServiceListItem>::iterator & sIt, uint8_t SCIdS)
{
    QList<RadioControlServiceComponentListItem>::iterator scIt;
    for (scIt = sIt->serviceComponents.begin(); scIt < sIt->serviceComponents.end(); ++scIt)
    {
        if (SCIdS == scIt->SCIdS)
        {
            return scIt;
        }
    }
    return scIt;
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
            RadioControlServiceComponentData *pServiceCompList = new RadioControlServiceComponentData;
            pServiceCompList->SId.value = pInfo->sid;

            dabProcServiceCompListItem_t item;
            for (int s = 0; s < pInfo->numServiceComponents; ++s)
            {
                pInfo->getServiceComponentListItem(radioCtrl->dabProcHandle, s, &item);
                pServiceCompList->list.append(item);
            }

            RadioControlEvent * pEvent = new RadioControlEvent;
            pEvent->type = RadioControlEventType::SERVICE_COMPONENT_LIST;
            pEvent->status = p->status;
            pEvent->pData = intptr_t(pServiceCompList);
            radioCtrl->emit_dabEvent(pEvent);
        }
        else
        {
            qDebug("SId %4.4X not found", pInfo->sid);
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
    pData->append(p->aacSfHeader);
    pData->append((const char *) p->pAuData, p->auLen);

    radioCtrl->emit_audioData(pData);
}
