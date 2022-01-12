#include <QDebug>
#include "motdecoder.h"
#include "mscdatagroup.h"

MOTDecoder::MOTDecoder(QObject *parent) : QObject(parent)
{
    directory = nullptr;
    objCache = new MOTObjectCache;
}

MOTDecoder::~MOTDecoder()
{
    if (nullptr != directory)
    {
        delete directory;
    }
    delete objCache;
}

void MOTDecoder::reset()
{
    objCache->clear();
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
        MOTObject * objPtr = objCache->findMotObj(mscDataGroup.getTransportId());
        if (nullptr == objPtr)
        {  // object does not exist in cache
#if MOTDECODER_VERBOSE
            qDebug() << "New MOT header ID" << mscDataGroup.getTransportId() << "number of objects in carousel" << objCache->size();
#endif
            // all existing object shall be removed, only one MOT object is transmitted in header mode
            objCache->clear();

            // add new object to cache
            objPtr = objCache->addMotObj(new MOTObject(mscDataGroup.getTransportId()));
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

        // first check if we are in direcory mode
        if (nullptr != directory)
        {  // directory mode
            directory->addObjectSegment(mscDataGroup.getTransportId(), (const uint8_t *) dataFieldPtr, mscDataGroup.getSegmentNum(),
                                        segmentSize, mscDataGroup.getLastFlag());
        }
        else
        {   // this can be euth directory mode but directoy was not recieved yet or it can be header mode
            // Header mode is handled within the cache
            MOTObject * objPtr = objCache->findMotObj(mscDataGroup.getTransportId());
            if (nullptr == objPtr)
            {   // does not exist in cache -> body without header
                // add new object to cache
#if MOTDECODER_VERBOSE
                qDebug() << "New MOT object ID" << mscDataGroup.getTransportId() << "number of objects in carousel" << objCache->size();
#endif
                objPtr = objCache->addMotObj(new MOTObject(mscDataGroup.getTransportId()));
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
    }
        break;
    case 6:
        // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
        // The segments of an uncompressed MOT directory shall be transported in MSC Data Group type 6.
        if (nullptr != directory)
        {   // some directory exists
            // lets check if the segment belong to current directory
            if (directory->getTransportId() != mscDataGroup.getTransportId())
            {   // new directory
                delete directory;
                directory = new MOTDirectory(mscDataGroup.getTransportId(), objCache);
            }
            else
            { /* segment belongs to existing directory */ }
        }
        else
        {   // directory does not exist -> creating new directory
            directory = new MOTDirectory(mscDataGroup.getTransportId(), objCache);
        }

        if (directory->addSegment((const uint8_t *) dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag()))
        {
            qDebug() << "MOT Directory is complete";
        }
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

