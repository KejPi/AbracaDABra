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

#include <QDebug>
#include <QRegularExpression>
#include <QIODevice>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QThread>
#include <QLoggingCategory>

#include "metadatamanager.h"


Q_LOGGING_CATEGORY(metadataManager, "MetadataManager", QtInfoMsg)

MetadataManager::MetadataManager(QObject *parent)
    : QObject(parent)
{
    m_epgModel = new EPGModel(this);
}

void MetadataManager::processXML(const QString &xml, QString scopeId)
{
    QDomDocument xmldocument;
    if (!xmldocument.setContent(xml))
    {
        qCWarning(metadataManager) << "Failed to parse SPI document";
        qCDebug(metadataManager) << xml;
        return;
    }

    qCDebug(metadataManager) << qPrintable(xmldocument.toString());

    QDomElement docElem = xmldocument.documentElement();
    if ("serviceInformation" ==  docElem.tagName())
    {
        QDomNode node = docElem.firstChild();
        while (!node.isNull())        
        {   // ETSI TS 102 818 V3.4.1 [6.2]
            // serviceInformation: can contain zero or one of each of the following elements in this order:
            //      services
            //      serviceGroups
            QDomElement element = node.toElement(); // try to convert the node to an element
            if (!element.isNull())
            {
                if (("services" == element.tagName()) || ("ensemble" == element.tagName()))
                {
                    QDomNode servicesNode = element.firstChild();
                    while (!servicesNode.isNull())
                    {   // ETSI TS 102 818 V3.4.1 [6.3]
                        // Services element can contain zero or one "serviceProvider" element and zero or more "service" elements
                        QDomElement servicesElement = servicesNode.toElement(); // try to convert the node to an element
                        if (!servicesElement.isNull())
                        {
                            if ("service" == servicesElement.tagName())
                            {   // ETSI TS 102 818 V3.4.1 [6.5]
                                // Service element describes metadata and available bearers for a service.
                                QStringList sidList;
                                serviceInfo_t serviceInfo;
                                QDomNode serviceNode = servicesElement.firstChild();
                                while (!serviceNode.isNull())
                                {   // ETSI TS 102 818 V3.4.1 [6.5]
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
                                        QDomElement bearerElement = bearerList.at(b).toElement(); // try to convert the node to an element
                                        if (!bearerElement.isNull())
                                        {
                                            QString bearer = bearerElement.attribute("id", "");
                                            static const QRegularExpression sidRegex("dab:[a-f0-9]([a-f0-9]{2}).\\w{4}.(\\w{4}.\\d+)", QRegularExpression::CaseInsensitiveOption);
                                            QRegularExpressionMatch match = sidRegex.match(bearer);
                                            if (match.hasMatch())
                                            {   // found valid DAB SId ==> service is available in DAB
                                                QString sidStr = match.captured(1) + match.captured(2);
                                                if (!sidList.contains(sidStr))
                                                {   // append if not in list
                                                    sidList.append(sidStr);
                                                }
                                            }
                                        }
                                    }

                                    if (!sidList.isEmpty())
                                    {   // at least one valid DAB SId was found => go through elements
                                        QDomElement serviceElement = serviceNode.toElement(); // try to convert the node to an element
                                        if (!serviceElement.isNull())
                                        {   // ETSI TS 102 818 V3.4.1 [6.5]
                                            // at lease one of these shall be provided
                                            if (("shortName" == serviceElement.tagName()) ||
                                                ("mediumName" == serviceElement.tagName()) ||
                                                ("longName" == serviceElement.tagName()))
                                            {
                                                serviceInfo[serviceElement.tagName()] = serviceElement.text();
                                            }
                                            else if ("mediaDescription" == serviceElement.tagName())
                                            {
                                                QDomNode mediaDescNode = serviceElement.firstChild();
                                                while (!mediaDescNode.isNull())
                                                {
                                                    QDomElement mediaDescElement = mediaDescNode.toElement(); // try to convert the node to an element
                                                    if (!mediaDescElement.isNull())
                                                    {
                                                        if (("shortDescription" == mediaDescElement.tagName()) ||
                                                            ("longDescription" == mediaDescElement.tagName()))
                                                        {   // service description long and/or short
                                                            serviceInfo[mediaDescElement.tagName()] = mediaDescElement.text();
                                                        }
                                                        else if ("multimedia" == mediaDescElement.tagName())
                                                        {   // Service logos shall be provided using the mediaDescription/multimedia element
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
                                                                for (const QString & sidStr : sidList)
                                                                {   // ask for logo for each SId (actually there should be only one valid DAB SId)
                                                                    QString filename = QString("%1/%2x%3.%4").arg(sidStr, width, height, ext);
                                                                    qCDebug(metadataManager) << url << "===>" << filename;

                                                                    emit getFile(url, filename);
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
                                for (const QString & sidStr : sidList)
                                {   // insert in database
                                    qCDebug(metadataManager) << sidStr << serviceInfo;
                                    m_info.insert(sidStr, serviceInfo);
                                }
                            }
                            else if ("serviceProvider" == servicesElement.tagName())
                            {   // not service/ensemble or serviceGroups
                                // this should not happen for XML from internet but it may be coming from binary encondig that does not support serviceProvider element
                                qCDebug(metadataManager, "serviceProvider found");
                            }
                            else
                            {   // not service/ensemble or serviceGroups
                                // this may happen for XML coming from binary encondig that does not support serviceProvider element
                                // in this case services element has id attribute that is UEID
                                // ETSI TS 102 371 V3.2.1 [4.17.1 DAB ensemble element encoding]
                                if (element.hasAttribute("id"))
                                {
                                    qCDebug(metadataManager) << "Ensemble" << element.attribute("id")  << servicesElement.tagName();
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
                {   // not service/ensemble or serviceGroups - this should not happen
                    qCDebug(metadataManager) << element.tagName();
                }
            }
            node = node.nextSibling();
        }
    }
    else if ("epg" ==  docElem.tagName())
    {
        // qCInfo(metadataManager) << "======================= EPG =========================>";
        // qCInfo(metadataManager) << qPrintable(xmldocument.toString());
        // qCInfo(metadataManager) << "<=====================================================";

        QDomNode node = docElem.firstChild();
        while (!node.isNull())
        {
            QDomElement element = node.toElement(); // try to convert the node to an element.
            if(!element.isNull() && ("schedule" == element.tagName()))
            {
                if (!element.firstChildElement("scope").isNull())
                {   // found scope element
                    QDomElement serviceScope = element.firstChildElement("scope").firstChildElement("serviceScope");
                    if (!serviceScope.isNull())
                    {   // found serviceScope
                        scopeId = serviceScope.attribute("id");
                        qDebug() << "Found scopeId:" << scopeId;
                    }
                }
                QDomElement child = element.firstChildElement("programme");
                while (!child.isNull())
                {
                    parseProgramme(child, scopeId);
                    child = child.nextSiblingElement("programme");
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

void MetadataManager::parseProgramme(const QDomElement &element, const QString & scopeId)
{
    QDomElement child = element.firstChildElement();
    EPGModelItem * progItem = new EPGModelItem;
    progItem->setShortId(element.attribute("shortId").toInt());
    while (!child.isNull())
    {
        //qDebug() << "---" << child.tagName();
        if ("location" == child.tagName())
        {
            parseLocation(child, progItem);
        }
        else if ("longName" == child.tagName())
        {
            progItem->setLongName(child.text());
        }
        else if ("mediumName" == child.tagName())
        {
            progItem->setMediumName(child.text());
        }
        else if ("shortName" == child.tagName())
        {
            progItem->setShortName(child.text());
        }
        else if ("mediaDescription" == child.tagName())
        {
            parseDescription(child, progItem);
        }

        child = child.nextSiblingElement();
    }

    m_epgModel->addItem(progItem);
}

void MetadataManager::parseDescription(const QDomElement &element, EPGModelItem *progItem)
{
    QDomElement child = element.firstChildElement();
    while (!child.isNull())
    {
        //qDebug() << "---" << child.tagName();
        if ("location" == child.tagName())
        {
            parseLocation(child, progItem);
        }
        else if ("shortDescription" == child.tagName())
        {
            progItem->setShortDescription(child.text());
        }
        else if ("longDescription" == child.tagName())
        {
            progItem->setLongDescription(child.text());
        }

        child = child.nextSiblingElement();
    }
}
void MetadataManager::parseLocation(const QDomElement &element, EPGModelItem * progItem)
{
    QDomElement child = element.firstChildElement();
    while (!child.isNull())
    {
        //qDebug() << "---" << child.tagName();
        if ("time" == child.tagName())
        {
            // qDebug() << child.attribute("time") << QDateTime::fromString(child.attribute("time"), Qt::ISODate);
            progItem->setStartTime(QDateTime::fromString(child.attribute("time"), Qt::ISODate));

            // ETSI TS 102 818 V3.3.1 (2020-08) [5.2.5 duration type]
            // Duration is based on the ISO 8601 [2] format: PTnHnMnS, where "T" represents the date/time separator,
            // "nH" the number of hours, "nM" the number of minutes and "nS" the number of seconds.
            static const QRegularExpression durationRe("^PT((\\d+)H)?((\\d+)M)?((\\d+)S)?");
            QRegularExpressionMatch match = durationRe.match(child.attribute("duration"));
            int duration = 0;
            if (match.hasMatch())
            {
                // hours
                duration += 3600 * (match.captured(2).isEmpty() ? 0 : match.captured(2).toInt());
                // minutes
                duration += 60 * (match.captured(4).isEmpty() ? 0 : match.captured(4).toInt());
                // seconds
                duration += (match.captured(6).isEmpty() ? 0 : match.captured(6).toInt());
            }
            // qDebug() << child.attribute("duration") << "===>" << duration;
            progItem->setDurationSec(duration);
        }
        else if ("bearer" == child.tagName())
        {
            qDebug() << child.text();
        }

        child = child.nextSiblingElement();
    }
}

void MetadataManager::onFileReceived(const QByteArray & data, const QString & requestId)
{
    if (data.size() == 0)
    {   // empty data, do nothing
        return;
    }

    QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/"+ requestId;
    qCDebug(metadataManager) << requestId << filename;

    const QRegularExpression re("([0-9a-f]{6})\\.(\\d+)/(\\d+x\\d+)\\..*", QRegularExpression::CaseInsensitiveOption);

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
        {   // different file => overwrite
            file.open(QIODevice::WriteOnly);
            file.write(data);
            file.close();

            QRegularExpressionMatch match = re.match(requestId);
            if (match.hasMatch())
            {
                uint32_t sid = match.captured(1).toUInt(nullptr, 16);
                uint8_t scids = match.captured(2).toUInt();
                QString size = match.captured(3);
                MetadataRole role = MetadataRole::SLSLogo;
                if (size == "32x32") {
                    role = MetadataRole::SmallLogo;
                }
                emit dataUpdated(sid, scids, role);
            }
        }
        else
        {   /* do nothing, file is the same */
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
            uint32_t sid = match.captured(1).toUInt(nullptr, 16);
            uint8_t scids = match.captured(2).toUInt();
            QString size = match.captured(3);
            MetadataRole role = MetadataRole::SLSLogo;
            if (size == "32x32") {
                role = MetadataRole::SmallLogo;
            }
            emit dataUpdated(sid, scids, role);
        }
    }
}

QVariant MetadataManager::data(uint32_t sid, uint8_t SCIdS, MetadataRole role)
{
    QString sidStr = QString("%1.%2").arg(sid, 6, 16, QChar('0')).arg(SCIdS);
    switch (role) {
    case SLSLogo:
    {
        QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/320x240.").arg(sidStr);
        if (QFileInfo::exists(filename+"png"))
        {
            QPixmap pixmap;
            pixmap.load(filename+"png");
            return QVariant(pixmap);
        }
        else if (QFileInfo::exists(filename+"jpg"))
        {
            QPixmap pixmap;
            pixmap.load(filename+"jpg");
            return QVariant(pixmap);
        }
    }
    break;
    case SmallLogo:
    {
        QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/32x32.").arg(sidStr);
        if (QFileInfo::exists(filename+"png"))
        {
            QPixmap pixmap;
            pixmap.load(filename+"png");
            return QVariant(pixmap);
        }
        else if (QFileInfo::exists(filename+"jpg"))
        {
            QPixmap pixmap;
            pixmap.load(filename+"jpg");
            return QVariant(pixmap);
        }
    }
    break;
    default:
        break;
    }

    return QVariant();
}

EPGModel *MetadataManager::epgModel() const
{
    return m_epgModel;
}
