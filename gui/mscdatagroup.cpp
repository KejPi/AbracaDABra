#include <QDebug>
#include "mscdatagroup.h"


MSCDataGroup::MSCDataGroup(const QByteArray &dataGroup)
{
    valid = false;
    if (dataGroup.size() <= 0)
    {  // no data
        return;
    }
    else
    { /* data available */ }

    // some data available => first check header
    const uint8_t * inputDataPtr = (const uint8_t *) dataGroup.constBegin();
    bool crcFlag = (*inputDataPtr & 0x40) != 0;
    if (crcFlag)  // bit 6
    {   // if CRC present, do check CRC16 according to [ETSI EN 300 401, 5.3.3.4]
        if (!crc16check(dataGroup))
        {
            qDebug() << "CRC failed";
            return;
        }
        else
        { /* CRC OK */ }
    }
    else
    { /* no CRC */ }

    // Data group header [ETSI EN 300 401, 5.3.3.1]
    extensionFlag = (*inputDataPtr & 0x80) != 0;    // byte 0, bit 7
    segmentFlag = (*inputDataPtr & 0x20) != 0;      // byte 0, bit 5
    userAccessFlag = (*inputDataPtr & 0x10) != 0;   // byte 0, bit 4
    type = (*inputDataPtr++ & 0x0F);    // byte 0, bits 3-0

    // inputDataPtr->byte 1
    continuityIdx = (*inputDataPtr >> 4) & 0x0F; // byte 1, bits 7-4
    repetitionIdx = (*inputDataPtr++ & 0x0F);    // byte 1, bits 3-0

    // inputDataPtr->byte 2 (extension field)
    if (extensionFlag)
    {   // extension field is present
        // Extension field: this 16-bit field shall be used to carry information for CA on data group level
        // (see ETSI TS 102 367 [4]). For other Data group types, the Extension field is reserved for future
        // additions to the Data group header.
        extensionField = (*inputDataPtr << 8) | (*inputDataPtr+1);
        inputDataPtr += 2;
    }
    else
    { /* extension field is not present */ }

    // Session header [ETSI EN 300 401, 5.3.3.2]
    // inputDataPtr->segment field
    lastFlag = false;
    segmentNum = 0;
    if (segmentFlag)
    {   // segment field is present
        lastFlag = (*inputDataPtr & 0x80) != 0;
        segmentNum = (*inputDataPtr++ << 8) & 0x7FFF;
        segmentNum += *inputDataPtr++;
    }
    else
    { /* no segment field */ }

    transportIdFlag = false;
    lengthIndicator = 0;
    transportId = 0;
    if (userAccessFlag)
    {
        transportIdFlag = (*inputDataPtr & 0x10) != 0;
        lengthIndicator = (*inputDataPtr++ & 0x0F);
        if (transportIdFlag)
        {
            transportId = (*inputDataPtr << 8) | *(inputDataPtr+1);
            inputDataPtr += 2;
        }
        else
        { /* transport ID is not present */ }

        if (lengthIndicator - transportIdFlag * 2 > 0)
        {   // end user address field is present
            endUserAddrField = QByteArray((const char *) inputDataPtr, (lengthIndicator - transportIdFlag * 2));
            inputDataPtr += lengthIndicator - transportIdFlag * 2;
        }
        else
        {  /* no end user address field */ }
    }
    else
    {  }

    // inputDataPtr -> beginning of MSC data group data field
    int dataFieldSize = dataGroup.size() - (inputDataPtr - (const uint8_t *) dataGroup.constBegin()) - crcFlag * 2;

    dataField = QByteArray((const char *) inputDataPtr,  dataFieldSize);

    valid = true;
}

QByteArray::const_iterator MSCDataGroup::dataFieldConstBegin() const
{
    return dataField.constBegin();
}

uint8_t MSCDataGroup::getType() const
{
    return type;
}

uint16_t MSCDataGroup::getSegmentNum() const
{
    return segmentNum;
}

uint16_t MSCDataGroup::getTransportId() const
{
    return transportId;
}

bool MSCDataGroup::getLastFlag() const
{
    return lastFlag;
}

bool MSCDataGroup::isValid() const
{
    return valid;
}

bool MSCDataGroup::crc16check(const QByteArray & data)
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

    uint16_t invTxCRC = ~((uint16_t(data[data.size()-2]) << 8) | uint8_t(data[data.size()-1]));

    return (crc == invTxCRC);
}
