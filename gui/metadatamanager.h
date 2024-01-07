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
#include "servicelistid.h"
#include "epgmodel.h"

typedef QHash<QString, QString> serviceInfo_t;

class MetadataManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList epgDatesList READ epgDatesList NOTIFY epgDatesListChanged FINAL)

public:
    enum MetadataRole{
        SmallLogo = Qt::UserRole,
        SLSLogo,
        ShortName,
        MediumName,
        LongName,
    };

    explicit MetadataManager(QObject * parent = nullptr);
    ~MetadataManager();
    void processXML(const QString &xmldocument, uint16_t decoderId);
    void onFileReceived(const QByteArray & data, const QString & requestId);
    QVariant data(uint32_t sid, uint8_t SCIdS, MetadataManager::MetadataRole role) const;
    QVariant data(const ServiceListId & id, MetadataManager::MetadataRole role) const;

    EPGModel *epgModel(const ServiceListId & id) const;

    Q_INVOKABLE QDate epgDate(int idx) const;
    QStringList epgDatesList() const;

    void onValidEpgTime();
    void addServiceEpg(const ServiceListId & servId);
    void removeServiceEpg(const ServiceListId & servId);
    void clearEpg();

signals:
    void getFile(uint16_t decoderId, const QString & url, const QString & requestId);
    void dataUpdated(const ServiceListId & id, MetadataManager::MetadataRole role);
    void epgModelChanged(const ServiceListId & id);
    void epgDatesListChanged();

private:
    bool m_isLoadingFromCache;
    bool m_cleanEpgCache;
    QMap<QDate, QString> m_epgDates;
    QHash<QString, serviceInfo_t> m_info;
    QHash<ServiceListId, EPGModel *> m_epgList;

    void parseProgramme(const QDomElement &element, const ServiceListId &id);
    void parseDescription(const QDomElement &element, EPGModelItem *progItem);
    void parseLocation(const QDomElement &element, EPGModelItem *progItem);

    ServiceListId bearerToServiceId(const QString & bearerUri) const;

    void loadEpg(const ServiceListId & id);
    void addEpgDate(const QDate &date);
};

#endif // METADATAMANAGER_H
