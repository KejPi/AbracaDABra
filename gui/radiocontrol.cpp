#include <QThread>
#include <QDate>
#include <QTime>

#include <QDebug>

#include "radiocontrol.h"
#include "dldecoder.h"

const QMap<uint32_t, QString> RadioControl::DABChannelList =
{
    {174928 , "5A"},
    {176640 , "5B"},
    {178352 , "5C"},
    {180064 , "5D"},
    {181936 , "6A"},
    {183648 , "6B"},
    {185360 , "6C"},
    {187072 , "6D"},
    {188928 , "7A"},
    {190640 , "7B"},
    {192352 , "7C"},
    {194064 , "7D"},
    {195936 , "8A"},
    {197648 , "8B"},
    {199360 , "8C"},
    {201072 , "8D"},
    {202928 , "9A"},
    {204640 , "9B"},
    {206352 , "9C"},
    {208064 , "9D"},
    {209936 , "10A"},
    {211648 , "10B"},
    {213360 , "10C"},
    {215072 , "10D"},
    #if RADIO_CONTROL_N_CHANNELS_ENABLE
    {210096 , "10N"},
    #endif
    {216928 , "11A"},
    {218640 , "11B"},
    {220352 , "11C"},
    {222064 , "11D"},
    #if RADIO_CONTROL_N_CHANNELS_ENABLE
    {217088 , "11N"},
    #endif
    {223936 , "12A"},
    {225648 , "12B"},
    {227360 , "12C"},
    {229072 , "12D"},
    #if RADIO_CONTROL_N_CHANNELS_ENABLE
    {224096 , "12N"},
    #endif
    {230784 , "13A"},
    {232496 , "13B"},
    {234208 , "13C"},
    {235776 , "13D"},
    {237488 , "13E"},
    {239200 , "13F"}
};

const uint16_t RadioControl::ebuLatin2UCS2[] =
{   /* UCS2 == UTF16 for Basic Multilingual Plane 0x0000-0xFFFF */
    0x0000, 0x0118, 0x012E, 0x0172, 0x0102, 0x0116, 0x010E, 0x0218, /* 0x00 - 0x07 */
    0x021A, 0x010A, 0x000A, 0x000B, 0x0120, 0x0139, 0x017B, 0x0143, /* 0x08 - 0x0f */
    0x0105, 0x0119, 0x012F, 0x0173, 0x0103, 0x0117, 0x010F, 0x0219, /* 0x10 - 0x17 */
    0x021B, 0x010B, 0x0147, 0x011A, 0x0121, 0x013A, 0x017C, 0x001F, /* 0x18 - 0x1f */
    0x0020, 0x0021, 0x0022, 0x0023, 0x0142, 0x0025, 0x0026, 0x0027, /* 0x20 - 0x27 */
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, /* 0x28 - 0x2f */
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, /* 0x30 - 0x37 */
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, /* 0x38 - 0x3f */
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, /* 0x40 - 0x47 */
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, /* 0x48 - 0x4f */
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, /* 0x50 - 0x57 */
    0x0058, 0x0059, 0x005A, 0x005B, 0x016E, 0x005D, 0x0141, 0x005F, /* 0x58 - 0x5f */
    0x0104, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, /* 0x60 - 0x67 */
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F, /* 0x68 - 0x6f */
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, /* 0x70 - 0x77 */
    0x0078, 0x0079, 0x007A, 0x00AB, 0x016F, 0x00BB, 0x013D, 0x0126, /* 0x78 - 0x7f */
    0x00E1, 0x00E0, 0x00E9, 0x00E8, 0x00ED, 0x00EC, 0x00F3, 0x00F2, /* 0x80 - 0x87 */
    0x00F , 0x00F9, 0x00D1, 0x00C7, 0x015E, 0x00DF, 0x00A1, 0x0178, /* 0x88 - 0x8f */
    0x00E2, 0x00E4, 0x00EA, 0x00EB, 0x00EE, 0x00EF, 0x00F4, 0x00F6, /* 0x90 - 0x97 */
    0x00FB, 0x00FC, 0x00F1, 0x00E7, 0x015F, 0x011F, 0x0131, 0x00FF, /* 0x98 - 0x9f */
    0x0136, 0x0145, 0x00A9, 0x0122, 0x011E, 0x011B, 0x0148, 0x0151, /* 0xa0 - 0xa7 */
    0x0150, 0x20AC, 0x00A3, 0x0024, 0x0100, 0x0112, 0x012A, 0x016A, /* 0xa8 - 0xaf */
    0x0137, 0x0146, 0x013B, 0x0123, 0x013C, 0x0130, 0x0144, 0x0171, /* 0xb0 - 0xb7 */
    0x0170, 0x00BF, 0x013E, 0x00B0, 0x0101, 0x0113, 0x012B, 0x016B, /* 0xb8 - 0xbf */
    0x00C1, 0x00C0, 0x00C9, 0x00C8, 0x00CD, 0x00CC, 0x00D3, 0x00D2, /* 0xc0 - 0xc7 */
    0x00DA, 0x00D9, 0x0158, 0x010C, 0x0160, 0x017D, 0x00D0, 0x013F, /* 0xc8 - 0xcf */
    0x00C2, 0x00C4, 0x00CA, 0x00CB, 0x00CE, 0x00CF, 0x00D4, 0x00D6, /* 0xd0 - 0xd7 */
    0x00DB, 0x00DC, 0x0159, 0x010D, 0x0161, 0x017E, 0x0111, 0x0140, /* 0xd8 - 0xdf */
    0x00C3, 0x00C5, 0x00C6, 0x0152, 0x0177, 0x00DD, 0x00D5, 0x00D8, /* 0xe0 - 0xe7 */
    0x00DE, 0x014A, 0x0154, 0x0106, 0x015A, 0x0179, 0x0164, 0x00F0, /* 0xe8 - 0xef */
    0x00E3, 0x00E5, 0x00E6, 0x0153, 0x0175, 0x00FD, 0x00F5, 0x00F8, /* 0xf0 - 0xf7 */
    0x00FE, 0x014B, 0x0155, 0x0107, 0x015B, 0x017A, 0x0165, 0x0127, /* 0xf8 - 0xff */
};

const uint8_t RadioControl::EEPCoderate[] =
{ // ETSI EN 300 401 V2.1.1 [6.2.1 Basic sub-channel organization] table 9 & 10
    0x14, 0x38, 0x12, 0x34,   // EEP 1-A..4-A : 1/4 3/8 1/2 3/4
    0x49, 0x47, 0x46, 0x45    // EEP 1-B..4-B : 4/9 4/7 4/6 4/5
};

const QStringList RadioControl::PTyNames =
{
    "None",
    "News",
    "Current Affairs",
    "Information",
    "Sport",
    "Education",
    "Drama",
    "Culture",
    "Science",
    "Varied",
    "Pop Music",
    "Rock Music",
    "Easy Listening Music",
    "Light Classical",
    "Serious Classical",
    "Other Music",
    "Weather/meteorology",
    "Finance/Business",
    "Children's programmes",
    "Social Affairs",
    "Religion",
    "Phone In",
    "Travel",
    "Leisure",
    "Jazz Music",
    "Country Music",
    "National Music",
    "Oldies Music",
    "Folk Music",
    "Documentary",
    "",
    "",
};

void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx);
void dataGroupCb(dabProcDataGroupCBData_t * p, void * ctx);
void audioDataCb(dabProcAudioCBData_t * p, void * ctx);

RadioControl::RadioControl(QObject *parent) : QObject(parent)
{
    dabProcHandle = NULL;
    frequency = 0;
    serviceList.clear();
    //serviceList = NULL;

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

void RadioControl::eventFromDab(radioControlEvent_t * pEvent)
{
    //qDebug() << Q_FUNC_INFO << QThread::currentThreadId() << pEvent->type;
    switch (pEvent->type)
    {
    case RCTRL_EVENT_SYNC_STATUS:
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
    case RCTRL_EVENT_TUNE:
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
    case RCTRL_EVENT_ENSEMBLE_INFO:
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
            ensemble.label = convertToQString(pInfo->label.str, pInfo->label.charset);
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
    case RCTRL_EVENT_SERVICE_LIST:
    {
#if 1 // RADIO_CONTROL_VERBOSE
        qDebug() << "RCTRL_EVENT_SERVICE_LIST";
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
                radioControlServiceListItem_t item;
                item.SId = it->sid;
                item.label = convertToQString(it->label.str, it->label.charset);
                item.labelShort = toShortLabel(item.label, it->label.charField);
                if (it->pty & 0x80)
                {  // dynamic pty
                    // TODO:
                }
                item.pty = getPtyName(it->pty & 0x1F);
                item.CAId = it->CAId;
                item.numServiceComponents = it->numComponents;
                serviceList.append(item);
                dabGetServiceComponent(it->sid);
            }
        }
        delete pServiceList;
    }
        break;
    case RCTRL_EVENT_SERVICE_COMPONENT_LIST:
    {
#if RADIO_CONTROL_VERBOSE
        qDebug() << "RCTRL_EVENT_SERVICE_COMPONENT_LIST";
#endif

        radioControlServiceComponentData_t * pServiceComp = (radioControlServiceComponentData_t *) pEvent->pData;
        // find service ID
        radioControlServiceListIterator_t serviceIt = findService(pServiceComp->SId);
        if (serviceIt < serviceList.end())
        {   // SId found
            serviceIt->serviceComponents.clear();

            bool requestUpdate = false;
            for (QList<dabProcServiceCompListItem_t>::const_iterator it = pServiceComp->list.constBegin(); it < pServiceComp->list.constEnd(); ++it)
            {
                // first checking validity of data
                if ((it->SubChAddr < 0)
                        || ((!it->ps) && (it->label.str[0] == '\0'))
                        || ((RADIOCONTROL_TMPacketData == it->TMId) && (it->packetData.packetAddress < 0)))
                {
#if RADIO_CONTROL_VERBOSE
                    qDebug("Service SID %4.4X not complete", pServiceComp->SId);
#endif
                    requestUpdate = true;
                    break;
                }

                radioControlServiceComponentListItem_t item;
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
                    item.label = convertToQString(it->label.str, it->label.charset);
                    item.labelShort = toShortLabel(item.label, it->label.charField);
                }
                item.TMId = radioControlTMId_t(it->TMId);
                switch (it->TMId)
                {
                case RADIOCONTROL_TMStreamAudio:
                    item.streamAudio.ASCTy = it->streamAudio.ASCTy;
                    item.streamAudio.bitRate = it->streamAudio.bitRate;
                    break;
                case RADIOCONTROL_TMStreamData:
                    item.streamData.DSCTy = it->streamData.DSCTy;
                    item.streamData.bitRate = it->streamData.bitRate;
                    break;
                case RADIOCONTROL_TMPacketData:
                    item.packetData.DGflag = it->packetData.DGflag;
                    item.packetData.DSCTy = it->packetData.DSCTy;
                    item.packetData.SCId = it->packetData.SCId;
                    item.packetData.packetAddress = it->packetData.packetAddress;
                    break;
                default:
                    qDebug("Service SID %4.4X: Unexpected transport mode %d", pServiceComp->SId, it->TMId);
                    requestUpdate = true;
                }
                serviceIt->serviceComponents.append(item);
            }
            if (requestUpdate)
            {
                serviceIt->serviceComponents.clear();
                dabGetServiceComponent(pServiceComp->SId);
            }
            else
            {  // service list item information is complete
                emit serviceListItemAvailable(*serviceIt);
            }
        }
        else
        {  // SId not found
            qDebug("Service SID %4.4X not in service list", pServiceComp->SId);
        }
        delete pServiceComp;
    }
        break;
    case RCTRL_EVENT_SERVICE_SELECTION:
    {
        dabProc_NID_SERVICE_SELECTION_t * pData = (dabProc_NID_SERVICE_SELECTION_t *) pEvent->pData;
        if (DABPROC_NSTAT_SUCCESS == pEvent->status)
        {
#if RADIO_CONTROL_VERBOSE
            qDebug() << "RCTRL_EVENT_SERVICE_SELECTION success";
#endif
            radioControlServiceListIterator_t serviceIt = findService(pData->SId);
            if (serviceIt != serviceList.end())
            {   // service is in the list
                radioControlServiceComponentListIterator_t scIt = findServiceComponent(serviceIt, pData->SCIdS);
                if (scIt != serviceIt->serviceComponents.end())
                {   // service components exists in service
                    emit newService(pData->SId, pData->SCIdS);
                    if (RADIOCONTROL_TMStreamAudio == scIt->TMId)
                    {   // audio service
                        emit newAudioService(DabAudioMode(scIt->streamAudio.ASCTy));
                    }
                }
            }
        }        
        else
        {
            qDebug() << "RCTRL_EVENT_SERVICE_SELECTION error" << pEvent->status;
        }
        delete pData;
    }
        break;
    case RCTRL_EVENT_AUTO_NOTIFICATION:
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
    case RCTRL_EVENT_DATAGROUP_DL:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RCTRL_EVENT_DATAGROUP_DL";
#endif
        QByteArray * pData = (QByteArray *) pEvent->pData;
        emit dlDataGroup(*pData);
        delete pData;
    }
        break;
    case RCTRL_EVENT_DATAGROUP_MSC:
    {
#if RADIO_CONTROL_VERBOSE > 1
        qDebug() << "RCTRL_EVENT_DATAGROUP_MSC";
#endif
        QByteArray * pData = (QByteArray *) pEvent->pData;
        emit mscDataGroup(*pData);
        delete pData;
    }
        break;

    default:
        qDebug() << Q_FUNC_INFO << "ERROR: Unsupported event" << pEvent->type;
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

void RadioControl::tune(uint32_t freq)
{
    if (freq != frequency)
    {
        autoNotificationEna = false;
        frequency = freq;
        dabTune(0);
    }
}

void RadioControl::start(uint32_t freq)
{
    if (freq)
    {   // when input device tuned, freq is passed to be set in SDR
        // when frequency is 0 then we are done (input device is in idle)
        frequency = freq;
        sync = DABPROC_SYNC_LEVEL_NO_SYNC;
        emit syncStatus(RADIOCONTROL_SYNC_LEVEL0);
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
    //qDebug() << Q_FUNC_INFO << freq << frequency;
    if (freq == frequency)
    {
        dabServiceSelection(SId, SCIdS);
    }
    else
    {
        // TODO: tune and service selection
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
            emit syncStatus(RADIOCONTROL_SYNC_LEVEL0);
            break;
        case DABPROC_SYNC_LEVEL_ON_NULL:
            emit syncStatus(RADIOCONTROL_SYNC_LEVEL1);
            break;
        case DABPROC_SYNC_LEVEL_FIC:
            emit syncStatus(RADIOCONTROL_SYNC_LEVEL2);
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

radioControlServiceListIterator_t RadioControl::findService(uint32_t SId)
{
    radioControlServiceListIterator_t serviceIt;
    for (serviceIt = serviceList.begin(); serviceIt < serviceList.end(); ++serviceIt)
    {
        if (SId ==  serviceIt->SId)
        {  // found SId
            return serviceIt;
        }
    }
    return serviceIt;
}

radioControlServiceComponentListIterator_t RadioControl::findServiceComponent(const radioControlServiceListIterator_t & sIt, uint8_t SCIdS)
{
    radioControlServiceComponentListIterator_t scIt;
    for (scIt = sIt->serviceComponents.begin(); scIt < sIt->serviceComponents.end(); ++scIt)
    {
        if (SCIdS == scIt->SCIdS)
        {
            return scIt;
        }
    }
    return scIt;
}

QString RadioControl::convertToQString(const char *c, uint8_t charset, uint8_t len)
{
    QString out;
    switch (static_cast<DabCharset>(charset))
    {
    case DabCharset::UTF8:
        out = out.fromUtf8(c);
        break;
    case DabCharset::UCS2:
    {
        uint8_t ucsLen = len/2+1;
        uint16_t * temp = new uint16_t[ucsLen];
        // BOM = 0xFEFF, DAB label is in big endian, writing it byte by byte
        uint8_t * bomPtr = (uint8_t *) temp;
        *bomPtr++ = 0xFE;
        *bomPtr++ = 0xFF;
        memcpy(temp+1, c, len);
        out = out.fromUtf16(temp, ucsLen);
        delete [] temp;
    }
        break;
    case DabCharset::EBULATIN:
        while(*c)
        {
            out.append(QChar(ebuLatin2UCS2[uint8_t(*c++)]));
        }
        break;
    default:
        // do noting, unsupported charset
        qDebug("ERROR: Charset %d is not supported", charset);
        break;
    }

    return out;
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

QString RadioControl::getPtyName(const uint8_t pty)
{
    if (pty >= PTyNames.size())
    {
        return PTyNames[0];
    }
    return PTyNames[pty];
}

void dabNotificationCb(dabProcNotificationCBData_t * p, void * ctx)
{
    RadioControl * radioCtrl = (RadioControl * ) ctx;
    switch (p->nid)
    {
    case DABPROC_NID_SYNC_STATUS:
    {
        radioControlEvent_t * pEvent = new radioControlEvent_t;

        const dabProc_NID_SYNC_STATUS_t * pInfo = (const dabProc_NID_SYNC_STATUS_t *) p->pData;
        qDebug("DABPROC_NID_SYNC_STATUS: %d", pInfo->syncLevel);

        pEvent->type = RCTRL_EVENT_SYNC_STATUS;
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
        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_TUNE;
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

        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_ENSEMBLE_INFO;
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

        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_SERVICE_LIST;
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
            radioControlServiceComponentData_t *pServiceCompList = new radioControlServiceComponentData_t;
            pServiceCompList->SId = pInfo->sid;

            dabProcServiceCompListItem_t item;
            for (int s = 0; s < pInfo->numServiceComponents; ++s)
            {
                pInfo->getServiceComponentListItem(radioCtrl->dabProcHandle, s, &item);
                pServiceCompList->list.append(item);
            }

            radioControlEvent_t * pEvent = new radioControlEvent_t;
            pEvent->type = RCTRL_EVENT_SERVICE_COMPONENT_LIST;
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

        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_SERVICE_SELECTION;

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

            radioControlEvent_t * pEvent = new radioControlEvent_t;
            pEvent->type = RCTRL_EVENT_AUTO_NOTIFICATION;
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

        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_DATAGROUP_DL;
        pEvent->status = DABPROC_NSTAT_SUCCESS;
        pEvent->pData = intptr_t(data);
        radioCtrl->emit_dabEvent(pEvent);        
    }
        break;
    case DABPROC_DATAGROUP_MOT:
    {
        QByteArray * data = new QByteArray((const char *)p->pDgData, p->dgLen);

        radioControlEvent_t * pEvent = new radioControlEvent_t;
        pEvent->type = RCTRL_EVENT_DATAGROUP_MSC;
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
