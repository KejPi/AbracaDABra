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

#include "epgtime.h"

EPGTime* EPGTime ::m_instancePtr = nullptr;

EPGTime *EPGTime::getInstance()
{
    if (m_instancePtr == nullptr)
    {
        m_instancePtr = new EPGTime();
        return m_instancePtr;
    }
    else
    {
        return m_instancePtr;
    }
}

EPGTime::EPGTime() : QObject(nullptr)
{
    m_minuteTimer = new QTimer();
    m_minuteTimer->setInterval(1000*60); // 1 minute
    m_isLiveBroadcasting = true;
    m_secSinceEpoch = 0;
}

EPGTime::~EPGTime()
{
    m_minuteTimer->stop();
    delete m_minuteTimer;
}

void EPGTime::setTime(const QDateTime & time)
{
    if (!isValid())
    {   // evaluate timezone
        if (m_isLiveBroadcasting)
        {   // time zone is taken from system
            m_ltoSec = QDateTime::currentDateTime().offsetFromUtc();
        }
        else
        {   // using timezone from DAB
            m_ltoSec = time.toLocalTime().offsetFromUtc();
        }
        qDebug() << "LTO [minutes]" << m_ltoSec/60;

        m_currentTime = time.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(m_ltoSec));
        setSecSinceEpoch(m_currentTime.toSecsSinceEpoch());
        setCurrentDateString(m_currentTime.date().toString("dd.MM.yyyy"));
        setCurrentTimeString(m_currentTime.time().toString("HH:mm"));

        emit haveValidTime();

        return;
    }
    m_currentTime = time.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(m_ltoSec));
    setSecSinceEpoch(m_currentTime.toSecsSinceEpoch());
    setCurrentDateString(m_currentTime.date().toString("dd.MM.yyyy"));
    setCurrentTimeString(m_currentTime.time().toString("HH:mm"));
}

void EPGTime::onTimerTimeout()
{
    if (m_currentTime.isValid())
    {
        setTime(m_currentTime.addSecs(60));
    }
}

QLocale EPGTime::timeLocale() const
{
    return m_timeLocale;
}

void EPGTime::setTimeLocale(const QLocale &newTimeLocale)
{
    m_timeLocale = newTimeLocale;
}

void EPGTime::setIsLiveBroadcasting(bool newIsLiveBroadcasting)
{
    if ((newIsLiveBroadcasting == false) || (newIsLiveBroadcasting != m_isLiveBroadcasting))
    {   // switching raw file
        // or switching between live and file
        m_isLiveBroadcasting = newIsLiveBroadcasting;
        // reset time value
        m_secSinceEpoch = 0;
    }
}


void EPGTime::onDabTime(const QDateTime &d)
{
    setTime(d);
    //setTime(QDateTime::currentDateTime());
    m_minuteTimer->start();
}

qint64 EPGTime::secSinceEpoch() const
{
    return m_secSinceEpoch;
}

void EPGTime::setSecSinceEpoch(qint64 newSecSinceEpoch)
{
    if (m_secSinceEpoch == newSecSinceEpoch)
        return;
    m_secSinceEpoch = newSecSinceEpoch;
    emit secSinceEpochChanged();
}

QString EPGTime::currentDateString() const
{
    return m_currentDateString;
}

void EPGTime::setCurrentDateString(const QString &newCurrentDateString)
{
    if (m_currentDateString == newCurrentDateString)
        return;
    m_currentDateString = newCurrentDateString;
    emit currentDateStringChanged();
}

QString EPGTime::currentTimeString() const
{
    return m_currentTimeString;
}

void EPGTime::setCurrentTimeString(const QString &newCurrentTimeString)
{
    if (m_currentTimeString == newCurrentTimeString)
        return;
    m_currentTimeString = newCurrentTimeString;
    emit currentTimeStringChanged();
}
