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

#include "txtablemodelitem.h"

TxTableModelItem::TxTableModelItem()
{}

TxTableModelItem::TxTableModelItem(int8_t mainId, int8_t subId, float level, const QGeoCoordinate &coordinates, const QList<TxDataItem *> txItemList)
{
    setTII(mainId, subId);
    setLevel(level);

    TxDataItem *tx = nullptr;
    float distance = 0;
    for (const auto &txItem : txItemList)
    {
        if ((txItem->subId() == subId) && (txItem->mainId() == mainId))
        {
            // tx = txItem;
            // break;

            if (coordinates.isValid())
            {
                if (tx == nullptr)
                {
                    distance = coordinates.distanceTo(txItem->coordinates());
                    tx = txItem;
                }
                else
                {
                    float d = coordinates.distanceTo(txItem->coordinates());
                    if (txItem->power() * (distance * distance) > tx->power() * (d * d))
                    {
                        tx = txItem;
                        distance = d;
                    }
                }
            }
            else
            {
                tx = txItem;
                break;
            }
        }
    }
    if (tx != nullptr)
    {  // found transmitter record
        setTransmitterData(*tx);

        if (coordinates.isValid())
        {  // we can calculate distance and azimuth
            setDistance(coordinates.distanceTo(tx->coordinates()) * 0.001);
            setAzimuth(coordinates.azimuthTo(tx->coordinates()));
        }
        else
        {
            setDistance(-1.0);
            setAzimuth(-1.0);
        }
    }
}

float TxTableModelItem::level() const
{
    return m_level;
}

int8_t TxTableModelItem::mainId() const
{
    return m_mainId;
}

void TxTableModelItem::setTII(int8_t newMainId, int8_t newSubId)
{
    m_mainId = newMainId;
    m_subId = newSubId;
    m_id = (newSubId << 8) | newMainId;
}

int8_t TxTableModelItem::subId() const
{
    return m_subId;
}

float TxTableModelItem::distance() const
{
    return m_distance;
}

void TxTableModelItem::setDistance(float newDistance)
{
    m_distance = newDistance;
}

float TxTableModelItem::azimuth() const
{
    return m_azimuth;
}

void TxTableModelItem::setAzimuth(float newAzimuth)
{
    m_azimuth = newAzimuth;
}

int TxTableModelItem::id() const
{
    return m_id;
}

void TxTableModelItem::updateGeo(const QGeoCoordinate &coordinates)
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

float TxTableModelItem::power() const
{
    if (hasTxData())
    {
        return m_transmitterData.power();
    }
    return 0.0;
}

void TxTableModelItem::setLevel(float newLevel)
{
    m_level = newLevel;
}

const TxDataItem &TxTableModelItem::transmitterData() const
{
    return m_transmitterData;
}

void TxTableModelItem::setTransmitterData(const TxDataItem &newTransmitterData)
{
    m_transmitterData = newTransmitterData;
}

void TxTableModelItem::setEnsData(const ServiceListId &ensId, const QString &ensLabel, int numServices, float snr)
{
    m_ensId = ensId;
    m_ensLabel = ensLabel;
    m_numServices = numServices;
    m_snr = snr;
}

void TxTableModelItem::setEnsConfig(const QString &config, const QString &configCSV)
{
    m_ensConfig = config;
    m_ensConfigCSV = configCSV;
}

ServiceListId TxTableModelItem::ensId() const
{
    return m_ensId;
}

QString TxTableModelItem::ensLabel() const
{
    return m_ensLabel;
}

int TxTableModelItem::numServices() const
{
    return m_numServices;
}

QDateTime TxTableModelItem::rxTime() const
{
    return m_rxTime;
}

void TxTableModelItem::setRxTime(const QDateTime &newRxTime)
{
    m_rxTime = newRxTime;
}

QString TxTableModelItem::ensConfig() const
{
    return m_ensConfig;
}

QString TxTableModelItem::ensConfigCSV() const
{
    return m_ensConfigCSV;
}

float TxTableModelItem::snr() const
{
    return m_snr;
}
