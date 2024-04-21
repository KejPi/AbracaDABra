/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef TIITABLEMODELITEM_H
#define TIITABLEMODELITEM_H

#include <stdint.h>
#include <txdataitem.h>

class TiiTableModelItem
{
public:
    TiiTableModelItem();

    bool haveTxData() const { return m_transmitterData.isValid(); }

    float level() const;
    void setLevel(float newLevel);

    const TxDataItem &transmitterData() const;
    void setTransmitterData(const TxDataItem &newTransmitterData);

    uint8_t mainId() const;
    void setMainId(uint8_t newMainId);

    uint8_t subId() const;
    void setSubId(uint8_t newSubId);

    float distance() const;
    void setDistance(float newDistance);

    float azimuth() const;
    void setAzimuth(float newAzimuth);

private:
    uint8_t m_mainId;   // main ID
    uint8_t m_subId;    // sub ID
    float m_level;      // signal level
    float m_distance;   // distance of the transmitter from current position (must be >= 0)
    float m_azimuth;    // azimuth of the transmitter from current position
    TxDataItem m_transmitterData;   // information about transmitter
};

#endif // TIITABLEMODELITEM_H
