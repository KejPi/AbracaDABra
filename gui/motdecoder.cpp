#include <QDebug>
#include "motdecoder.h"
#include "mscdatagroup.h"

MOTDecoder::MOTDecoder(QObject *parent) : QObject(parent)
{
    directory = nullptr;
}

MOTDecoder::~MOTDecoder()
{
    if (nullptr != directory)
    {
        delete directory;
    }
}

void MOTDecoder::reset()
{
    objCache.clear();
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
    const uint8_t * dataFieldPtr = reinterpret_cast<const uint8_t *>(mscDataGroup.dataFieldConstBegin());

    // [ETSI EN 301 234, 5.1.1 Segmentation header]
    uint8_t repetitionCount = (*dataFieldPtr >> 5) & 0x7;

    // [ETSI EN 301 234, 5.1 Segmentation of MOT entities]
    // MOT entities will be split up in segments with equal size.
    // Only the last segment may have a smaller size (to carry the remaining bytes of the MOT entity).
    uint16_t segmentSize = (*dataFieldPtr++ << 8) & 0x1FFF;
    segmentSize += *dataFieldPtr++;

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
        MOTObject * objPtr = objCache.findMotObj(mscDataGroup.getTransportId());
        if (nullptr == objPtr)
        {  // object does not exist in cache
#if MOTDECODER_VERBOSE
            qDebug() << "New MOT header ID" << mscDataGroup.getTransportId() << "number of objects in carousel" << objCache.size();;
#endif
            // all existing object shall be removed, only one MOT object is transmitted in header mode
            objCache.clear();

            // add new object to list

            objPtr = objCache.addMotObj(new MOTObject(mscDataGroup.getTransportId()));
        }
        else
        {  /* do nothing - it already exists, just adding next segment */ }

        // add header segment
        objPtr->addSegment((const uint8_t *) dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag(), true);
    }
        break;
    case 4:
    {   // [ETSI EN 301 234, 5.1.2 Segmentation of the MOT body]
        // If conditional access on MOT level is applied, then scrambled MOT body segments
        // shall be transported in MSC data group type 5. In all other cases (no scrambling on MOT level
        // or unscrambled MOT body segments) the segments of the MOT body shall be transported in MSC data group type 4.

        MOTObject * objPtr = objCache.findMotObj(mscDataGroup.getTransportId());
        if (nullptr == objPtr)
        {   // does not exist in cache -> body without header
            // add new object to cache
#if MOTDECODER_VERBOSE
            qDebug() << "New MOT object ID" << mscDataGroup.getTransportId() << "number of objects in carousel" << objCache.size();
#endif
            objPtr = objCache.addMotObj(new MOTObject(mscDataGroup.getTransportId()));
        }
        objPtr->addSegment((const uint8_t *) dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag());

        if (objPtr->isComplete())
        {
            QByteArray body = objPtr->getBody();
#if MOTDECODER_VERBOSE
            qDebug() << "MOT complete :)";
#endif // MOTDECODER_VERBOSE
            emit motObjectComplete(body);
        }
    }
        break;
    case 6:
        // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
        // The segments of an uncompressed MOT directory shall be transported in MSC Data Group type 6.
        if (nullptr != directory)
        {
            delete directory;
        }
        directory = new MOTDirectory(mscDataGroup.getTransportId());
        directory->addSegment((const uint8_t *) dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag());
        break;
    case 7:
        // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
        // The segments of a compressed MOT directory shall be transported in MSC Data Group type 7.
        qDebug() << "Compressed MOT directory not supported";
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

//=================================================================================
MOTObjectCache::MOTObjectCache()
{
}

MOTObjectCache::~MOTObjectCache()
{
    clear();
}

void MOTObjectCache::clear()
{
    for (int n = 0; n<cache.size(); ++n)
    {
        delete cache[n];
    }

    cache.clear();
}

MOTObject * MOTObjectCache::findMotObj(uint16_t transportId)
{
    for (int n = 0; n<cache.size(); ++n)
    {
        if (cache[n]->getId() == transportId)
        {
            return cache[n];
        }
    }
    return nullptr;
}

void MOTObjectCache::deleteMotObj(uint16_t transportId)
{
    for (int n = 0; n<cache.size(); ++n)
    {
        if (cache[n]->getId() == transportId)
        {
            delete cache[n];
            cache.removeAt(n);
            return;
        }
    }
}

MOTObject * MOTObjectCache::addMotObj(MOTObject *obj)
{
    cache.append(obj);
    return obj;
}



