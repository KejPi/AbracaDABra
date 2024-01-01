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
}

EPGTime::~EPGTime()
{
    m_minuteTimer->stop();
    delete m_minuteTimer;
}

void EPGTime::setTime(const QDateTime & time)
{
    m_dabTime = time;
    setSecSinceEpoch(m_dabTime.toSecsSinceEpoch());
}

void EPGTime::onTimerTimeout()
{
    if (m_dabTime.isValid())
    {
        setTime(m_dabTime.addSecs(60));
    }
}


void EPGTime::onDabTime(const QDateTime &d)
{
    setTime(d);
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
