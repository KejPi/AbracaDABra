/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <QDebug>
#include "mscdatagroup.h"

MSCDataGroup::MSCDataGroup(const QByteArray &dataGroup)
{
    m_isValid = false;
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
    m_extensionFlag = (*inputDataPtr & 0x80) != 0;    // byte 0, bit 7
    m_segmentFlag = (*inputDataPtr & 0x20) != 0;      // byte 0, bit 5
    m_userAccessFlag = (*inputDataPtr & 0x10) != 0;   // byte 0, bit 4
    m_type = (*inputDataPtr++ & 0x0F);    // byte 0, bits 3-0

    // inputDataPtr->byte 1
    m_continuityIdx = (*inputDataPtr >> 4) & 0x0F; // byte 1, bits 7-4
    m_repetitionIdx = (*inputDataPtr++ & 0x0F);    // byte 1, bits 3-0

    // inputDataPtr->byte 2 (extension field)
    if (m_extensionFlag)
    {   // extension field is present
        // Extension field: this 16-bit field shall be used to carry information for CA on data group level
        // (see ETSI TS 102 367 [4]). For other Data group types, the Extension field is reserved for future
        // additions to the Data group header.
        m_extensionField = (*inputDataPtr << 8) | (*inputDataPtr+1);
        inputDataPtr += 2;
    }
    else
    { /* extension field is not present */ }

    // Session header [ETSI EN 300 401, 5.3.3.2]
    // inputDataPtr->segment field
    m_lastFlag = false;
    m_segmentNum = 0;
    if (m_segmentFlag)
    {   // segment field is present
        m_lastFlag = (*inputDataPtr & 0x80) != 0;
        m_segmentNum = (*inputDataPtr++ << 8) & 0x7FFF;
        m_segmentNum += *inputDataPtr++;
    }
    else
    { /* no segment field */ }

    m_transportIdFlag = false;
    m_lengthIndicator = 0;
    m_transportId = 0;
    if (m_userAccessFlag)
    {
        m_transportIdFlag = (*inputDataPtr & 0x10) != 0;
        m_lengthIndicator = (*inputDataPtr++ & 0x0F);
        if (m_transportIdFlag)
        {
            m_transportId = (*inputDataPtr << 8) | *(inputDataPtr+1);
            inputDataPtr += 2;
        }
        else
        { /* transport ID is not present */ }

        if (m_lengthIndicator - m_transportIdFlag * 2 > 0)
        {   // end user address field is present
            m_endUserAddrField = QByteArray((const char *) inputDataPtr, (m_lengthIndicator - m_transportIdFlag * 2));
            inputDataPtr += m_lengthIndicator - m_transportIdFlag * 2;
        }
        else
        {  /* no end user address field */ }
    }
    else
    {  }

    // inputDataPtr -> beginning of MSC data group data field
    int dataFieldSize = dataGroup.size() - (inputDataPtr - (const uint8_t *) dataGroup.constBegin()) - crcFlag * 2;

    m_dataField = QByteArray((const char *) inputDataPtr,  dataFieldSize);

    m_isValid = true;
}

QByteArray::const_iterator MSCDataGroup::dataFieldConstBegin() const
{
    return m_dataField.constBegin();
}

uint8_t MSCDataGroup::getType() const
{
    return m_type;
}

uint16_t MSCDataGroup::getSegmentNum() const
{
    return m_segmentNum;
}

uint16_t MSCDataGroup::getTransportId() const
{
    return m_transportId;
}

bool MSCDataGroup::getLastFlag() const
{
    return m_lastFlag;
}

bool MSCDataGroup::isValid() const
{
    return m_isValid;
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
