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

#ifndef AUDIORECSCHEDULEITEM_H
#define AUDIORECSCHEDULEITEM_H

#include "servicelistid.h"
#include <QString>
#include <QDateTime>

class AudioRecScheduleItem
{
public:
    AudioRecScheduleItem();
    QString name() const;
    void setName(const QString &newName);

    QDateTime startTime() const;
    void setStartTime(const QDateTime &newStartTime);
    QDateTime endTime() const { return m_startTime.addSecs(m_durationSec); }
    void setEndTime(const QDateTime &newEndTime);

    int durationSec() const;
    void setDurationSec(int newDurationSec);
    QTime duration() const;
    void setDuration(const QTime & time);

    ServiceListId serviceId() const;
    void setServiceId(const ServiceListId &newServiceId);
    bool hasConflict() const;
    void setHasConflict(bool newHasConflict);

    bool isRecorded() const;
    void setIsRecorded(bool newIsRecorded);

private:
    QString m_name;
    QDateTime m_startTime;
    int m_durationSec;
    ServiceListId m_serviceId;
    bool m_hasConflict = false;
    bool m_isRecorded = false;
};

bool operator<(const AudioRecScheduleItem & a, const AudioRecScheduleItem & b);

#endif // AUDIORECSCHEDULEITEM_H
