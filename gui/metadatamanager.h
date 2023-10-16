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

#ifndef METADATAMANAGER_H
#define METADATAMANAGER_H

#include <QObject>
#include <QDomDocument>
#include <QPixmap>
#include <QHash>

typedef QHash<QString, QString> serviceInfo_t;

class MetadataManager : public QObject
{
    Q_OBJECT

public:
    enum MetadataRole{
        SmallLogo = Qt::UserRole,
        SLSLogo,
        ShortName,
        MediumName,
        LongName,
    };

    explicit MetadataManager(QObject *parent = nullptr);
    void processXML(const QString &xmldocument, const QString & scopeId);
    void onFileReceived(const QByteArray & data, const QString & requestId);
    QVariant data(uint32_t sid, uint8_t SCIdS, MetadataManager::MetadataRole role);

signals:
    void getFile(const QString & url, const QString & requestId);
    void dataUpdated(uint32_t sid, uint8_t SCIdS, MetadataManager::MetadataRole role);

private:
//    static MetadataManager * m_instancePtr;    // static pointer which will points to the instance of this class
    QHash<QString, serviceInfo_t> m_info;

};

#endif // METADATAMANAGER_H
