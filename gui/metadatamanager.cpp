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
#include "metadatamanager.h"

// initializing instancePtr with NULL
//MetadataManager * MetadataManager::m_instancePtr = nullptr;

MetadataManager::MetadataManager(QObject *parent)
    : QObject(parent)
{

}

void MetadataManager::processXML(const QString &xml)
{
    qDebug() << Q_FUNC_INFO << QThread::currentThreadId();

    //qDebug() << Q_FUNC_INFO;
    QDomDocument xmldocument;
    if (!xmldocument.setContent(xml))
    {
        qDebug() << "Failed to parse SPI document";
    }

    QDomElement docElem = xmldocument.documentElement();
    if ("serviceInformation" ==  docElem.tagName())
    {
        //qDebug("%s", xmldocument.toString().toLocal8Bit().data());

        QDomNode node = docElem.firstChild();
        while (!node.isNull())
        {
            QDomElement element = node.toElement(); // try to convert the node to an element
            if (!element.isNull())
            {
                if (("services" == element.tagName()) || ("ensemble" == element.tagName()))
                {
                    qDebug() << "====> Service information" << element.tagName();

                    QDomNode servicesNode = element.firstChild();
                    while (!servicesNode.isNull())
                    {
                        QDomElement servicesElement = servicesNode.toElement(); // try to convert the node to an element
                        if (!servicesElement.isNull())
                        {
                            if ("service" == servicesElement.tagName())
                            {
                                serviceInfo_t serviceInfo;
#if 0

                                QHash<QString, QString> mediaList;
                                QDomNode serviceNode = servicesElement.firstChild();
                                while (!serviceNode.isNull())
                                {
                                    QDomElement serviceElement = serviceNode.toElement(); // try to convert the node to an element
                                    if (!serviceElement.isNull())
                                    {
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
                                                    {
                                                        serviceInfo[mediaDescElement.tagName()] = mediaDescElement.text();
                                                    }
                                                    else if ("multimedia" == mediaDescElement.tagName())
                                                    {
                                                        QString type = mediaDescElement.attribute("type");
                                                        QString url = mediaDescElement.attribute("url");
                                                        QString width = mediaDescElement.attribute("width");
                                                        QString height = mediaDescElement.attribute("height");

                                                        if ("logo_colour_square" == type)
                                                        {
                                                            width = height = "32";
                                                        }
                                                        else if ("logo_colour_rectangle" == type)
                                                        {
                                                            width = "112";
                                                            height = "32";
                                                        }

                                                        QString ext = url.mid(url.lastIndexOf('.'), url.size()-1);
                                                        QString filename = QString("%1x%2__######%3").arg(width, height, ext);
                                                        qDebug() << url << "===>" << filename;

                                                        emit getFile(url, filename);

                                                    }
                                                }
                                                mediaDescNode = mediaDescNode.nextSibling();
                                            }
                                        }
                                        else if ("bearer" == serviceElement.tagName())
                                        {
                                            QString bearer = serviceElement.attribute("id", "");
                                            static const QRegularExpression sidRegex("dab:[a-f0-9]([a-f0-9]{2}).\\w{4}.(\\w{4}).*", QRegularExpression::CaseInsensitiveOption);
                                            QRegularExpressionMatch match = sidRegex.match(bearer);
                                            if (match.hasMatch())
                                            {
                                                //qDebug() << match.captured(1) << match.captured(2);
                                                QString sidStr = match.captured(1) + match.captured(2);
                                                bool ok;
                                                uint32_t sidValue = sidStr.toInt(&ok, 16);
                                                if (ok)
                                                {
                                                    sid = sidValue;
                                                }
                                                //qDebug("%X", sidStr.toInt(&ok, 16));
                                            }
                                        }
                                    }
                                    serviceNode = serviceNode.nextSibling();
                                }

                                qDebug() << "Service IDs:" << sid;
                                qDebug() << serviceInfo;
#else
                                QString sidStr;
                                QDomNodeList bearerList = servicesElement.elementsByTagName("bearer");
                                for (int b = 0; b < bearerList.count(); ++b)
                                {
                                    //qDebug() << bearerList.at(b).toElement().attribute("id", "UNKNOWN BEARER");
                                    QDomElement bearerElement = bearerList.at(b).toElement(); // try to convert the node to an element
                                    if (!bearerElement.isNull())
                                    {
                                        QString bearer = bearerElement.attribute("id", "");
                                        static const QRegularExpression sidRegex("dab:[a-f0-9]([a-f0-9]{2}).\\w{4}.(\\w{4}.\\d+)", QRegularExpression::CaseInsensitiveOption);
                                        QRegularExpressionMatch match = sidRegex.match(bearer);
                                        if (match.hasMatch())
                                        {
                                            //QDomElement serviceElement = serviceNode.toElement(); // try to convert the node to an element

                                            sidStr = match.captured(1) + match.captured(2);
                                            // found valid DAB SId
                                            QString shortName = servicesElement.attribute("shortName");
                                            serviceInfo["shortName"] = shortName;
                                            QString mediumName = servicesElement.attribute("mediumName");
                                            serviceInfo["mediumName"] = mediumName;
                                            QString longName = servicesElement.attribute("longName");
                                            serviceInfo["longName"] = mediumName;

                                        }
                                    }
                                }

                                if (!sidStr.isEmpty())
                                {
                                    QDomNode serviceNode = servicesElement.firstChild();
                                    while (!serviceNode.isNull())
                                    {
                                        QDomElement serviceElement = serviceNode.toElement(); // try to convert the node to an element
                                        if (!serviceElement.isNull())
                                        {
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
                                                        {
                                                            serviceInfo[mediaDescElement.tagName()] = mediaDescElement.text();
                                                        }
                                                        else if ("multimedia" == mediaDescElement.tagName())
                                                        {
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
                                                            if (((widthVal == 320) && (heightVal == 240)) ||
                                                                ((widthVal == 32) && (heightVal == 32)))
                                                            {
                                                                QString filename = QString("%1/%2x%3.%4").arg(sidStr, width, height, ext);
                                                                qDebug() << url << "===>" << filename;

                                                                emit getFile(url, filename);
                                                            }
                                                        }
                                                    }
                                                    mediaDescNode = mediaDescNode.nextSibling();
                                                }
                                            }
                                        }
                                        serviceNode = serviceNode.nextSibling();
                                    }
                                    qDebug() << "Service IDs:" << sidStr;
                                    qDebug() << serviceInfo;
                                    m_info.insert(sidStr, serviceInfo);
                                }
#endif
                            }
                        }
                        servicesNode = servicesNode.nextSibling();
                    }
                }
                else { /* not service/ensemble information */  }
            }

            node = node.nextSibling();
        }
    }
}

void MetadataManager::onFileReceived(const QByteArray & data, const QString & requestId)
{
    QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/"+ requestId;
    qDebug() << Q_FUNC_INFO << requestId << filename;

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
                StationLogoRole role = StationLogoRole::SLSLogo;
                if (size == "32x32") {
                    role = StationLogoRole::SmallLogo;
                }
                emit logoUpdated(sid, scids, role);
            }
        }
        else
        {   /* do nothing, file is the same */
            qDebug() << filename << "is the same";
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
            StationLogoRole role = StationLogoRole::SLSLogo;
            if (size == "32x32") {
                role = StationLogoRole::SmallLogo;
            }
            emit logoUpdated(sid, scids, role);
        }
    }
}

QPixmap MetadataManager::getStationLogo(uint32_t sid, uint8_t SCIdS, StationLogoRole role)
{
    QPixmap pixmap;
    QString sidStr = QString("%1.%2").arg(sid, 6, 16, QChar('0')).arg(SCIdS);
    switch (role) {
    case SLSLogo:
    {
        QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/320x240.").arg(sidStr);
        if (QFileInfo::exists(filename+"png"))
        {
            pixmap.load(filename+"png");
        }
        else if (QFileInfo::exists(filename+"jpg"))
        {
            pixmap.load(filename+"jpg");
        }

    }
        break;
    case SmallLogo:
    {
        QString filename = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + QString("%1/32x32.").arg(sidStr);
        if (QFileInfo::exists(filename+"png"))
        {
            pixmap.load(filename+"png");
        }
        else if (QFileInfo::exists(filename+"jpg"))
        {
            pixmap.load(filename+"jpg");
        }
    }
        break;
    default:
        break;
    }

    return pixmap;
}
