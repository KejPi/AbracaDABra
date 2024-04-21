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

float TiiTableModelItem::level() const
{
    return m_level;
}

uint8_t TiiTableModelItem::mainId() const
{
    return m_mainId;
}

void TiiTableModelItem::setMainId(uint8_t newMainId)
{
    m_mainId = newMainId;
}

uint8_t TiiTableModelItem::subId() const
{
    return m_subId;
}

void TiiTableModelItem::setSubId(uint8_t newSubId)
{
    m_subId = newSubId;
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
