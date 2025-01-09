/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef MSCDATAGROUP_H
#define MSCDATAGROUP_H

#include <QByteArray>

class MSCDataGroup
{
public:
    explicit MSCDataGroup(const QByteArray &dataGroup);
    QByteArray::const_iterator dataFieldConstBegin() const;

    uint8_t getType() const;
    uint16_t getSegmentNum() const;
    uint16_t getTransportId() const;
    bool getLastFlag() const;
    bool isValid() const;

private:
    bool m_isValid = false;
    bool m_extensionFlag;
    bool m_segmentFlag;
    bool m_userAccessFlag;
    uint8_t m_type;
    uint8_t m_continuityIdx;
    uint8_t m_repetitionIdx;
    uint16_t m_extensionField;
    bool m_lastFlag;
    uint16_t m_segmentNum;
    bool m_transportIdFlag;
    uint8_t m_lengthIndicator;
    uint16_t m_transportId;
    QByteArray m_endUserAddrField;
    QByteArray m_dataField;

    bool crc16check(const QByteArray &data);
};

#endif  // MSCDATAGROUP_H
