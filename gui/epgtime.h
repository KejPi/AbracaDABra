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

#ifndef EPGTIME_H
#define EPGTIME_H

#include <QObject>
#include <QDateTime>
#include <QTimer>

// singleton class
class EPGTime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 secSinceEpoch READ secSinceEpoch WRITE setSecSinceEpoch NOTIFY secSinceEpochChanged FINAL)
    Q_PROPERTY(QString currentDateString READ currentDateString WRITE setCurrentDateString NOTIFY currentDateStringChanged FINAL)
public:
    EPGTime(const EPGTime& obj) = delete;   // deleting copy constructor
    ~EPGTime();
    static EPGTime *getInstance();
    QDateTime currentTime() const { return m_dabTime; }
    Q_INVOKABLE QDate currentDate() const { return m_dabTime.date(); }
    Q_INVOKABLE bool isCurrentDate(const QDate & date) const { return date == currentDate(); }

    qint64 secSinceEpoch() const;
    void setSecSinceEpoch(qint64 newSecSinceEpoch);

    Q_INVOKABLE int secSinceMidnight() const { return m_dabTime.date().startOfDay().secsTo(m_dabTime); }

    void onDabTime(const QDateTime & d);
    void setIsLiveBroadcasting(bool newIsLiveBroadcasting);

    QString currentDateString() const;
    void setCurrentDateString(const QString &newCurrentDateString);

    QLocale timeLocale() const;
    void setTimeLocale(const QLocale &newTimeLocale);

signals:
    void secSinceEpochChanged();

    void currentDateStringChanged();

private:
    EPGTime();
    void setTime(const QDateTime &time);
    void onTimerTimeout();

    static EPGTime * m_instancePtr;
    QDateTime m_dabTime;
    QTimer * m_minuteTimer;
    qint64 m_secSinceEpoch;
    bool m_isLiveBroadcasting;
    QString m_currentDateString;
    QLocale m_timeLocale;
};

#endif // EPGTIME_H
