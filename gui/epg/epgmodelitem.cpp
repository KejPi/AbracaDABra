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

#include "epgmodelitem.h"

EPGModelItem::EPGModelItem()
{}

QString EPGModelItem::longName() const
{
    return m_longName;
}

void EPGModelItem::setLongName(const QString &newLongName)
{
    m_longName = newLongName;
}

QString EPGModelItem::mediumName() const
{
    return m_mediumName;
}

void EPGModelItem::setMediumName(const QString &newMediumName)
{
    m_mediumName = newMediumName;
}

QString EPGModelItem::shortName() const
{
    return m_shortName;
}

void EPGModelItem::setShortName(const QString &newShortName)
{
    m_shortName = newShortName;
}

QDateTime EPGModelItem::startTime() const
{
    return m_startTime;
}

int EPGModelItem::startTimeSec() const
{
    QTime time = m_startTime.time();
    return time.hour() * 3600 + time.minute() * 60 + time.second();
}

int EPGModelItem::endTimeSec() const
{
    return startTimeSec() + m_durationSec;
    // QTime endTime = m_startTime.addSecs(m_durationSec).time();
    // int ret = endTime.hour()*3600 + endTime.minute() * 60 + endTime.second();
    // if (ret < startTimeSec()) {
    //     ret += 24*3600;   // add one day day
    // }
    // return ret;
}

QDateTime EPGModelItem::endTime() const
{
    return m_startTime.addSecs(m_durationSec);
}

void EPGModelItem::setStartTime(const QDateTime &newStartTime)
{
    m_startTime = newStartTime;
    m_startTimeSecSinceEpoch = m_startTime.toSecsSinceEpoch();
}

int EPGModelItem::durationSec() const
{
    return m_durationSec;
}

void EPGModelItem::setDurationSec(int newDurationSec)
{
    m_durationSec = newDurationSec;
}

QString EPGModelItem::longDescription() const
{
    return m_longDescription;
}

void EPGModelItem::setLongDescription(const QString &newLongDescription)
{
    m_longDescription = newLongDescription;
}

QString EPGModelItem::shortDescription() const
{
    return m_shortDescription;
}

void EPGModelItem::setShortDescription(const QString &newShortDescription)
{
    m_shortDescription = newShortDescription;
}

bool EPGModelItem::isValid() const
{
    if ((m_shortId < 0) || (m_shortId >= 16777215))
    {
        return false;
    }
    if (m_mediumName.isEmpty())
    {
        return false;
    }
    if (!m_startTime.isValid())
    {
        return false;
    }
    if (m_durationSec == 0)
    {
        return false;
    }
    return true;
}

int EPGModelItem::shortId() const
{
    return m_shortId;
}

void EPGModelItem::setShortId(int newShortId)
{
    m_shortId = newShortId;
}

qint64 EPGModelItem::startTimeSecSinceEpoch() const
{
    return m_startTimeSecSinceEpoch;
}

qint64 EPGModelItem::endTimeSecSinceEpoch() const
{
    return m_startTimeSecSinceEpoch + m_durationSec;
}
