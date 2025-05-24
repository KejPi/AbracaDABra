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

#include "txlocallist.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(localTx, "ScannerLocalTxDb", QtDebugMsg)

TxLocalList::TxLocalList(const QString &filename) : m_jsonFilename(filename)
{
    load();
}

TxLocalList::~TxLocalList()
{
    save();
}

void TxLocalList::load()
{
    if (m_jsonFilename.isEmpty())
    {
        return;
    }

    QFile loadFile(m_jsonFilename);
    if (!loadFile.exists())
    {  // file does not exist -> do nothing
        return;
    }

    // file exists here
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        return;
    }

    QByteArray data = loadFile.readAll();
    loadFile.close();

    if (data.isEmpty())
    {  // no data in the file
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray())
    {
        return;
    }

    m_data.clear();

    // doc is not null and is array
    QVariantList list = doc.toVariant().toList();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {  // loading the list
        auto map = it->toMap();
        bool ok = true;
        uint32_t freq = map.value("Frequency", 0).toUInt(&ok);
        if (!ok || freq == 0)
        {
            qCWarning(localTx) << "Failed to load frequency value";
            continue;
        }

        for (auto itemIt = map.cbegin(); itemIt != map.cend(); ++itemIt)
        {
            if (itemIt.key() != "Frequency")
            {  // if it is not frequency key => UEID
                uint32_t ueid = itemIt.key().toUInt(&ok, 16);
                if (!ok || ueid == 0)
                {
                    qCWarning(localTx) << "Failed to load UEID";
                    continue;
                }
                auto tiiList = itemIt.value().toList();
                if (tiiList.isEmpty())
                {
                    continue;
                }
                for (const auto &tii : std::as_const(tiiList))
                {
                    auto tiiMap = tii.toMap();
                    int m = tiiMap.value("main", -1).toInt(&ok);
                    if (!ok)
                    {
                        qCWarning(localTx) << "Failed to load TII main";
                        continue;
                    }
                    int s = tiiMap.value("sub", -1).toInt(&ok);
                    if (!ok)
                    {
                        qCWarning(localTx) << "Failed to load TII sub";
                        continue;
                    }
                    if (m >= 0 && s >= 0)
                    {
                        m_data[ServiceListId(freq, ueid)].append((s << 8) | m);
                    }
                }
            }
        }
    }
    m_dataChanged = false;
}

void TxLocalList::save() const
{
    if (m_dataChanged && !m_jsonFilename.isEmpty())
    {
        QVariantList list;
        uint32_t freq = 0;
        QVariantMap map;
        for (auto it = m_data.cbegin(); it != m_data.cend(); ++it)
        {
            QVariantList tiiList;
            for (const auto &item : it.value())
            {
                tiiList.append(QVariantMap{{"main", item & 0x00FF}, {"sub", (item >> 8) & 0x00FF}});
            }
            if (it.key().freq() != freq && map.empty() == false)
            {  // new frequency
                list.append(map);
                map.clear();
                freq = it.key().freq();
            }
            freq = it.key().freq();
            map["Frequency"] = freq;
            map[QString("%1").arg(it.key().ueid(), 6, 16, QChar('0'))] = tiiList;
        }

        // store last record
        if (map.empty() == false)
        {
            list.append(map);
        }

        QDir().mkpath(QFileInfo(m_jsonFilename).path());
        QFile saveFile(m_jsonFilename);
        if (!saveFile.open(QIODevice::WriteOnly))
        {
            qCWarning(localTx) << "Failed to save local transmitter database to file.";
            return;
        }
        saveFile.write(QJsonDocument::fromVariant(list).toJson());
        saveFile.close();
    }
}

void TxLocalList::set(ServiceListId ensId, int tii, bool markAsLocal)
{
    if (markAsLocal == true)
    {
        if (!get(ensId, tii))
        {
            m_data[ensId].append(tii);
            m_dataChanged = true;
        }
    }
    else
    {
        if (get(ensId, tii))
        {
            m_data[ensId].removeAll(tii);
            if (m_data.value(ensId).isEmpty())
            {
                m_data.remove(ensId);
            }
            m_dataChanged = true;
        }
    }
}

bool TxLocalList::get(ServiceListId ensId, int tii) const
{
    return m_data.contains(ensId) && m_data.value(ensId).contains(tii);
}
