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

#include "audiorecscheduleitem.h"

AudioRecScheduleItem::AudioRecScheduleItem()
{}

QString AudioRecScheduleItem::name() const
{
    return m_name;
}

void AudioRecScheduleItem::setName(const QString &newName)
{
    m_name = newName;
}

QDateTime AudioRecScheduleItem::startTime() const
{
    return m_startTime;
}

void AudioRecScheduleItem::setStartTime(const QDateTime &newStartTime)
{
    int offset = newStartTime.offsetFromUtc();
    m_startTime = newStartTime.toOffsetFromUtc(offset);
}

void AudioRecScheduleItem::setEndTime(const QDateTime &newEndTime)
{
    int duration = m_startTime.secsTo(newEndTime);
    if (duration < 0)
    {
        duration = 0;
    }
    m_durationSec = duration;
}

int AudioRecScheduleItem::durationSec() const
{
    return m_durationSec;
}

void AudioRecScheduleItem::setDurationSec(int newDurationSec)
{
    m_durationSec = newDurationSec;
}

QTime AudioRecScheduleItem::duration() const
{
    return QTime(0, 0, 0).addSecs(m_durationSec);
}

void AudioRecScheduleItem::setDuration(const QTime &time)
{
    m_durationSec = time.msecsSinceStartOfDay() / 1000;
}

ServiceListId AudioRecScheduleItem::serviceId() const
{
    return m_serviceId;
}

void AudioRecScheduleItem::setServiceId(const ServiceListId &newServiceId)
{
    m_serviceId = newServiceId;
}

bool AudioRecScheduleItem::hasConflict() const
{
    return m_hasConflict;
}

void AudioRecScheduleItem::setHasConflict(bool newHasConflict)
{
    m_hasConflict = newHasConflict;
}

bool AudioRecScheduleItem::isRecorded() const
{
    return m_isRecorded;
}

void AudioRecScheduleItem::setIsRecorded(bool newIsRecorded)
{
    m_isRecorded = newIsRecorded;
}

bool operator<(const AudioRecScheduleItem &a, const AudioRecScheduleItem &b)
{
    if (a.isRecorded())
    {
        return true;
    }
    if (b.isRecorded())
    {
        return false;
    }
    if (a.startTime() < b.startTime())
    {
        return true;
    }
    if (a.startTime() > b.startTime())
    {
        return false;
    }

    // start time is equal - shorter first
    return a.durationSec() < b.durationSec();
}
