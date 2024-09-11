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

#ifndef TXDATAITEM_H
#define TXDATAITEM_H

#include <QString>
#include "servicelistid.h"
#include <QGeoCoordinate>

class TxDataItem {
public:
    TxDataItem();

    bool isValid() const { return m_ensId.isValid(); }

    QString ensLabel() const;
    void setEnsLabel(const QString &newEnsLabel);

    QString location() const;
    void setLocation(const QString &newLocation);

    ServiceListId ensId() const;
    void setEnsId(const ServiceListId &newEnsId);

    uint8_t mainId() const;
    void setMainId(uint8_t newMainId);

    uint8_t subId() const;
    void setSubId(uint8_t newSubId);

    QGeoCoordinate coordinates() const;
    void setCoordinates(const QGeoCoordinate &newCoordinates);

    float power() const;
    void setPower(float newPower);

private:
    // enum Polarization { Unknown = -1, Vertical, Horizontal };
    QString m_ensLabel;
    QString m_location;
    ServiceListId m_ensId;
    uint8_t m_mainId;
    uint8_t m_subId;
    QGeoCoordinate m_coordinates;
    float m_power;
    // int m_antenna;
    // Polarization m_polarization;
};


#endif // TXDATAITEM_H
