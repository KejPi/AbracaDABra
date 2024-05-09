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

#include "tiitablemodelitem.h"

TiiTableModelItem::TiiTableModelItem() {}

TiiTableModelItem::TiiTableModelItem(uint8_t mainId, uint8_t subId, float level, const QGeoCoordinate &coordinates, const QList<TxDataItem *> txItemList)
{
    setTII(mainId, subId);
    setLevel(level);

    TxDataItem * tx = nullptr;
    for (const auto & txItem : txItemList)
    {
        if ((txItem->subId() == subId) && (txItem->mainId() == mainId))
        {
            tx = txItem;
            break;
        }
    }
    if (tx != nullptr)
    {   // found transmitter record
        setTransmitterData(*tx);

        if (coordinates.isValid())
        {   // we can calculate distance and azimuth
            setDistance(coordinates.distanceTo(tx->coordinates()) * 0.001);
            setAzimuth(coordinates.azimuthTo(tx->coordinates()));
        }
    }
}

float TiiTableModelItem::level() const
{
    return m_level;
}

uint8_t TiiTableModelItem::mainId() const
{
    return m_mainId;
}

void TiiTableModelItem::setTII(uint8_t newMainId, int8_t newSubId)
{
    m_mainId = newMainId;
    m_subId = newSubId;
    m_id = (newSubId << 8) | newMainId;
}

uint8_t TiiTableModelItem::subId() const
{
    return m_subId;
}

float TiiTableModelItem::distance() const
{
    return m_distance;
}

void TiiTableModelItem::setDistance(float newDistance)
{
    m_distance = newDistance;
}

float TiiTableModelItem::azimuth() const
{
    return m_azimuth;
}

void TiiTableModelItem::setAzimuth(float newAzimuth)
{
    m_azimuth = newAzimuth;
}

int TiiTableModelItem::id() const
{
    return m_id;
}

void TiiTableModelItem::updateGeo(const QGeoCoordinate &coordinates)
{
    if (m_transmitterData.isValid() && coordinates.isValid())
    {
        setDistance(coordinates.distanceTo(m_transmitterData.coordinates()) * 0.001);
        setAzimuth(coordinates.azimuthTo(m_transmitterData.coordinates()));
    }
    else
    {
        setDistance(-1.0);
        setAzimuth(-1.0);
    }
}

void TiiTableModelItem::setLevel(float newLevel)
{
    m_level = newLevel;
}

const TxDataItem & TiiTableModelItem::transmitterData() const
{
    return m_transmitterData;
}

void TiiTableModelItem::setTransmitterData(const TxDataItem &newTransmitterData)
{
    m_transmitterData = newTransmitterData;
}
