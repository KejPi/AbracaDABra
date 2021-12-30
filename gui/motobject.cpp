#include "motobject.h"
#include "dabtables.h"
#include <QDebug>

MOTObject::MOTObject(uint16_t transportId, const uint8_t *segment, uint16_t segmenLen, bool lastFlag)
{
    id = transportId;

    // [ETSI EN 301 234, 6.1 Header core]
    bodySize = (segment[0] << 20) | (segment[1] << 12) | (segment[2] << 4) | ((segment[3] >> 4) & 0x0F);
    headerSize = ((segment[3] & 0x0F) << 9) | (segment[4] << 1) | ((segment[5] >> 7) & 0x01);
    headerParams.contentType = (segment[5] >> 1) & 0x3F;
    headerParams.contentSubType = ((segment[5] & 0x01) << 8) | segment[6];

    if (lastFlag && headerSize != segmenLen)
    {
        qDebug() << "Unexpected segment length";
    }
#if MOTDECODER_VERBOSE
    qDebug() << bodySize << headerSize << headerParams.contentType << headerParams.contentSubType;
#endif
#if 0 // not implemented at the moment
    QString header;
    for (int d = 0; d < headerSize-7; ++d)
    {
        header += QString("%1 ").arg(segment[d+7], 2, 16, QLatin1Char('0'));
    }
    qDebug() << header;

    int n = 7;
    while (n<headerSize)
    {        
        uint8_t PLI = (segment[n] >> 6) & 0x03;
        uint8_t paramId = segment[n++] & 0x3F;
        uint_fast8_t dataFieldLen = 0;

        switch (PLI)
        {
        case 0:
            // do nothing here
            break;
        case 1:
            dataFieldLen = 1;
            break;
        case 2:            
            dataFieldLen = 4;
            break;
        case 3:
            if (n+1 < headerSize)
            {
                uint16_t dataLengthIndicator = segment[n] & 0x7F;
                if (segment[n++] & 0x80)
                {
                    if (n < headerSize)
                    {
                        dataLengthIndicator <<= 8;
                        dataLengthIndicator |= segment[n++];
                    }
                    else
                    {   // somethign is wrong
                        break;
                    }
                }
                dataFieldLen = dataLengthIndicator;
            }
            break;
        }

        // segment[n] is first byte of data field if dataFieldLen > 0

        if (n + dataFieldLen <= headerSize)
        {
            QString data;
            for (int d = 0; d < dataFieldLen; ++d)
            {
                data += QString("%1 ").arg(segment[n+d], 2, 16, QLatin1Char('0'));
            }
            qDebug("%s: PLI=%d, ParamID = 0x%2.2X, DataLength = %d: DataField = %s",
                   Q_FUNC_INFO, PLI, paramId, dataFieldLen, data.toStdString().c_str());
        }
        else
        { /* something went wrong */ }

        if (DabMotExtParameter::CAInfo == DabMotExtParameter(paramId))
        {   // ignoring scrembled data
            qDebug() << "MOT CA scrambled ignoring";
            bodySize = 0;
        }
        if (DabMotExtParameter::CompressionType == DabMotExtParameter(paramId))
        {   // ignoring compressed data
            qDebug() << "MOT compressed ignoring";
            bodySize = 0;
        }
        if (DabMotExtParameter::ContentName == DabMotExtParameter(paramId))
        {
            headerParams.ContentName = DabTables::convertToQString((const char*) &segment[n+1], ((segment[n] >> 4) & 0x0F), dataFieldLen-1);
            qDebug() << headerParams.ContentName;
        }
        if (DabMotExtParameter::MimeType == DabMotExtParameter(paramId))
        {
            headerParams.MimeType = DabTables::convertToQString((const char*) &segment[n],
                                                            static_cast<uint8_t>(DabCharset::LATIN1), dataFieldLen);
            qDebug() << headerParams.MimeType;
        }

        n += dataFieldLen;
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

void MOTObject::addBodySegment(const uint8_t * segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag)
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

    // [ETSI EN 301 234, 6.1 Header core]
    // BodySize: This 28-bit field, coded as an unsigned binary number, indicates the total size of the body in bytes.
    // If the body size signalled by this parameter does not correspond to the size of the reassembled MOT body, then the
    // MOT body shall be discarded.
    int receivedBodySize = 0;
    for (int n = 0; n<numBodySegments; ++n)
    {
        receivedBodySize += body[n].size();
    }
    //qDebug() << Q_FUNC_INFO << bodySize << receivedBodySize;
    if (bodySize == receivedBodySize)
    {
        bodyComplete = true;
    }
}

void MOTObject::getBody(QByteArray & b)
{
    if (bodyComplete)
    {
        // body is complete
         b = body.join();
    }
}
