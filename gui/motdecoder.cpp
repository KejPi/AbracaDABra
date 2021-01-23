#include <QDebug>
#include "motdecoder.h"

MOTDecoder::MOTDecoder(QObject *parent) : QObject(parent)
{

}

void MOTDecoder::reset()
{
    motObjList.clear();
}

void MOTDecoder::newDataGroup(const QByteArray &dataGroup)
{
#if MOTDECODER_VERBOSE
    qDebug() << Q_FUNC_INFO << "Data group len=" << dataGroup.size();
#endif

    if (dataGroup.size() <= 0)
    {  // no data
        return;
    }

    // some data available => first check header
    //QByteArray::const_iterator dgIt = dataGroup.constBegin();
    const quint8 * dgIt = (const quint8 *) dataGroup.constBegin();
    if (*dgIt & 0x40)
    {   // if CRC present, do check CRC16 according to [ETSI EN 300 401, 5.3.3.4]
        //quint16 chsum = qChecksum((const char *)dgIt, dataGroup.size(), Qt::ChecksumItuV41);
        if (!crc16check(dataGroup))
        {
            qDebug() << "CRC failed";
            return;
        }
        else
        {}  // CRC OK
    }
#if MOTDECODER_VERBOSE>1
    QString msg = "";
    int len = (dataGroup.size() < 45) ? dataGroup.size() : 45;
    for (int n = 0; n<len; ++n)
    {
        msg += QString("%1 ").arg(quint8(dataGroup.at(n)), 2, 16, QLatin1Char('0'));
    }
    qDebug() << msg;
#endif

    // Data group header [ETSI EN 300 401, 5.3.3.1]
    bool extensionFlag = (*dgIt & 0x80) != 0;
    bool segmentFlag = (*dgIt & 0x20) != 0;
    bool userAccessFlag = (*dgIt & 0x10) != 0;
    quint8 dataGroupType = (*dgIt++ & 0x0F);
    quint8 continuityIdx = (*dgIt >> 4) & 0x0F;
    quint8 repetitionIdx = (*dgIt++ & 0x0F);   // repetition on MSC data group level [ETSI EN 300 401, 5.3.1.2]
    if (extensionFlag)
    {   // skip extension field
        dgIt += 2;
    }
#if MOTDECODER_VERBOSE
    qDebug() << "continuityIdx=" << continuityIdx << ", repetitionIdx=" << repetitionIdx;
#endif

    // Session header [ETSI EN 300 401, 5.3.3.2]
    bool lastFlag = false;
    quint16 segmentNum = 0;
    if (segmentFlag)
    {
        lastFlag = (*dgIt & 0x80) != 0;
        segmentNum = (*dgIt++ << 8) & 0x7FFF;
        segmentNum += *dgIt++;
    }
    bool transportIdFlag = false;
    quint8 lengthIndicator = 0;
    quint16 transportId = 0;
    if (userAccessFlag)
    {
        transportIdFlag = (*dgIt & 0x10) != 0;
        lengthIndicator = (*dgIt++ & 0x0F);
        if (transportIdFlag)
        {
            transportId = (*dgIt << 8) & 0x7FFF;
            transportId += *(dgIt+1);
        }
        // skip enduser address field and transport ID
        dgIt += lengthIndicator;
    }
    else
    {   // [ETSI EN 301 234, 5.1]
        // The user access field in the Session header (see EN 300 401 [1]) is not optional
        // if MOT segments are carried in MSC data groups. It cannot be omitted, since this
        // field contains the TransportId, necessary for MOT object transfer.
        return;
    }
    // dgIt -> beginning of MSC data group data field
#if MOTDECODER_VERBOSE
    qDebug() << "Data Group type = " << dataGroupType << ", TransportID = "<< transportId;
#endif

    // [ETSI EN 301 234, 5.1.1 Segmentation header]
    quint8 repetitionCount = (*dgIt >> 5) & 0x7;
    quint16 segmentSize = (*dgIt++ << 8) & 0x1FFF;
    segmentSize += *dgIt++;
#if MOTDECODER_VERBOSE
    qDebug() << "Segment number = "<< segmentNum << ", last = " << lastFlag;
    qDebug() << "Segment size = " << segmentSize << ", Repetition count = " << repetitionCount;
#endif
    switch (dataGroupType)
    {
    case 3:
    {
        if (segmentNum == 0)
        {  // multiple header segment not suported at the moment
            int motObjIdx = findMotObj(transportId);
            if (motObjIdx < 0)
            {  // does not exist in list
#if MOTDECODER_VERBOSE
                qDebug() << "New MOT object";
#endif
                addMotObj(transportId, dgIt, segmentSize, lastFlag);
            }
        }
    }
        break;
    case 4:
    {
        int motObjIdx = findMotObj(transportId);
        if (motObjIdx < 0)
        {  // does not exist in list -> body without header
#if MOTDECODER_VERBOSE
            qDebug() << "Body without header, ignoring";
#endif
            break;
        }
        motObjList[motObjIdx].addBodySegment(dgIt, segmentNum, segmentSize, lastFlag);

        if (motObjList[motObjIdx].isBodyComplete())
        {
            QByteArray body;
            motObjList[motObjIdx].getBody(body);

            qDebug() << "MOT complete :)";
            emit motObjectComplete(body);

            //motObjList.removeAt(motObjIdx);
        }
    }
        break;
    default:
        // directory not supported
        return;
    }
}

bool MOTDecoder::crc16check(const QByteArray & data)
{
    quint16 crc = 0xFFFF;
    quint16 mask = 0x1020;

    for (int n = 0; n < data.size()-2; ++n)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            quint16 in = (data[n] >> bit) & 0x1;
            quint16 tmp = in ^ ((crc>>15) & 0x1);
            crc <<= 1;

            crc ^= (tmp * mask);
            crc += tmp;
        }
    }

    quint16 invTxCRC = ~(quint16(data[data.size()-2] << 8) | quint8(data[data.size()-1]));

    return (crc == invTxCRC);
}

int MOTDecoder::findMotObj(quint16 transportId)
{
    for (int n = 0; n<motObjList.size(); ++n)
    {
        if (motObjList[n].getId() == transportId)
        {
            return n;
        }
    }
    return -1;
}

int MOTDecoder::addMotObj(quint16 transportId, const quint8 *segment, quint16 segmenLen, bool lastFlag)
{
    motObjList.append(MOTObject(transportId, segment, segmenLen, lastFlag));
    if (motObjList.size() > 8)
    {  // remove the oldest
        motObjList.removeFirst();
    }
    return motObjList.size()-1;
}


