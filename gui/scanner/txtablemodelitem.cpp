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

#include "txtablemodelitem.h"

TxTableModelItem::TxTableModelItem() {}

TxTableModelItem::TxTableModelItem(uint8_t mainId, uint8_t subId, float level, const QGeoCoordinate &coordinates, const QList<TxDataItem *> txItemList)
    : TiiTableModelItem(mainId, subId, level, coordinates, txItemList)
{
}

void TxTableModelItem::setEnsData(const ServiceListId &ensId, const QString &ensLabel, int numServices, float snr)
{
    m_ensId = ensId;
    m_ensLabel = ensLabel;
    m_numServices = numServices;
    m_snr = snr;
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

float TxTableModelItem::snr() const
{
    return m_snr;
}

