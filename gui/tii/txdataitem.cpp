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

#include "txdataitem.h"

TxDataItem::TxDataItem()
{}

QString TxDataItem::ensLabel() const
{
    return m_ensLabel;
}

void TxDataItem::setEnsLabel(const QString &newEnsLabel)
{
    m_ensLabel = newEnsLabel;
}

QString TxDataItem::location() const
{
    return m_location;
}

void TxDataItem::setLocation(const QString &newLocation)
{
    m_location = newLocation;
}

ServiceListId TxDataItem::ensId() const
{
    return m_ensId;
}

void TxDataItem::setEnsId(const ServiceListId &newEnsId)
{
    m_ensId = newEnsId;
}

uint8_t TxDataItem::mainId() const
{
    return m_mainId;
}

void TxDataItem::setMainId(uint8_t newMainId)
{
    m_mainId = newMainId;
}

uint8_t TxDataItem::subId() const
{
    return m_subId;
}

void TxDataItem::setSubId(uint8_t newSubId)
{
    m_subId = newSubId;
}

QGeoCoordinate TxDataItem::coordinates() const
{
    return m_coordinates;
}

void TxDataItem::setCoordinates(const QGeoCoordinate &newCoordinates)
{
    m_coordinates = newCoordinates;
}

float TxDataItem::power() const
{
    return m_power;
}

void TxDataItem::setPower(float newPower)
{
    m_power = newPower;
}
