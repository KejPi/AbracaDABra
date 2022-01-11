#include <QDebug>
#include "motdecoder.h"
#include "mscdatagroup.h"

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

    MSCDataGroup mscDataGroup(dataGroup);
    if (!mscDataGroup.isValid())
    {   // data group was not valid - wrong CRC, or length
        return;
    }
    else
    { /* datagroup was valid */ }

    // unsigned required
    //QByteArray::const_iterator dataFieldIt = mscDataGroup.dataFieldConstBegin();
    const uint8_t * dataFieldIt = reinterpret_cast<const uint8_t *>(mscDataGroup.dataFieldConstBegin());

    // [ETSI EN 301 234, 5.1.1 Segmentation header]
    uint8_t repetitionCount = (*dataFieldIt >> 5) & 0x7;

    // [ETSI EN 301 234, 5.1 Segmentation of MOT entities]
    // MOT entities will be split up in segments with equal size.
    // Only the last segment may have a smaller size (to carry the remaining bytes of the MOT entity).
    uint16_t segmentSize = (*dataFieldIt++ << 8) & 0x1FFF;
    segmentSize += *dataFieldIt++;

#if MOTDECODER_VERBOSE
    qDebug() << "Segment number = "<< mscDataGroup.getSegmentNum() << ", last = " << mscDataGroup.getLastFlag();
    qDebug() << "Segment size = " << segmentSize << ", Repetition count = " << repetitionCount;
#endif

    switch (mscDataGroup.getType())
    {
    case 3:
    {   // [ETSI EN 301 234, 5.1.4 Segmentation of the MOT header]
        // The segments of the MOT header shall be transported in MSC Data group type 3.

        // this is header mode
        int motObjIdx = findMotObj(mscDataGroup.getTransportId());
        if (motObjIdx < 0)
        {  // object does not exist in list
#if MOTDECODER_VERBOSE
            qDebug() << "New MOT header" << mscDataGroup.getTransportId();
#endif
            // all existing object shall be removed, only one MOT object is transmitted in header mode
            motObjList.clear();

            // add new object to list
            motObjIdx = addMotObj(MOTObject(mscDataGroup.getTransportId()));
        }
        else
        {  /* do nothing - it already exists, just adding next segment */ }

        // add header segment
        motObjList[motObjIdx].addSegment((const uint8_t *) dataFieldIt, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag(), true);
    }
        break;
    case 4:
    {   // [ETSI EN 301 234, 5.1.2 Segmentation of the MOT body]
        // If conditional access on MOT level is applied, then scrambled MOT body segments
        // shall be transported in MSC data group type 5. In all other cases (no scrambling on MOT level
        // or unscrambled MOT body segments) the segments of the MOT body shall be transported in MSC data group type 4.

        int motObjIdx = findMotObj(mscDataGroup.getTransportId());
        if (motObjIdx < 0)
        {   // does not exist in list -> body without header
            // add new object to list
#if MOTDECODER_VERBOSE
            qDebug() << "New MOT object" << mscDataGroup.getTransportId();
#endif
            motObjIdx = addMotObj(MOTObject(mscDataGroup.getTransportId()));
        }
        motObjList[motObjIdx].addSegment((const uint8_t *) dataFieldIt, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag());

        if (motObjList[motObjIdx].isComplete())
        {
            QVector<uint8_t> body = motObjList[motObjIdx].getBody();
#if MOTDECODER_VERBOSE
            qDebug() << "MOT complete :)";
#endif // MOTDECODER_VERBOSE
            emit motObjectComplete(QByteArray((const char *) body.data(), body.size()));
        }
    }
        break;
    case 6:
    case 7:
        // directory not supported
        // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
        // The segments of an uncompressed MOT directory shall be transported in MSC Data Group type 6.
        // The segments of a compressed MOT directory shall be transported in MSC Data Group type 7.
        qDebug() << "MOT directory not supported";
        break;
    default:
        return;
    }
}

bool MOTDecoder::crc16check(const QByteArray & data)
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

int MOTDecoder::findMotObj(uint16_t transportId)
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

int MOTDecoder::addMotObj(const MOTObject & obj)
{
    motObjList.append(obj);
    return motObjList.size()-1;
}


