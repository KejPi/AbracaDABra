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

#include <QString>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QGeoCoordinate>
#include <QRegularExpression>
#include <QStandardPaths>

#include "txdataloader.h"


void TxDataLoader::loadTable(QMultiHash<ServiceListId, TxDataItem *> &txList)
{
    enum TiiTable
    {
        ColCountry = 1,
        ColChannel = 3,
        ColLabel = 3,
        ColEID = 4,
        ColTII = 5,
        ColLocation = 6,
        ColLatitude = 7,
        ColLongitude = 8,
        ColAltitude = 9,
        ColPolarization = 11,
        ColFreq = 12,
        ColPower = 13,
        NumColumns = 14
    };

    const QHash<QString, int> eccMap
    {
        {"AT", 0xE0 },
        {"AU", 0xF0 },
        {"AZ", 0xE3 },
        {"BE", 0xE0 },
        {"CH", 0xE1 },
        {"CZ", 0xE2 },
        {"DE", 0xE0 },
        {"DK", 0xE1 },
        {"AZ", 0xE0 },
        {"ES", 0xE2 },
        {"FR", 0xE1 },
        {"GB", 0xE1 },
        {"GI", 0xE1 },
        {"GR", 0xE1 },
        {"HR", 0xE3 },
        {"ID", 0xF2 },
        {"IE", 0xE3 },
        {"IR", 0xF1 },
        {"IT", 0xE0 },
        {"IT", 0xE0 },
        {"KR", 0xF1 },
        {"KW", 0xF2 },
        {"LU", 0xE1 },
        {"MC", 0xE2 },
        {"ME", 0xE3 },
        {"MK", 0xE4 },
        {"MT", 0xE0 },
        {"MY", 0xF0 },
        {"NL", 0xE3 },
        {"NO", 0xE2 },
        {"OM", 0xF1 },
        {"PL", 0xE2 },
        {"QA", 0xF2 },
        {"RS", 0xE2 },
        {"SE", 0xE3 },
        {"SI", 0xE4 },
        {"SK", 0xE2 },
        {"TH", 0xF3 },
        {"TM", 0xE4 },
        {"TN", 0xE2 },
        {"TR", 0xE3 },
        {"UA", 0xE4 },
        {"VA", 0xE2 },
        {"ZA", 0xD0 },
    };

    const QString filename = "dab-tx-list.csv";
    static const QRegularExpression sepRegex("\\s*\\;\\s*");
    QDir directory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)  + "/TII/");
    if (directory.exists(filename)) {
        QFile file(directory.absolutePath() + "/" + filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDeleteAll(txList);
            QTextStream in(&file);

            // skip first line
            QString line = in.readLine();
            while (!in.atEnd())
            {
                // fill transmitter list
                line = in.readLine();
                QStringList columns = line.split(sepRegex);
                if ((columns.size() >= TiiTable::NumColumns) && !columns.at(TiiTable::ColTII).isEmpty())
                {
                    double lat = columns.at(TiiTable::ColLatitude).toDouble();
                    double lon = columns.at(TiiTable::ColLongitude).toDouble();
                    double alt = columns.at(TiiTable::ColAltitude).toDouble();
                    QGeoCoordinate coordinates = QGeoCoordinate(lat, lon, alt);

                    if (coordinates.isValid())
                    {
                        TxDataItem * item = new TxDataItem();
                        item->setCoordinates(coordinates);

                        uint32_t freq = static_cast<uint32_t>(columns.at(TiiTable::ColFreq).toFloat() * 1000);
                        uint8_t ecc = eccMap.value(columns.at(TiiTable::ColCountry), 0);
                        uint16_t eid = columns.at(TiiTable::ColEID).toUInt(nullptr, 16);
                        uint32_t ueid = (ecc << 16) | eid;
                        item->setEnsId(ServiceListId(freq, ueid));

                        item->setPower(columns.at(TiiTable::ColPower).toFloat());
                        // item->polarization = TxDataItem::Polarization::Unknown;
                        // if (!columns.at(TiiTable::ColPolarization).isEmpty())
                        // {
                        //     if (columns.at(TiiTable::ColPolarization).toUpper() == "H")
                        //     {
                        //         item->polarization = TxDataItem::Polarization::Horizontal;
                        //     }
                        //     else {
                        //         item->polarization = TxDataItem::Polarization::Vertical;
                        //     }
                        // }

                        uint16_t tii = columns.at(TiiTable::ColTII).toUInt();
                        item->setMainId(tii / 100);
                        item->setSubId(tii % 100);

                        item->setEnsLabel(columns.at(TiiTable::ColLabel));
                        item->setLocation(columns.at(TiiTable::ColLocation));

                        if (item->isValid())
                        {
                            txList.insert(item->ensId(), item);
                        }
                        else {
                            delete item;
                        }
                    }
                    else
                    {
                        // qDebug() << "Skipping line:" << line;
                    }
                }
                else
                {
                    // qDebug() << "Skipping line:" << line;
                }
            }
        }
        qDebug() << "TII items loaded:" << txList.size();
        file.close();
    }
}
