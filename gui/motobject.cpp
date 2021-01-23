#include "motobject.h"

#include <QDebug>

MOTObject::MOTObject(quint16 transportId, const quint8 *segment, quint16 segmenLen, bool lastFlag)
{
    id = transportId;

    // [6.1 Header core]
    bodySize = (segment[0] << 20) | (segment[1] << 12) | (segment[2] << 4) | ((segment[3] >> 4) & 0x0F);
    headerSize = ((segment[3] & 0x0F) << 9) | (segment[4] << 1) | ((segment[5] >> 7) & 0x01);
    contentType = (segment[5] >> 1) & 0x3F;
    contentSubType = ((segment[5] & 0x01) << 8) | segment[6];

    if (lastFlag && headerSize != segmenLen)
    {
        qDebug() << "Unexpected segment length";
    }
#if MOTDECODER_VERBOSE
    qDebug() << bodySize << headerSize << contentType << contentSubType;
#endif
#if 0 // not implemented at the moment
    for (int n = 7; n<headerSize; ++n)
    {
        quint8 PLI = (segment[n] >> 6) & 0x3;
        quint8 paramId = segment[n] & 0x3F;
        switch (PLI)
        {
        case 0:
            break;
        case 1:
            break;
        case 2:
            break;
        case 3:
            break;
        }
    }
#endif
    reset();
}

void MOTObject::reset()
{
    // empty body
    numBodySegments = 0;
    bodyComplete = false;
    body.clear();
}

void MOTObject::addBodySegment(const quint8 * segment, quint16 segmentNum, quint16 segmentSize, bool lastFlag)
{
    if ((segmentNum >= 8192) || (segmentSize == 0))
    {
        return;
    }

    if (lastFlag)
    {
        numBodySegments = segmentNum+1;
    }

    // all segments have the same size only the last can be shorter
    if (segmentNum > body.size())
    {
        // insert empty items
        for (int n = body.size(); n<segmentNum; ++n)
        {
            body.append(QByteArray());
        }
    }

    if (segmentNum < body.size())
    {
        body.replace(segmentNum, QByteArray((const char *)segment, segmentSize));
    }
    else if (segmentNum == body.size())
    {
        body.append(QByteArray((const char *)segment, segmentSize));
    }


    if (numBodySegments == 0)
    {   // last segment was not received yet
        return;
    }
    // last segment was already received => we could have complete body
    // check is all segments are non zero and the same size
    int expectedSize = body[0].size();
    for (int n = 0; n<numBodySegments-1; ++n)
    {
        if (body[n].size() == 0 || body[n].size() != expectedSize)
        {
            return;
        }
    }
    bodyComplete = true;
}

void MOTObject::getBody(QByteArray & b)
{
    if (bodyComplete)
    {
        // body is complete
         b = body.join();
    }
}
