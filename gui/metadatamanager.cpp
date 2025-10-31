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

#include "metadatamanager.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include "epgtime.h"
#include "spiapp.h"

Q_LOGGING_CATEGORY(metadataManager, "MetadataManager", QtInfoMsg)

MetadataManager::MetadataManager(const ServiceList *serviceList, QObject *parent)
    : QObject(parent), m_serviceList(serviceList), m_isLoadingFromCache(false), m_cleanEpgCache(true)
{}

MetadataManager::~MetadataManager()
{
    if (m_cleanEpgCache && EPGTime::getInstance()->isValid())
    {  // do chache maintenance
        QDir directory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/EPG/");
        QStringList xmlFiles = directory.entryList({"*_PI.xml"}, QDir::Files);
        QString currentDateStr2 = EPGTime::getInstance()->currentDate().addDays(-2).toString("yyyyMMdd");
        for (const QString &filename : xmlFiles)
        {
            if (filename.first(8) < currentDateStr2)
            {  // scope is out of range
                qCDebug(metadataManager) << "File" << filename << "is old ===> deleting";
                directory.remove(filename);
            }
        }
    }
}

void MetadataManager::processXML(const QString &xml, const QString &scopeId, uint16_t decoderId)
{
    QDomDocument xmldocument;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
    if (!xmldocument.setContent(xml, QDomDocument::ParseOption::UseNamespaceProcessing))
#else
    if (!xmldocument.setContent(xml, true))
#endif
    {
        qCWarning(metadataManager) << "Failed to parse SPI document for:" << scopeId;
        qCDebug(metadataManager) << xml;
        return;
    }

    qCDebug(metadataManager) << qPrintable(xmldocument.toString());

    QDomElement docElem = xmldocument.documentElement();
    if ("serviceInformation" == docElem.tagName())
    {
        QDomNode node = docElem.firstChild();
        while (!node.isNull())
        {  // ETSI TS 102 818 V3.4.1 [6.2]
            // serviceInformation: can contain zero or one of each of the following elements in this order:
            //      services
            //      serviceGroups
            QDomElement element = node.toElement();  // try to convert the node to an element
            if (!element.isNull())
            {
                if (("services" == element.tagName()) || ("ensemble" == element.tagName()))
                {
                    QDomNode servicesNode = element.firstChild();
                    while (!servicesNode.isNull())
                    {  // ETSI TS 102 818 V3.4.1 [6.3]
                        // Services element can contain zero or one "serviceProvider" element and zero or more "service" elements
                        QDomElement servicesElement = servicesNode.toElement();  // try to convert the node to an element
                        if (!servicesElement.isNull())
                        {
                            if ("service" == servicesElement.tagName())
                            {  // ETSI TS 102 818 V3.4.1 [6.5]
                                // Service element describes metadata and available bearers for a service.
                                QStringList sidList;
                                serviceInfo_t serviceInfo;
                                QDomNode serviceNode = servicesElement.firstChild();
                                while (!serviceNode.isNull())
                                {  // ETSI TS 102 818 V3.4.1 [6.5]
                                    // Service element can contain the following elements, which should be provided in the defined order:
                                    // * nameGroup (shortName, mediumName, longName);
                                    // * alias;
                                    // * phoneme;
                                    // * mediaDescription;
                                    // * genre;
                                    // * keywords;
                                    // * link;
                                    // * bearer;
                                    // * radiodns;
                                    // * geolocation;
                                    // * serviceGroupMember

                                    // first analyze bearer(s)
                                    QDomNodeList bearerList = servicesElement.elementsByTagName("bearer");
                                    for (int b = 0; b < bearerList.count(); ++b)
                                    {
                                        QDomElement bearerElement = bearerList.at(b).toElement();  // try to convert the node to an element
                                        if (!bearerElement.isNull())
                                        {
                                            QString bearer = bearerElement.attribute("id", "");
                                            static const QRegularExpression sidRegex("dab:[a-f0-9]([a-f0-9]{2}).\\w{4}.(\\w{4}.\\d+)",
                                                                                     QRegularExpression::CaseInsensitiveOption);
                                            QRegularExpressionMatch match = sidRegex.match(bearer);
                                            if (match.hasMatch())
                                            {  // found valid DAB SId ==> service is available in DAB
                                                QString sidStr = match.captured(1) + match.captured(2);
                                                if (!sidList.contains(sidStr))
                                                {  // append if not in list
                                                    sidList.append(sidStr);
                                                }
                                            }
                                        }
                                    }

                                    if (!sidList.isEmpty())
                                    {  // at least one valid DAB SId was found => go through elements
                                        QDomElement serviceElement = serviceNode.toElement();  // try to convert the node to an element
                                        if (!serviceElement.isNull())
                                        {  // ETSI TS 102 818 V3.4.1 [6.5]
                                            // at lease one of these shall be provided
                                            if (("shortName" == serviceElement.tagName()) || ("mediumName" == serviceElement.tagName()) ||
                                                ("longName" == serviceElement.tagName()))
                                            {
                                                serviceInfo[serviceElement.tagName()] = serviceElement.text();
                                            }
                                            else if ("mediaDescription" == serviceElement.tagName())
                                            {
                                                QDomNode mediaDescNode = serviceElement.firstChild();
                                                while (!mediaDescNode.isNull())
                                                {
                                                    QDomElement mediaDescElement =
                                                        mediaDescNode.toElement();  // try to convert the node to an element
                                                    if (!mediaDescElement.isNull())
                                                    {
                                                        if (("shortDescription" == mediaDescElement.tagName()) ||
                                                            ("longDescription" == mediaDescElement.tagName()))
                                                        {  // service description long and/or short
                                                            serviceInfo[mediaDescElement.tagName()] = mediaDescElement.text();
                                                        }
                                                        else if ("multimedia" == mediaDescElement.tagName())
                                                        {  // Service logos shall be provided using the mediaDescription/multimedia element
                                                            QString type = mediaDescElement.attribute("type");
                                                            QString url = mediaDescElement.attribute("url");
                                                            QString width = mediaDescElement.attribute("width");
                                                            QString height = mediaDescElement.attribute("height");
                                                            QString ext = "png";
                                                            if ("logo_colour_square" == type)
                                                            {
                                                                width = height = "32";
                                                            }
                                                            else if ("logo_colour_rectangle" == type)
                                                            {
                                                                width = "112";
                                                                height = "32";
                                                            }
                                                            else
                                                            {
                                                                if (mediaDescElement.attribute("mimeValue") == "image/jpeg")
                                                                {
                                                                    ext = "jpg";
                                                                }
                                                            }
                                                            int widthVal = width.toInt();
                                                            int heightVal = height.toInt();

                                                            // only SLS size or logo size is accepted
                                                            if (((widthVal == 320) && (heightVal == 240)) || ((widthVal == 32) && (heightVal == 32)))
                                                            {
                                                                for (const QString &sidStr : sidList)
                                                                {  // ask for logo for each SId (actually there should be only one valid DAB SId)
                                                                    QString filename = QString("%1/%2x%3.%4").arg(sidStr, width, height, ext);
                                                                    qCDebug(metadataManager) << url << "===>" << filename;

                                                                    emit getFile(decoderId, url, filename);
                                                                }
                                                            }
                                                        }
                                                    }
                                                    mediaDescNode = mediaDescNode.nextSibling();
                                                }
                                            }
                                        }
                                    }
                                    serviceNode = serviceNode.nextSibling();
                                }
                                for (const QString &sidStr : sidList)
                                {  // insert in database
                                    qCDebug(metadataManager) << sidStr << serviceInfo;
                                    m_info.insert(sidStr, serviceInfo);
                                }
                            }
                            else if ("serviceProvider" == servicesElement.tagName())
                            {  // not service/ensemble or serviceGroups
                                // this should not happen for XML from internet but it may be coming from binary encondig that does not support
                                // serviceProvider element
                                qCDebug(metadataManager, "serviceProvider found");
                            }
                            else
                            {  // not service/ensemble or serviceGroups
                                // this may happen for XML coming from binary encondig that does not support serviceProvider element
                                // in this case services element has id attribute that is UEID
                                // ETSI TS 102 371 V3.2.1 [4.17.1 DAB ensemble element encoding]
                                if (element.hasAttribute("id"))
                                {
                                    qCDebug(metadataManager) << "Ensemble" << element.attribute("id") << servicesElement.tagName();

                                    QString eidStr = element.attribute("id");
                                    bool isEidValid = (eidStr.length() == 6);
                                    if (isEidValid)
                                    {
                                        eidStr.toInt(&isEidValid, 16);
                                    }
                                    if (isEidValid && "mediaDescription" == servicesElement.tagName())
                                    {
                                        QDomNode mediaDescNode = servicesElement.firstChild();
                                        while (!mediaDescNode.isNull())
                                        {
                                            QDomElement mediaDescElement = mediaDescNode.toElement();  // try to convert the node to an element
                                            if (!mediaDescElement.isNull())
                                            {
                                                if ("multimedia" == mediaDescElement.tagName())
                                                {  // Ensemble logos shall be provided using the mediaDescription/multimedia element
                                                    QString type = mediaDescElement.attribute("type");
                                                    QString url = mediaDescElement.attribute("url");
                                                    QString width = mediaDescElement.attribute("width");
                                                    QString height = mediaDescElement.attribute("height");
                                                    QString ext = "png";
                                                    if ("logo_colour_square" == type)
                                                    {
                                                        width = height = "32";
                                                    }
                                                    else if ("logo_colour_rectangle" == type)
                                                    {
                                                        width = "112";
                                                        height = "32";
                                                    }
                                                    else
                                                    {
                                                        if (mediaDescElement.attribute("mimeValue") == "image/jpeg")
                                                        {
                                                            ext = "jpg";
                                                        }
                                                    }
                                                    int widthVal = width.toInt();
                                                    int heightVal = height.toInt();

                                                    // only SLS size or logo size is accepted
                                                    if (((widthVal == 320) && (heightVal == 240)) || ((widthVal == 32) && (heightVal == 32)))
                                                    {
                                                        // ask for logo
                                                        QString filename = QString("%1/%2x%3.%4").arg(eidStr, width, height, ext);
                                                        qCDebug(metadataManager) << url << "===>" << filename;

                                                        emit getFile(decoderId, url, filename);
                                                    }
                                                }
                                            }
                                            mediaDescNode = mediaDescNode.nextSibling();
                                        }
                                    }
                                }
                            }
                        }
                        servicesNode = servicesNode.nextSibling();
                    }
                }
                else if ("serviceGroups" == element.tagName())
                {
                    qCDebug(metadataManager, "serviceGroups found");
                }
                else
                {  // not service/ensemble or serviceGroups - this should not happen
                    qCDebug(metadataManager) << element.tagName();
                }
            }
            node = node.nextSibling();
        }
    }
    else if ("epg" == docElem.tagName())
    {
        // qCInfo(metadataManager) << "======================= EPG =========================>";
        // qCInfo(metadataManager) << qPrintable(xmldocument.toString());
        // qCInfo(metadataManager) << "<=====================================================";
        QDomNode node = docElem.firstChild();
        while (!node.isNull())
        {
            QDomElement element = node.toElement();  // try to convert the node to an element.
            if (!element.isNull() && ("schedule" == element.tagName()))
            {
                QString scStart;
                QString scId = scopeId;
                if (!element.firstChildElement("scope").isNull())
                {  // found scope element
                    QDomElement scope = element.firstChildElement("scope");
                    scStart = scope.attribute("startTime");
                    QDomElement serviceScope = scope.firstChildElement("serviceScope");
                    while (!serviceScope.isNull())
                    {
                        ServiceListId id = bearerToServiceId(serviceScope.attribute("id"));
                        if (id.isValid())
                        {  // found valid DAB service scope
                            scId = serviceScope.attribute("id");
                            break;
                        }
                        serviceScope = serviceScope.nextSiblingElement("serviceScope");
                    }
                }

                qCDebug(metadataManager) << "Service scope ID:" << scId;
                if (!scId.isEmpty() && !scStart.isEmpty())
                {
                    ServiceListId id = bearerToServiceId(scId);
                    if (id.isValid())
                    {
                        bool valid = false;
                        QDomElement child = element.firstChildElement("programme");
                        while (!child.isNull())
                        {
                            valid = parseProgramme(child, id) || valid;
                            child = child.nextSiblingElement("programme");
                        }

                        if (!m_isLoadingFromCache && valid)
                        {  // save parsed file to the cache
                            // "20140805_e1c221.0_PI.xml"
                            QString filename = QString("%1/EPG/%2_%3.%4_PI.xml")
                                                   .arg(QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
                                                        QDateTime::fromString(scStart, Qt::ISODate)
                                                            .toUTC()
                                                            .toOffsetFromUtc(EPGTime::getInstance()->ltoSec())
                                                            .toString("yyyyMMdd"))
                                                   .arg(id.sid(), 6, 16, QChar('0'))
                                                   .arg(id.scids());

                            QDir dir;
                            dir.mkpath(QFileInfo(filename).absolutePath());
                            QFile file(filename);
                            if (!file.exists())
                            {
                                file.open(QIODevice::WriteOnly);
                                QTextStream output(&file);
                                output << xml;
                                file.close();
                            }
                            else
                            {
                                // qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!! File EXISTS";
                            }
                        }
                    }
                }
            }
            node = node.nextSibling();
        }
    }
    else
    {
        qCInfo(metadataManager) << "=====================================================>";
        qCInfo(metadataManager) << qPrintable(xmldocument.toString());
        qCInfo(metadataManager) << "<=====================================================";
    }
}

bool MetadataManager::parseProgramme(const QDomElement &element, const ServiceListId &id)
{
    bool ret = false;

    // ETSI TS 102 818 V3.3.1 (2020-08) [7.8]
    // The location element may appear zero or more times within a programme or programmeEvent element.
    QList<EPGModelItem *> itemList;
    int ltoSec = EPGTime::getInstance()->ltoSec();
    QDomElement locationElement = element.firstChildElement("location");
    while (!locationElement.isNull())
    {
        QDomElement timeElement = locationElement.firstChildElement("time");
        while (!timeElement.isNull())
        {
            EPGModelItem *progItem = new EPGModelItem;

            QDateTime startTime = QDateTime::fromString(timeElement.attribute("time"), Qt::ISODate);
            progItem->setStartTime(startTime.toUTC().toOffsetFromUtc(ltoSec));

            // ETSI TS 102 818 V3.3.1 (2020-08) [5.2.5 duration type]
            // Duration is based on the ISO 8601 [2] format: PTnHnMnS, where "T" represents the date/time separator,
            // "nH" the number of hours, "nM" the number of minutes and "nS" the number of seconds.
            static const QRegularExpression durationRe("^PT((\\d+)H)?((\\d+)M)?((\\d+)S)?");
            QRegularExpressionMatch match = durationRe.match(timeElement.attribute("duration"));
            int duration = 0;
            if (match.hasMatch())
            {
                // hours
                duration += 3600 * (match.captured(2).isEmpty() ? 0 : match.captured(2).toInt());
                // minutes
                duration += 60 * (match.captured(4).isEmpty() ? 0 : match.captured(4).toInt());
                // seconds
                duration += (match.captured(6).isEmpty() ? 0 : match.captured(6).toInt());

                progItem->setDurationSec(duration);
                progItem->setShortId(element.attribute("shortId").toInt());

                itemList.append(progItem);
            }
            else
            {  // duration not valid
                delete progItem;
            }

            timeElement = timeElement.nextSiblingElement("time");
        }
        locationElement = locationElement.nextSiblingElement("location");
    }

    QDomElement child = element.firstChildElement();
    while (!child.isNull())
    {
        if ("longName" == child.tagName())
        {
            for (auto &progItem : itemList)
            {
                progItem->setLongName(child.text());
            }
        }
        else if ("mediumName" == child.tagName())
        {
            for (auto &progItem : itemList)
            {
                progItem->setMediumName(child.text());
            }
        }
        else if ("shortName" == child.tagName())
        {
            for (auto &progItem : itemList)
            {
                progItem->setShortName(child.text());
            }
        }
        else if ("mediaDescription" == child.tagName())
        {
            for (auto &progItem : itemList)
            {
                parseDescription(child, progItem);
            }
        }

        child = child.nextSiblingElement();
    }

    if (!itemList.isEmpty())
    {
        if (m_epgList.value(id, nullptr) == nullptr)
        {
            EPGModel *model = new EPGModel(this);
            model->setServiceId(id);
            m_epgList[id] = model;
            emit epgModelChanged(id);
            emit epgAvailable();
        }

        for (auto &progItem : itemList)
        {
            if (progItem->startTime().date() < EPGTime::getInstance()->currentDate().addDays(7))
            {
                addEpgDate(progItem->startTime().date());
                ret = m_epgList[id]->addItem(progItem) || ret;
            }
            else
            {
                delete progItem;
            }
        }
    }

    return ret;
}

void MetadataManager::parseDescription(const QDomElement &element, EPGModelItem *progItem)
{
    QDomElement child = element.firstChildElement();
    while (!child.isNull())
    {
        if ("shortDescription" == child.tagName())
        {
            progItem->setShortDescription(child.text().replace(QChar('\\'), QChar()).trimmed());
        }
        else if ("longDescription" == child.tagName())
        {
            progItem->setLongDescription(child.text().replace(QChar('\\'), QChar()).trimmed());
        }

        child = child.nextSiblingElement();
    }
}

ServiceListId MetadataManager::bearerToServiceId(const QString &bearerUri) const
{  // ETSI TS 103 270 V1.4.1 (2022-05) [5.1.2.4 Construction of bearerURI]
    // The bearerURI for a DAB/DAB+ service is compiled as follows:
    // dab:<gcc>.<eid>.<sid>.<scids>[.<uatype>]
    // example: dab:de0.10bc.d210.0
    static const QRegularExpression re("dab:[0-9a-f]([0-9a-f]{2})\\.[0-9a-f]{4}\\.([0-9a-f]{4})\\.(\\d).*",
                                       QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = re.match(bearerUri);
    if (match.hasMatch())
    {
        uint32_t sid = QString(match.captured(1) + match.captured(2)).toUInt(nullptr, 16);
        uint8_t scids = match.captured(3).toInt();
        return ServiceListId(sid, scids);
    }
    return ServiceListId();
}

void MetadataManager::onFileReceived(const QByteArray &data, const QString &requestId)
{
    if (data.size() == 0)
    {  // empty data, do nothing
        return;
    }

    QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + requestId;
    qCDebug(metadataManager) << requestId << filename;

    static const QRegularExpression re("([0-9a-f]{6})(\\.(\\d+))?/(\\d+x\\d+)\\..*", QRegularExpression::CaseInsensitiveOption);

    QDir dir;
    dir.mkpath(QFileInfo(filename).absolutePath());

    QFile file(filename);
    if (file.exists())
    {
        QCryptographicHash md5gen(QCryptographicHash::Md5);
        QFile file(filename);
        file.open(QIODevice::ReadOnly);
        md5gen.addData(file.readAll());
        file.close();

        QByteArray md5FileInCache = md5gen.result();
        md5gen.reset();
        md5gen.addData(data);
        if (md5gen.result() != md5FileInCache)
        {  // different file => overwrite
            file.open(QIODevice::WriteOnly);
            file.write(data);
            file.close();

            QRegularExpressionMatch match = re.match(requestId);
            if (match.hasMatch())
            {
                if (match.captured(2).isEmpty())
                {  // ensemble
                    uint32_t ueid = match.captured(1).toUInt(nullptr, 16);
                    QString size = match.captured(4);
                    MetadataRole role = MetadataRole::SLSLogo;
                    if (size == "32x32")
                    {
                        role = MetadataRole::SmallLogo;
                    }
                    emit dataUpdated(ServiceListId(174928, ueid), role);  // using some frequency (5A)
                }
                else
                {
                    uint32_t sid = match.captured(1).toUInt(nullptr, 16);
                    uint8_t scids = match.captured(3).toUInt();
                    QString size = match.captured(4);
                    MetadataRole role = MetadataRole::SLSLogo;
                    if (size == "32x32")
                    {
                        role = MetadataRole::SmallLogo;
                    }
                    emit dataUpdated(ServiceListId(sid, scids), role);
                }
            }
        }
        else
        { /* do nothing, file is the same */
            qCDebug(metadataManager) << filename << "is the same";
        }
    }
    else
    {
        file.open(QIODevice::WriteOnly);
        file.write(data);
        file.close();

        QRegularExpressionMatch match = re.match(requestId);
        if (match.hasMatch())
        {
            if (match.captured(2).isEmpty())
            {  // ensemble
                uint32_t ueid = match.captured(1).toUInt(nullptr, 16);
                QString size = match.captured(4);
                MetadataRole role = MetadataRole::SLSLogo;
                if (size == "32x32")
                {
                    role = MetadataRole::SmallLogo;
                }
                emit dataUpdated(ServiceListId(174928, ueid), role);  // using some frequency (5A)
            }
            else
            {
                uint32_t sid = match.captured(1).toUInt(nullptr, 16);
                uint8_t scids = match.captured(3).toUInt();
                QString size = match.captured(4);
                MetadataRole role = MetadataRole::SLSLogo;
                if (size == "32x32")
                {
                    role = MetadataRole::SmallLogo;
                }
                emit dataUpdated(ServiceListId(sid, scids), role);
            }
        }
    }
}

QVariant MetadataManager::data(uint32_t sid, uint8_t SCIdS, MetadataRole role) const
{
    return data(ServiceListId(sid, SCIdS), role);
}

QVariant MetadataManager::data(const ServiceListId &id, MetadataRole role) const
{
    QString idStr =
        id.isService() ? QString("%1.%2").arg(id.sid(), 6, 16, QChar('0')).arg(id.scids()) : QString("%1").arg(id.ueid(), 6, 16, QChar('0'));
    switch (role)
    {
        case SLSLogo:
        {
            QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/320x240.").arg(idStr);
            if (QFileInfo::exists(filename + "png"))
            {
                QPixmap pixmap;
                pixmap.load(filename + "png");
                if (pixmap.width() <= 320 && pixmap.height() <= 240)
                {
                    return QVariant(pixmap);
                }
                return QVariant();
            }
            else if (QFileInfo::exists(filename + "jpg"))
            {
                QPixmap pixmap;
                pixmap.load(filename + "jpg");
                if (pixmap.width() <= 320 && pixmap.height() <= 240)
                {
                    return QVariant(pixmap);
                }
                return QVariant();
            }
        }
        break;
        case SmallLogo:
        {
            QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/32x32.").arg(idStr);
            if (QFileInfo::exists(filename + "png"))
            {
                QPixmap pixmap;
                pixmap.load(filename + "png");
                if (pixmap.width() <= 32 && pixmap.height() <= 32)
                {
                    return QVariant(pixmap);
                }
                return QVariant();
            }
            else if (QFileInfo::exists(filename + "jpg"))
            {
                QPixmap pixmap;
                pixmap.load(filename + "jpg");
                if (pixmap.width() <= 32 && pixmap.height() <= 32)
                {
                    return QVariant(pixmap);
                }
                return QVariant();
            }
        }
        break;
        case CountryFlag:
        {
            QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/flags/" +
                               QString("%1.png").arg(DabTables::getCountryCodeISO3166(id.sid()));
            if (QFileInfo::exists(filename))
            {
                QPixmap pixmap;
                pixmap.load(filename);
                return QVariant(pixmap);
            }
            else
            {  // download flag
                QString countryCode = DabTables::getCountryCodeISO3166(id.sid());
                if (!countryCode.isEmpty())
                {
                    QNetworkAccessManager *manager = new QNetworkAccessManager();
                    connect(manager, &QNetworkAccessManager::finished, this,
                            [=](QNetworkReply *reply)
                            {
                                if (reply->error() == QNetworkReply::NoError)
                                {
                                    QDir dir;

                                    dir.mkpath(QFileInfo(filename).absolutePath());
                                    QFile file(filename);
                                    if (file.open(QIODevice::WriteOnly))
                                    {
                                        file.write(reply->readAll());
                                        file.close();
                                        emit dataUpdated(id, MetadataManager::CountryFlag);
                                    }
                                }
                                reply->deleteLater();
                            });
                    connect(manager, &QNetworkAccessManager::finished, manager, &QNetworkAccessManager::deleteLater);
                    QNetworkRequest *request = new QNetworkRequest();
                    request->setUrl(QUrl("https://flagcdn.com/40x30/" + QString("%1.png").arg(countryCode)));
                    manager->get(*request);
                }
            }
        }
        break;
        default:
            break;
    }

    return QVariant();
}

EPGModel *MetadataManager::epgModel(const ServiceListId &id) const
{
    return m_epgList.value(id, nullptr);
}

QStringList MetadataManager::epgDatesList() const
{
    return m_epgDates.values();
}

void MetadataManager::loadEpg(const ServiceListId &servId, const QList<uint32_t> &ueidList)
{
    if (EPGTime::getInstance()->isValid() && servId.isValid())
    {
        m_isLoadingFromCache = true;

        QDir directory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/EPG/");
        QDate currentDate = EPGTime::getInstance()->currentDate();
        for (int day = -2; day < +7; ++day)
        {
            QDate date = currentDate.addDays(day);
            QString xmlFileName = QString("%1_%2.%3_PI.xml").arg(date.toString("yyyyMMdd")).arg(servId.sid(), 6, 16, QChar('0')).arg(servId.scids());
            if (directory.exists(xmlFileName))
            {
                QFile xmlfile(directory.absolutePath() + "/" + xmlFileName);
                qCDebug(metadataManager) << "Loading:" << xmlfile.fileName();
                if (xmlfile.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    DabSId dabSid(servId.sid());
                    QString scopeId = QString("dab:%1.%2.%3.0")
                                          .arg(dabSid.gcc(), 3, 16, QChar('0'))
                                          .arg(uint16_t(ueidList.at(0)), 4, 16, QChar('0'))
                                          .arg(dabSid.progSId(), 4, 16, QChar('0'));
                    QTextStream in(&xmlfile);
                    processXML(in.readAll(), scopeId, SPI_APP_INVALID_DECODER_ID);
                    xmlfile.close();
                }
            }
            else
            {
                if (currentDate >= QDateTime::currentDateTime().date().addDays(-5))
                {  // this is to avoid downloading PI for recordings
                    QDate date = currentDate.addDays(day);
                    emit getPI(servId, ueidList, date);
                }
            }
        }

        m_isLoadingFromCache = false;
    }
    else
    { /* do nothing */
    }
}

void MetadataManager::getEpgData()
{
    for (ServiceListConstIterator sIt = m_serviceList->serviceListBegin(); sIt != m_serviceList->serviceListEnd(); ++sIt)
    {
        QList<uint32_t> ueidList;
        for (int e = 0; e < (*sIt)->numEnsembles(); ++e)
        {
            uint32_t ueid = (*sIt)->getEnsemble(e)->id().ueid();
            if (!ueidList.contains(ueid))
            {
                ueidList.append(ueid);
            }
        }
        loadEpg((*sIt)->id(), ueidList);
    }
}

void MetadataManager::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_currentEnsemble = ServiceListId(ens.frequency, ens.ueid);
}

void MetadataManager::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    emit getSI(ServiceListId(s), m_currentEnsemble.ueid());
}

void MetadataManager::addServiceEpg(const ServiceListId &ensId, const ServiceListId &servId)
{
    loadEpg(servId, QList<uint32_t>{ensId.ueid()});
}

void MetadataManager::removeServiceEpg(const ServiceListId &servId)
{
    if (m_epgList.value(servId, nullptr) != nullptr)
    {
        delete m_epgList[servId];
        m_epgList.remove(servId);
        emit epgModelChanged(servId);
    }
    else
    {
        m_epgList.remove(servId);
    }
}

void MetadataManager::clearEpg()
{
    emit epgEmpty();

    // remove all EPG models
    for (const ServiceListId id : m_epgList.keys())
    {
        removeServiceEpg(id);
    }
    m_epgDates.clear();
    emit epgDatesListChanged();
}

void MetadataManager::addEpgDate(const QDate &date)
{
    if (date.isValid() && !m_epgDates.contains(date))
    {
        m_epgDates[date] = date.toString("d. M.");
        emit epgDatesListChanged();
    }
}

QDate MetadataManager::epgDate(int idx) const
{
    if ((idx < 0) || (idx >= m_epgDates.keys().size()))
    {
        return QDate();
    }
    return m_epgDates.keys().at(idx);
}
