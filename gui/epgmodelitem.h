/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef EPGMODELITEM_H
#define EPGMODELITEM_H

#include <QString>
#include <QDateTime>

class EPGModelItem
{
public:
    EPGModelItem();
    //~EPGModelItem() { }

    QString longName() const;
    void setLongName(const QString &newLongName);

    QString mediumName() const;
    void setMediumName(const QString &newMediumName);

    QString shortName() const;
    void setShortName(const QString &newShortName);

    QDateTime startTime() const;
    void setStartTime(const QDateTime &newStartTime);

    int durationSec() const;
    void setDurationSec(int newDurationSec);

    QString longDescription() const;
    void setLongDescription(const QString &newLongDescription);

    QString shortDescription() const;
    void setShortDescription(const QString &newShortDescription);

    bool isValid() const;
    int startTimeSec() const;
    int endTimeSec() const;
    qint64 startTimeSecSinceEpoch() const;
    qint64 endTimeSecSinceEpoch() const;

    int shortId() const;
    void setShortId(int newShortId);


signals:

private:
    QString m_longName;
    QString m_mediumName;
    QString m_shortName;
    QDateTime m_startTime;
    qint64 m_startTimeSecSinceEpoch;
    int m_durationSec;
    QString m_longDescription;
    QString m_shortDescription;
    int m_shortId;
};

#endif // EPGMODELITEM_H
