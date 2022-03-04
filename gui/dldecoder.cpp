#include "dldecoder.h"
#include "dabtables.h"
#include <QString>
#include <QDebug>

QDebug operator<<(QDebug debug, DLPlusContentType type);

DLDecoder::DLDecoder(QObject *parent) : QObject(parent)
{
    reset();
}

void DLDecoder::reset()
{
    label.clear();
    dlCommand.clear();
    segmentCntr = 0;
    labelToggle = -1;
    cmdToggle = -1;
    cmdLink = -1;
    message.clear();
    itemToggle = -1;
    itemRunnning = -1;

    emit resetTerminal();
}

bool DLDecoder::crc16check(const QByteArray & data)
{
    uint16_t crc = 0xFFFF;
    uint16_t mask = 0x1020;

    for (int n = 0; n < data.size()-2; ++n)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            uint16_t in = (data[n] >> bit) & 0x1;
            uint16_t tmp = in ^ ((crc>>15) & 0x1);
            crc <<= 1;

            crc ^= (tmp * mask);
            crc += tmp;
        }
    }

    uint16_t invTxCRC = ~(uint16_t(data[data.size()-2] << 8) | uint8_t(data[data.size()-1]));

    return (crc == invTxCRC);
}

void DLDecoder::newDataGroup(const QByteArray & dataGroup)
{
    // CRC check is done in dabProc
    if (dataGroup.at(0) & 0x10)
    {  // C flag == 1
        switch (dataGroup.at(0) & 0x0F)
        {
        case 0b0001:
            // [ETSI EN 300 401 V2.1.1]
            // [0 0 0 1: clear display command - remove the dynamic label message from the display;]
#if DLDECODER_VERBOSE>0
            qDebug() << "DL: clear display command";
#endif
            emit resetTerminal();
            break;
        case 0b0010:
            // [ETSI EN 300 401 V2.1.1 ]
            // [0 0 1 0: DL Plus command, see ETSI TS 102 980 [7].]

            // [ETSI TS 102 980 V2.1.2] 7.4 Transmission sequence
            // The complete DL message shall be transmitted before the related DL Plus command(s) are transmitted.
            if (message.isEmpty())
            {  // this means no message has been received => do nothing
                return;
            }
            else
            { /* there is some message that might be related */ }

            if (assembleDLPlusCommand(dataGroup))
            {  // command is complete
                int8_t t = (dataGroup.at(0) & 0x80) != 0;
#if DLDECODER_VERBOSE>1
                if (t != cmdToggle)
                {
                    qDebug() << "New DL+ command";
                }
                else
                {
                    qDebug() << "Duplicate DL+ command";
                }
#endif
                cmdToggle = t;

                parseDLPlusCommand();
            }
            break;
        default:
            qDebug() << "DL: Unexpected DL command";
        }
    }
    else
    {
        if (assembleDL(dataGroup))
        {   // message is complete
//            if (label.lastIndexOf(0x0A) >= 0)
//            {
//                qDebug() << Q_FUNC_INFO << "control character 0x0A (preferred line break) found";
//            }
//            if (label.lastIndexOf(0x0B) >= 0)
//            {
//                qDebug() << Q_FUNC_INFO << "control character 0x0B (end of a headline) found";
//            }
//            if (label.lastIndexOf(0x1F) >= 0)
//            {
//                qDebug() << Q_FUNC_INFO << "control character 0x1F (preferred word break) found";
//            }

            message = DabTables::convertToQString(label.data(), charset, label.size());
            emit dlComplete(message);
            int8_t t = (dataGroup.at(0) & 0x80) != 0;
#if DLDECODER_VERBOSE>1
            if (t != labelToggle)
            {
                qDebug() << "New DL" << message;
            }
            else
            {
                qDebug() << "Duplicate DL" << message;
            }
#endif
            labelToggle = t;
        }
    }
}

bool DLDecoder::assembleDL(const QByteArray & dataGroup)
{
    uint8_t numChars = (dataGroup.at(0) & 0x0F) + 1;

    if (dataGroup.at(0) & 0x40)
    {   // first
        charset = (dataGroup.at(1) >> 4) & 0x0F;

        // starting message
        label.clear();
        message.clear();
        segmentCntr = 0;  // first segment
    }
    else
    {   // not first
        if ((label.size() == 0)
                || (segmentCntr != ((dataGroup.at(1) >> 4) & 0x07))
                || (label.size() + numChars > XPAD_DL_LEN_MAX))
        {   // missing first => reset and waiting until we receive first
            // or missing some previous
            // or label is just too long
            label.clear();
            message.clear();
            segmentCntr = 0;
            labelToggle = -1;     // starting from the beginning
            return false;
        }
    }

    // all segments captured so far
    segmentCntr++;  // incrementing expected segment number for next call

    // copy characters
    label.append(dataGroup.mid(2, numChars));

    // if this was last message we have complete message
    if (dataGroup[0] & 0x20)
    {   // last
#if DLDECODER_VERBOSE>1
        int8_t t = (dataGroup.at(0) & 0x80) != 0;
        qDebug() << QString("DL[new=%1] (charset %2) [len=%3]: %4").arg(t != labelToggle).arg(charset).arg(label.size()).arg(QString(label));
#endif
        return true;
    }
    return false;
}

bool DLDecoder::assembleDLPlusCommand(const QByteArray &dataGroup)
{
    // L is bit 7
    // L (Link bit): in commands that are linked to a DL message, this bit shall carry the same value as
    // the toggle bit of the DL message segments; otherwise it shall be set to zero
    // link bit is trasmitted for all segments
    uint_fast8_t linkBit = (dataGroup.at(1) >> 7) & 0x01;

    // Field 3: this 4-bit field, expressed as an unsigned binary number,
    // shall specify the number of bytes in the DL Command field minus 1.
    uint8_t numBytes = (dataGroup.at(1) & 0x0F) + 1;

    if (dataGroup.at(0) & 0x40)
    {   // first
        cmdLink = linkBit;

        // starting message
        dlCommand.clear();
        segmentCntr = 0;  // first segment
    }
    else
    {   // not first
        if ((dlCommand.size() == 0)
                || (segmentCntr != ((dataGroup.at(1) >> 4) & 0x07))
                || (linkBit != cmdLink)
                || ((dlCommand.size() + numBytes) > XPAD_DL_LEN_MAX))
        {   // missing first => reset and waiting until we receive first
            // or missing some previous
            // or unexpected value of link bit (many segments dropped and receiving already next command)
            // or command is just too long

            qDebug() << "DL+ corrupted message";

            dlCommand.clear();
            segmentCntr = 0;
            cmdToggle= -1;        // starting from the beginning
            return false;
        }
    }

    // all segments captured so far
    segmentCntr++;  // incrementing expected segment number for next call

    // copy characters
    dlCommand.append(dataGroup.mid(2, numBytes));

    // if this was last message we have complete message
    if (dataGroup.at(0) & 0x20)
    {   // last
        return true;
    }
    return false;
}

void DLDecoder::parseDLPlusCommand()
{
    uint_fast8_t cid = (dlCommand.at(0) >> 4) & 0x0F;
    switch (cid)
    {
    case 0:
    {   // "0 0 0 0": DL Plus tags command
#if DLDECODER_VERBOSE>1
        qDebug("DL+ CId = %d, CB = %d", cid, dlCommand.at(0) & 0x0F);
#endif

        if (cmdLink != labelToggle)
        {   // // [ETSI TS 102 980 V2.1.2] 7.4 Transmission sequence
            // The L (Link) bit of the DL Plus tags command shall be set to the same value as the T (Toggle) bit of the related DL message.
#if DLDECODER_VERBOSE>0
            qDebug() << "DL+ link bit does not match DL toggle bit";
#endif
            break;
        }

        int_fast8_t itToggle = (dlCommand.at(0) >> 3) & 0x1;
        int_fast8_t itRunning = (dlCommand.at(0) >> 2) & 0x1;
        int_fast8_t numTags = (dlCommand.at(0) & 0x03) + 1;

        if (itToggle != itemToggle)
        {
            itemToggle = itToggle;
            emit dlItemToggle();
        }
        else
        { /* do nothing */ }

        if (itRunning != itemRunnning)
        {
            itemRunnning = itRunning;
            emit dlItemRunning(0 != itemRunnning);
        }
        else
        { /* do nothing */ }

        // check is we have enough data
        // [ETSI TS 102 980 V2.1.2] 7.3 The DL Plus tags command
        // The NT field shall be evaluated by receivers to determine the correct number of DL tags.
        // NOTE: The DL Plus tag(s) may in future be followed by other data which is reserved for future amendments.
        if (numTags * 3 + 1 > dlCommand.size())
        {
#if DLDECODER_VERBOSE>0
            qDebug() << "DL+ unexpected command length";
#endif
            break;
        }
        else
        { /* there is enough data available */ }

#if DLDECODER_VERBOSE>1
        qDebug("DL+ Item toggle %d, Item Running %d, Num tags %d [0x%2.2X]", itToggle, itRunning, numTags, dlCommand.at(0));
#endif

        // each tag is 24 bits
        for (int t = 0; t<numTags; ++t)
        {
            uint8_t contentsType = dlCommand.at(1+3*t) & 0x7F;
            uint8_t startMarker = dlCommand.at(2+3*t) & 0x7F;
            uint8_t lengthMarker = (dlCommand.at(3+3*t) & 0x7F) + 1;

            if (startMarker + lengthMarker > message.size())
            {   // something is wrong
#if DLDECODER_VERBOSE>0
                qDebug("DL+ unexpected tag %d size: start %d, length %d", contentsType, startMarker, lengthMarker);
#endif
                return;
            }
            else
            { /* tag is inside received message */ }

            if (DLPlusContentType(contentsType) >= DLPlusContentType::DESCRIPTOR_PLACE)
            {   // descriptors are ignored at the moment
                continue;
            }
            else
            { /* not descriptor */ }

            DLPlusObject object(message.mid(startMarker, lengthMarker), DLPlusContentType(contentsType));

            emit dlPlusObject(object);

#if DLDECODER_VERBOSE>1
            qDebug("DL+ tag %d, start %d, length %d", contentsType, startMarker, lengthMarker);
#endif
        }
    }
        break;
    default:
        qDebug() << "DL+ unsupported command ID" << cid;
    }
}

DLPlusObject::DLPlusObject()
{
    tag.clear();
    type = DLPlusContentType::DUMMY;
}

DLPlusObject::DLPlusObject(const QString &newTag, DLPlusContentType newType) :
    tag(newTag),
    type(newType)
{ }

DLPlusContentType DLPlusObject::getType() const
{
    return type;
}

void DLPlusObject::setType(DLPlusContentType newType)
{
    type = newType;
}

const QString &DLPlusObject::getTag() const
{
    return tag;
}

void DLPlusObject::setTag(const QString &newTag)
{
    tag = newTag;
}

bool DLPlusObject::isDelete() const
{
    return (tag.size() == 1) && (tag.at(0) == QChar(' '));
}

bool DLPlusObject::isItem() const
{
    return ((type > DLPlusContentType::DUMMY) && (type < DLPlusContentType::INFO_NEWS));
}

bool DLPlusObject::isDummy() const
{
    return (DLPlusContentType::DUMMY == type);
}

bool DLPlusObject::operator==(const DLPlusObject &other) const
{
    return ((other.type == type) && (other.tag == tag));

}

bool DLPlusObject::operator!=(const DLPlusObject &other) const
{
    return ((other.type != type) || (other.tag != tag));
}

QDebug operator<<(QDebug debug, DLPlusContentType type)
{
    switch (type)
    {
    case DLPlusContentType::DUMMY: debug << "DLPlusContentType::DUMMY"; break;
    case DLPlusContentType::ITEM_TITLE: debug << "DLPlusContentType::ITEM_TITLE"; break;
    case DLPlusContentType::ITEM_ALBUM: debug << "DLPlusContentType::ITEM_ALBUM"; break;
    case DLPlusContentType::ITEM_TRACKNUMBER: debug << "DLPlusContentType::ITEM_TRACKNUMBER"; break;
    case DLPlusContentType::ITEM_ARTIST: debug << "DLPlusContentType::ITEM_ARTIST"; break;
    case DLPlusContentType::ITEM_COMPOSITION: debug << "DLPlusContentType::ITEM_COMPOSITION"; break;
    case DLPlusContentType::ITEM_MOVEMENT: debug << "DLPlusContentType::ITEM_MOVEMENT"; break;
    case DLPlusContentType::ITEM_CONDUCTOR: debug << "DLPlusContentType::ITEM_CONDUCTOR"; break;
    case DLPlusContentType::ITEM_COMPOSER: debug << "DLPlusContentType::ITEM_COMPOSER"; break;
    case DLPlusContentType::ITEM_BAND: debug << "DLPlusContentType::ITEM_BAND"; break;
    case DLPlusContentType::ITEM_COMMENT: debug << "DLPlusContentType::ITEM_COMMENT"; break;
    case DLPlusContentType::ITEM_GENRE: debug << "DLPlusContentType::ITEM_GENRE"; break;
    case DLPlusContentType::INFO_NEWS: debug << "DLPlusContentType::INFO_NEWS"; break;
    case DLPlusContentType::INFO_NEWS_LOCAL: debug << "DLPlusContentType::INFO_NEWS_LOCAL"; break;
    case DLPlusContentType::INFO_STOCKMARKET: debug << "DLPlusContentType::INFO_STOCKMARKET"; break;
    case DLPlusContentType::INFO_SPORT: debug << "DLPlusContentType::INFO_SPORT"; break;
    case DLPlusContentType::INFO_LOTTERY: debug << "DLPlusContentType::INFO_LOTTERY"; break;
    case DLPlusContentType::INFO_HOROSCOPE: debug << "DLPlusContentType::INFO_HOROSCOPE"; break;
    case DLPlusContentType::INFO_DAILY_DIVERSION: debug << "DLPlusContentType::INFO_DAILY_DIVERSION"; break;
    case DLPlusContentType::INFO_HEALTH: debug << "DLPlusContentType::INFO_HEALTH"; break;
    case DLPlusContentType::INFO_EVENT: debug << "DLPlusContentType::INFO_EVENT"; break;
    case DLPlusContentType::INFO_SCENE: debug << "DLPlusContentType::INFO_SCENE"; break;
    case DLPlusContentType::INFO_CINEMA: debug << "DLPlusContentType::INFO_CINEMA"; break;
    case DLPlusContentType::INFO_TV: debug << "DLPlusContentType::INFO_TV"; break;
    case DLPlusContentType::INFO_DATE_TIME: debug << "DLPlusContentType::INFO_DATE_TIME"; break;
    case DLPlusContentType::INFO_WEATHER: debug << "DLPlusContentType::INFO_WEATHER"; break;
    case DLPlusContentType::INFO_TRAFFIC: debug << "DLPlusContentType::INFO_TRAFFIC"; break;
    case DLPlusContentType::INFO_ALARM: debug << "DLPlusContentType::INFO_ALARM"; break;
    case DLPlusContentType::INFO_ADVERTISEMENT: debug << "DLPlusContentType::INFO_ADVERTISEMENT"; break;
    case DLPlusContentType::INFO_URL: debug << "DLPlusContentType::INFO_URL"; break;
    case DLPlusContentType::INFO_OTHER: debug << "DLPlusContentType::INFO_OTHER"; break;
    case DLPlusContentType::STATIONNAME_SHORT: debug << "DLPlusContentType::STATIONNAME_SHORT"; break;
    case DLPlusContentType::STATIONNAME_LONG: debug << "DLPlusContentType::STATIONNAME_LONG"; break;
    case DLPlusContentType::PROGRAMME_NOW: debug << "DLPlusContentType::PROGRAMME_NOW"; break;
    case DLPlusContentType::PROGRAMME_NEXT: debug << "DLPlusContentType::PROGRAMME_NEXT"; break;
    case DLPlusContentType::PROGRAMME_PART: debug << "DLPlusContentType::PROGRAMME_PART"; break;
    case DLPlusContentType::PROGRAMME_HOST: debug << "DLPlusContentType::PROGRAMME_HOST"; break;
    case DLPlusContentType::PROGRAMME_EDITORIAL_STAFF: debug << "DLPlusContentType::PROGRAMME_EDITORIAL_STAFF"; break;
    case DLPlusContentType::PROGRAMME_FREQUENCY: debug << "DLPlusContentType::PROGRAMME_FREQUENCY"; break;
    case DLPlusContentType::PROGRAMME_HOMEPAGE: debug << "DLPlusContentType::PROGRAMME_HOMEPAGE"; break;
    case DLPlusContentType::PROGRAMME_SUBCHANNEL: debug << "DLPlusContentType::PROGRAMME_SUBCHANNEL"; break;
    case DLPlusContentType::PHONE_HOTLINE: debug << "DLPlusContentType::PHONE_HOTLINE"; break;
    case DLPlusContentType::PHONE_STUDIO: debug << "DLPlusContentType::PHONE_STUDIO"; break;
    case DLPlusContentType::PHONE_OTHER: debug << "DLPlusContentType::PHONE_OTHER"; break;
    case DLPlusContentType::SMS_STUDIO: debug << "DLPlusContentType::SMS_STUDIO"; break;
    case DLPlusContentType::SMS_OTHER: debug << "DLPlusContentType::SMS_OTHER"; break;
    case DLPlusContentType::EMAIL_HOTLINE: debug << "DLPlusContentType::EMAIL_HOTLINE"; break;
    case DLPlusContentType::EMAIL_STUDIO: debug << "DLPlusContentType::EMAIL_STUDIO"; break;
    case DLPlusContentType::EMAIL_OTHER: debug << "DLPlusContentType::EMAIL_OTHER"; break;
    case DLPlusContentType::MMS_OTHER: debug << "DLPlusContentType::MMS_OTHER"; break;
    case DLPlusContentType::CHAT: debug << "DLPlusContentType::CHAT"; break;
    case DLPlusContentType::CHAT_CENTER: debug << "DLPlusContentType::CHAT_CENTER"; break;
    case DLPlusContentType::VOTE_QUESTION: debug << "DLPlusContentType::VOTE_QUESTION"; break;
    case DLPlusContentType::VOTE_CENTRE: debug << "DLPlusContentType::VOTE_CENTRE"; break;
        // rfu = 54
        // rfu = 55
    case DLPlusContentType::PRIVATE_1: debug << "DLPlusContentType::PRIVATE_1"; break;
    case DLPlusContentType::PRIVATE_2: debug << "DLPlusContentType::PRIVATE_2"; break;
    case DLPlusContentType::PRIVATE_3: debug << "DLPlusContentType::PRIVATE_3"; break;
    case DLPlusContentType::DESCRIPTOR_PLACE: debug << "DLPlusContentType::DESCRIPTOR_PLACE"; break;
    case DLPlusContentType::DESCRIPTOR_APPOINTMENT: debug << "DLPlusContentType::DESCRIPTOR_APPOINTMENT"; break;
    case DLPlusContentType::DESCRIPTOR_IDENTIFIER: debug << "DLPlusContentType::DESCRIPTOR_IDENTIFIER"; break;
    case DLPlusContentType::DESCRIPTOR_PURCHASE: debug << "DLPlusContentType::DESCRIPTOR_PURCHASE"; break;
    case DLPlusContentType::DESCRIPTOR_GET_DATA: debug << "DLPlusContentType::DESCRIPTOR_GET_DATA"; break;
    default: debug << int(type); break;
    }

    return debug;
}
