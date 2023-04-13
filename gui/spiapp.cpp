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

#include "spiapp.h"

SPIApp::SPIApp(QObject *parent) : UserApplication(parent)
{
    m_decoder = nullptr;
}

SPIApp::~SPIApp()
{
    if (nullptr != m_decoder)
    {
        delete m_decoder;
    }
}

void SPIApp::start()
{   // does nothing if application is already running
    if (m_isRunning)
    {   // do nothing, application is running
        return;
    }
    else
    { /* not running */ }

    // create new decoder
    m_decoder = new MOTDecoder();
    connect(m_decoder, &MOTDecoder::newMOTObject, this, &SPIApp::onNewMOTObject);
    connect(m_decoder, &MOTDecoder::newMOTDirectory, this, &SPIApp::onNewMOTDirectory);

    m_isRunning = true;
}

void SPIApp::stop()
{
    if (nullptr != m_decoder)
    {
        delete m_decoder;
        m_decoder = nullptr;
    }
    m_isRunning = false;

    // ask HMI to clear data
    emit resetTerminal();
}

void SPIApp::restart()
{
    stop();
    start();
}

void SPIApp::onUserAppData(const RadioControlUserAppData & data)
{
    if ((DabUserApplicationType::SPI == data.userAppType) && (m_isRunning))
    {
        // application is running and user application type matches
        // data is for this application
        // send data to decoder
        m_decoder->newDataGroup(data.data);
    }
    else
    { /* do nothing */ }
}

void SPIApp::onNewMOTDirectory()
{
    qDebug() << Q_FUNC_INFO << "New MOT directory";
    MOTObjectCache::const_iterator objIt;
    for (objIt = m_decoder->directoryBegin(); objIt != m_decoder->directoryEnd(); ++objIt)
    {
        if (!objIt->isComplete())
        {
            qDebug() << "MOT object, ID =" << objIt->getId() << ", contentsName =" << objIt->getContentName() << "is NOT complete!";
            continue;
        }
        else
        { /* MOT object is complete */ }

        qDebug() << "MOT object, ID =" << objIt->getId() << ", contentsName =" << objIt->getContentName()
                 << "Content type/subtype =" << objIt->getContentType() << "/" << objIt->getContentSubType();

        if (objIt->getContentType() == 7)
        {   // SPI content type/subtype values
            switch (objIt->getContentSubType())
            {
            case 0:
                qDebug() << "\tService Information";
                parseServiceInfo(*objIt);
                break;
            case 1:
                qDebug() << "\tProgramme Information";
                break;
            case 2:
                qDebug() << "\tGroup Information";
                break;
            default:
                // not supported
                continue;
            }
        }
    }
}

void SPIApp::onNewMOTObject(const MOTObject & obj)
{
    qDebug() << Q_FUNC_INFO;
}

void SPIApp::parseServiceInfo(const MOTObject &motObj)
{
    MOTObject::paramsIterator paramIt;
    for (paramIt = motObj.paramsBegin(); paramIt != motObj.paramsEnd(); ++paramIt)
    {
        switch (DabMotExtParameter(paramIt.key()))
        {
        case DabMotExtParameter::ProfileSubset:
            qDebug() << "\tProfileSubset";
            break;
        case DabMotExtParameter::CompressionType:
            qDebug() << "\tCompressionType - not supported";
            return;
            break;
        case DabMotExtParameter::CAInfo:
            // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.5 CAInfo] If this parameter is present a non CA-capable device shall discard this MOT object.
            qDebug() << "\tCAInfo";
            return;
            break;
        default:
            switch (Parameter(paramIt.key()))
            {
#if 1
            case Parameter::ScopeStart:
                // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.6 ScopeStart] This parameter is used for Programme Information SPI objects only
                qDebug() << "\tScopeStart";
                break;
            case Parameter::ScopeEnd:
                // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.7 ScopeEnd] This parameter is used for Programme Information SPI objects only
                qDebug() << "\tScopeEnd";
                break;
#endif
            case Parameter::ScopeID:
                if (paramIt.value().size() >= 3)
                {
                    uint32_t ueid = (uint8_t(paramIt.value().at(0)) << 16) | (uint8_t(paramIt.value().at(1)) << 8) | uint8_t(paramIt.value().at(2));
                    qDebug("\tScopeID: %6.6X", ueid);

//                    if (m_radioControl->getEnsembleUEID() != ueid)
//                    {
//                        qDebug("ScopeID: %6.6X is not current ensemble. Service info for current ensemble is only supported!", ueid);
//                        return;
//                    }
                }
                else
                {   // unexpected length
                   qDebug() << "\tScopeID: error";
                }

                break;
            default:
                // not supported
                break;
            }
            break;
        }
    }

    const QByteArray data = motObj.getBody();
    m_xmldocument.clear();
    m_tokenTable.clear();
    m_defaultLanguage.clear();
    QDomProcessingInstruction header = m_xmldocument.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
    m_xmldocument.appendChild(header);
    //m_xmldocument = QDomDocument("XML");
    const uint8_t * dataPtr = (uint8_t *) data.data();
    dataPtr += parseTag(dataPtr, nullptr, uint8_t(SPIElement::Tag::_invalid), data.size());

    qDebug("%s", m_xmldocument.toString().toLocal8Bit().data());
}


uint32_t SPIApp::parseTag(const uint8_t * dataPtr, QDomElement *parentElement, uint8_t parentTag, int maxSize)
{
//    qDebug("%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X",
//            *dataPtr, *(dataPtr+1), *(dataPtr+2), *(dataPtr+3), *(dataPtr+4), *(dataPtr+5), *(dataPtr+6),
//            *(dataPtr+7), *(dataPtr+8), *(dataPtr+9));

    if (maxSize < 2)
    {   // not enough data
        return maxSize;
    }

    uint8_t tag = *dataPtr;
    int len = *(dataPtr+1);
    int_fast32_t bytesRead = 2;

    if (maxSize < len + 2)
    {   // not enough data
        return maxSize;
    }

    if (0xFE == len)
    {
        if (maxSize < 4)
        {   // not enough data
            return maxSize;
        }
        len = (*(dataPtr+2) << 8) | *(dataPtr+3);
        bytesRead += 2;

        if (maxSize < len + 4)
        {   // not enough data
            return maxSize;
        }
    }
    else if (0xFF == len)
    {
        if (maxSize < 5)
        {   // not enough data
            return maxSize;
        }
        len = (*(dataPtr+2) << 16) | (*(dataPtr+3) << 8) | *(dataPtr+4);
        bytesRead += 3;

        if (maxSize < len + 5)
        {   // not enough data
            return maxSize;
        }
    }
    else
    { /* len < 0xFE */ }

    // we know that we have enough data here
    //qDebug("Tag = %2.2X, len = %d", tag, len);

    QDomElement element;
    if (tag < 0x80)
    {   // element tags
        switch (SPIElement::Tag(tag))
        {
        case SPIElement::Tag::CDATA:
        {
            QString cdata = QString::fromUtf8((const char *)(dataPtr+bytesRead), len);

            // replace tokens with strings
            QHash<uint8_t, QString>::const_iterator it = m_tokenTable.constBegin();
            while (it != m_tokenTable.cend())
            {
                cdata = cdata.replace(QChar(it.key()), it.value());
                ++it;
            }
            bytesRead += len;
            qDebug() << "CDATA" << cdata;
            if (nullptr != parentElement)
            {
                parentElement->appendChild(m_xmldocument.createTextNode(cdata));
            }
            else { /* do nothing here */ }

        }
            break;
        case SPIElement::Tag::epg:
            // not supported
            break;
        case SPIElement::Tag::serviceInformation:           
            element = m_xmldocument.createElement("serviceInformation");
            break;
        case SPIElement::Tag::tokenTable:
            m_tokenTable.clear();
            while (len > bytesRead)
            {
                uint8_t tokenId = *(dataPtr+bytesRead);
                uint8_t tokenLen = *(dataPtr+bytesRead+1);
                QString token = QString::fromUtf8((const char *)(dataPtr+bytesRead+2), tokenLen);
                m_tokenTable.insert(tokenId, token);
                bytesRead += tokenLen+2;
                qDebug() << "\t\ttoken" << tokenId << " : " << token;
            }
            break;
        case SPIElement::Tag::defaultLanguage:
            // ETSI TS 102 371 V3.2.1 [4.11]
            // This element is not defined in the XML specification. This element can only occur within the top-level elements
            // serviceInformation and epg and, if present, it shall occur after the string token table (if present) and before any other child elements.
            // This element applies to all elements within the parent top-level element and all children of the parent element.
            // If the default language element is present, then whenever an xml:lang attribute with the same value as the default language occurs within an element,
            // it does not need to be encoded. Whenever a decoder finds a missing xml:lang attribute for an element, then it shall use the default language value.
            m_defaultLanguage = getString(dataPtr+bytesRead, len, false);
            qDebug() << "defaultLanguage" << m_defaultLanguage;
            bytesRead += len;
            break;
        case SPIElement::Tag::shortName:
            element = m_xmldocument.createElement("shortName");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::mediumName:
            element = m_xmldocument.createElement("mediumName");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::longName:
            element = m_xmldocument.createElement("longName");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::mediaDescription:
            element = m_xmldocument.createElement("mediaDescription");
            break;
        case SPIElement::Tag::genre:
            element = m_xmldocument.createElement("genre");
            break;
        case SPIElement::Tag::keywords:
            element = m_xmldocument.createElement("keywords");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::memberOf:
            element = m_xmldocument.createElement("memberOf");
            break;
        case SPIElement::Tag::link:
            element = m_xmldocument.createElement("link");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::location:            
            element = m_xmldocument.createElement("location");
            break;
        case SPIElement::Tag::shortDescription:
            element = m_xmldocument.createElement("shortDescription");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::longDescription:
            element = m_xmldocument.createElement("longDescription");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::programme:
            element = m_xmldocument.createElement("programme");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::programmeGroups:
            element = m_xmldocument.createElement("programmeGroups");
            break;
        case SPIElement::Tag::schedule:
            element = m_xmldocument.createElement("schedule");
            break;
        case SPIElement::Tag::programmeGroup:
            element = m_xmldocument.createElement("programmeGroup");
            break;
        case SPIElement::Tag::scope:
            element = m_xmldocument.createElement("scope");
            break;
        case SPIElement::Tag::serviceScope:
            element = m_xmldocument.createElement("serviceScope");
            break;
        case SPIElement::Tag::ensemble:
            element = m_xmldocument.createElement("ensemble");
            break;
        case SPIElement::Tag::service:
            element = m_xmldocument.createElement("service");
            break;
        case SPIElement::Tag::bearer_serviceID:
            element = m_xmldocument.createElement("bearer");
            break;
        case SPIElement::Tag::multimedia:
            element = m_xmldocument.createElement("multimedia");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::time:
            element = m_xmldocument.createElement("time");
            break;
        case SPIElement::Tag::bearer:
            element = m_xmldocument.createElement("bearer");
            break;
        case SPIElement::Tag::programmeEvent:
            element = m_xmldocument.createElement("programmeEvent");
            if (!m_defaultLanguage.isEmpty())
            {
                element.setAttribute("xml:lang", m_defaultLanguage);
            }
            else { /* no default language */ }
            break;
        case SPIElement::Tag::relativeTime:
            element = m_xmldocument.createElement("relativeTime");
            break;
        case SPIElement::Tag::radiodns:
            element = m_xmldocument.createElement("radiodns");
            break;
        case SPIElement::Tag::geolocation:
            element = m_xmldocument.createElement("geolocation");
            break;
        case SPIElement::Tag::country:
            element = m_xmldocument.createElement("country");
            break;
        case SPIElement::Tag::point:
            element = m_xmldocument.createElement("point");
            break;
        case SPIElement::Tag::polygon:
            element = m_xmldocument.createElement("polygon");
            break;
        case SPIElement::Tag::onDemand:
            element = m_xmldocument.createElement("onDemand");
            break;
        case SPIElement::Tag::presentationTime:
            element = m_xmldocument.createElement("presentationTime");
            break;
        case SPIElement::Tag::acquisitionTime:
            element = m_xmldocument.createElement("acquisitionTime");
            break;
        default:
            // unknown
            bytesRead += len;
            break;
        }

        // parse attributes
        if (!element.tagName().isEmpty())
        {
            qDebug("<%s>", element.tagName().toLatin1().data());
            while (len > bytesRead)
            {
                bytesRead += parseTag(dataPtr+bytesRead, &element, tag, len);
            }
            qDebug("</%s>", element.tagName().toLatin1().data());

            if (nullptr == parentElement)
            {
                m_xmldocument.appendChild(element);
            }
            else
            {
                parentElement->appendChild(element);
            }
        }
    }
    else
    {   // attributes
        switch (SPIElement::Tag(parentTag))
        {
        case SPIElement::Tag::epg:
            qDebug() << "attributes: epg";
            break;
        case SPIElement::Tag::serviceInformation:
            switch (SPIElement::serviceInformation::attribute(tag))
            {
            case SPIElement::serviceInformation::attribute::version:
            {   // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                uint16_t version = 0;
                for (int n = 0; n<len; ++n)
                {
                    version = (version << 8) | *(dataPtr+bytesRead+n);
                }
                qDebug("version=\"%d\"", version);
                parentElement->setAttribute("version", version);
            }
                break;
            case SPIElement::serviceInformation::attribute::creationTime:
            {
                QString time  = getTime(dataPtr+bytesRead, len);
                qDebug() << "attribute:creationTime" << time;
                parentElement->setAttribute("creationTime", time);
            }
                break;
            case SPIElement::serviceInformation::attribute::originator:
            {
                QString originator = getString(dataPtr+bytesRead, len, false);
                qDebug() << "attribute:originator" << originator;
                parentElement->setAttribute("originator", originator);

            }
                break;
            case SPIElement::serviceInformation::attribute::serviceProvider:
            {
                QString serviceProvider = getString(dataPtr+bytesRead, len, false);
                qDebug() << "attribute:serviceProvider" << serviceProvider;
                parentElement->setAttribute("serviceProvider", serviceProvider);
            }
                break;
            }
            break;
//        case SPIElement::Tag::tokenTable:
//            break;
//        case SPIElement::Tag::defaultLanguage:
//            break;
        case SPIElement::Tag::shortName:
            switch (SPIElement::shortName::attribute(tag))
            {
                case SPIElement::shortName::attribute::xml_lang:
                {
                    QString lang = getString(dataPtr+bytesRead, len, false);
                    qDebug() << "xml:lang" << lang;
                    parentElement->setAttribute("xml:lang", lang);
                }
            break;
            }
            break;
        case SPIElement::Tag::mediumName:
            switch (SPIElement::mediumName::attribute(tag))
            {
                case SPIElement::mediumName::attribute::xml_lang:
                {
                    QString lang = getString(dataPtr+bytesRead, len, false);
                    qDebug() << "xml:lang" << lang;
                    parentElement->setAttribute("xml:lang", lang);
                }
                    break;
            }
            break;
        case SPIElement::Tag::longName:
            switch (SPIElement::longName::attribute(tag))
            {
            case SPIElement::longName::attribute::xml_lang:
            {
                    QString lang = getString(dataPtr+bytesRead, len, false);
                    qDebug() << "xml:lang" << lang;
                    parentElement->setAttribute("xml:lang", lang);
            }
            break;
            }
            break;
        case SPIElement::Tag::shortDescription:
            switch (SPIElement::shortDescription::attribute(tag))
            {
            case SPIElement::shortDescription::attribute::xml_lang:
            {
                    QString lang = getString(dataPtr+bytesRead, len, false);
                    qDebug() << "xml:lang" << lang;
                    parentElement->setAttribute("xml:lang", lang);
            }
            break;
            }
            break;
        case SPIElement::Tag::longDescription:
            switch (SPIElement::longDescription::attribute(tag))
            {
            case SPIElement::longDescription::attribute::xml_lang:
            {
                    QString lang = getString(dataPtr+bytesRead, len, false);
                    qDebug() << "xml:lang" << lang;
                    parentElement->setAttribute("xml:lang", lang);
            }
            break;
            }
            break;
//        case SPIElement::Tag::mediaDescription:
//            break;
        case SPIElement::Tag::genre:
            switch (SPIElement::genre::attribute(tag))
            {
            case SPIElement::genre::attribute::href:
                qDebug() << "genre::attribute::href";
                break;
            case SPIElement::genre::attribute::type:
                switch (*(dataPtr+bytesRead))
                {
                case 0x01:
                    qDebug() << "type=\"main\"";
                    parentElement->setAttribute("type", "main");
                    break;
                case 0x02:
                    qDebug() << "type=\"secondary\"";
                    parentElement->setAttribute("type", "secondary");
                    break;
                case 0x03:
                    qDebug() << "type=\"other\"";
                    parentElement->setAttribute("type", "other");
                    break;
                }
                break;
            }
            break;
        case SPIElement::Tag::keywords:
            qDebug() << "attributes: keywords";
            break;
        case SPIElement::Tag::memberOf:
            qDebug() << "attributes: memberOf";
            break;
        case SPIElement::Tag::location:
            qDebug() << "attributes: location";
            break;
        case SPIElement::Tag::programme:
            qDebug() << "attributes: programme";
            break;
        case SPIElement::Tag::programmeGroups:
            qDebug() << "attributes: programmeGroups";
            break;
        case SPIElement::Tag::schedule:
            qDebug() << "attributes: schedule";
            break;
        case SPIElement::Tag::programmeGroup:
            qDebug() << "attributes: programmeGroup";
            break;
        case SPIElement::Tag::scope:
            qDebug() << "attributes: scope";
            break;
        case SPIElement::Tag::serviceScope:
            qDebug() << "attributes: serviceScope";
            break;
        case SPIElement::Tag::ensemble:
            switch (SPIElement::ensemble::attribute(tag))
            {
            case SPIElement::ensemble::attribute::id:
                uint8_t ecc = *(dataPtr+bytesRead);
                uint16_t eid = (*(dataPtr+bytesRead+1) << 8) | *(dataPtr+bytesRead+2);
                qDebug("attribute:id %2.2x.%4.4X", ecc, eid);
                parentElement->setAttribute("id", QString("%1.%2").arg(ecc, 2, 16, QChar('0')).arg(eid, 4, 16, QChar('0')));
                break;
            }
            break;
        case SPIElement::Tag::service:
            switch (SPIElement::service::attribute(tag))
            {
            case SPIElement::service::attribute::version:
            {   // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                uint16_t version = 0;
                for (int n = 0; n<len; ++n)
                {
                    version = (version << 8) | *(dataPtr+bytesRead+n);
                }
                qDebug("version=\"%d\"", version);
                parentElement->setAttribute("version", version);
            }
                break;
            }
            break;
        case SPIElement::Tag::bearer_serviceID:
        {
            if (len >= 6)
            {
                dataPtr += bytesRead;
                uint8_t scids = *dataPtr & 0x0F;
                uint8_t ecc = *(dataPtr+1);
                uint16_t eid = (*(dataPtr+2) << 8) | *(dataPtr+3);
                uint32_t sid;
                if (*dataPtr & 0x10)
                {  // long SId
                    sid = (*(dataPtr+4) << 24) | (*(dataPtr+5) << 16) | (*(dataPtr+6) << 8) | *(dataPtr+7);
                    qDebug("id=\"dab:c%2.2x:%4.4x.%8.8x.%d\"", ecc, eid, sid, scids);
                    parentElement->setAttribute("id", QString("dab:c%1:%2.%3.%4")
                                                          .arg(ecc, 2, 16, QChar('0'))
                                                          .arg(eid, 4, 16, QChar('0'))
                                                          .arg(sid, 8, 16, QChar('0'))
                                                          .arg(scids)
                                                );
                }
                else
                {  // short SId
                    sid = (*(dataPtr+4) << 8) | *(dataPtr+5);
                    qDebug("id=\"dab:c%2.2x:%4.4x.%4.4x.%d\"", ecc,eid, sid, scids);
                    parentElement->setAttribute("id", QString("dab:c%1:%2.%3.%4")
                                                          .arg(ecc, 2, 16, QChar('0'))
                                                          .arg(eid, 4, 16, QChar('0'))
                                                          .arg(sid, 4, 16, QChar('0'))
                                                          .arg(scids)
                                                );
                }
            }
        }
            break;
        case SPIElement::Tag::multimedia:
            switch (SPIElement::multimedia::attribute(tag))
            {
            case SPIElement::multimedia::attribute::mimeValue:
            {
                QString mime = getString(dataPtr+bytesRead, len, false);
                qDebug() << "mime =" << mime;
                parentElement->setAttribute("mimeValue", mime);
                break;
            }
                break;
            case SPIElement::multimedia::attribute::xml_lang:
            {
                QString lang = getString(dataPtr+bytesRead, len, false);
                qDebug() << "xml:lang" << lang;
                parentElement->setAttribute("xml:lang", lang);
            }
                break;
            case SPIElement::multimedia::attribute::url:
            {
                QString url = QString::fromUtf8((const char *)(dataPtr+bytesRead), len);
                qDebug() << "url =" << url;
                parentElement->setAttribute("url", url);
            }
                break;
            case SPIElement::multimedia::attribute::type:
                switch (*(dataPtr+bytesRead))
                {
                case 0x02:
                    qDebug() << "type=\"logo_unrestricted\"";
                    parentElement->setAttribute("type", "logo_unrestricted");
                    break;
                case 0x04:
                    qDebug() << "type=\"logo_colour_square\"";
                    parentElement->setAttribute("type", "logo_colour_square");
                    break;
                case 0x06:
                    qDebug() << "type=\"logo_colour_rectangle\"";
                    parentElement->setAttribute("type", "logo_colour_rectangle");
                    break;
                }

                break;
            case SPIElement::multimedia::attribute::width:
                dataPtr += bytesRead;
                parentElement->setAttribute("width", (*(dataPtr) << 8) | *(dataPtr+1));
                break;
            case SPIElement::multimedia::attribute::height:
                dataPtr += bytesRead;
                parentElement->setAttribute("height", (*(dataPtr) << 8) | *(dataPtr+1));
                break;
            }
            break;
        case SPIElement::Tag::time:
            qDebug() << "attributes: time";
            break;
        case SPIElement::Tag::bearer:
            qDebug() << "attributes: bearer";
            break;
        case SPIElement::Tag::programmeEvent:
            qDebug() << "attributes: programmeEvent";
            break;
        case SPIElement::Tag::relativeTime:
            qDebug() << "attributes: relativeTime";
            break;
        case SPIElement::Tag::radiodns:
            qDebug() << "attributes: radiodns";
            break;
        case SPIElement::Tag::geolocation:
            qDebug() << "attributes: geolocation";
            break;
        case SPIElement::Tag::country:
            qDebug() << "attributes: country";
            break;
        case SPIElement::Tag::point:
            qDebug() << "attributes: point";
            break;
        case SPIElement::Tag::polygon:
            qDebug() << "attributes: polygon";
            break;
        case SPIElement::Tag::onDemand:
            qDebug() << "attributes: onDemand";
            break;
        case SPIElement::Tag::presentationTime:
            qDebug() << "attributes: presentationTime";
            break;
        case SPIElement::Tag::acquisitionTime:
            qDebug() << "attributes: acquisitionTime";
            break;
        default:
            // unknown
            break;
        }
        bytesRead+=len;
    }

    return bytesRead;
}

const uint8_t *SPIApp::parseAttributes(const uint8_t *attrPtr, uint8_t tag, int maxSize)
{
    switch (tag)
    {
    case 0x03:
        break;
    default:
        break;
    }

    return nullptr;
}

QString SPIApp::getString(const uint8_t *dataPtr, int len, bool doReplaceTokens)
{
    QString str = QString::fromUtf8((const char *) dataPtr, len);

    if (!doReplaceTokens)
    {   // we are done
        return str;
    }
    else { /* need to replace tokens */ }

    // replace tokens with strings
    QHash<uint8_t, QString>::const_iterator it = m_tokenTable.constBegin();
    while (it != m_tokenTable.cend())
    {
        str = str.replace(QChar(it.key()), it.value());
        ++it;
    }

    return str;
}

QString SPIApp::getTime(const uint8_t *dataPtr, int len)
{
    uint32_t dateHoursMinutes = *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;

    uint16_t secMsec = 0;
    if (dateHoursMinutes & (1 << 11))
    {   // UTC flag
        if (len < 6)
        {
            return QString();
        }
        else { /* enough data */}

        secMsec = *dataPtr << 8;
        dataPtr += 2;  // rfu
    }
    else { /* short form */ }

    int8_t lto = 0;
    if (dateHoursMinutes & (1 << 12))
    {   // LTO flag
        if (len < 7)
        {
            return QString();
        }
        else { /* enough data */}

        lto = *dataPtr & 0x1F;
        if (*dataPtr & (1 << 5))
        {
            lto = -lto;
        }
        else { /* positive */ }
    }
    else { /* no LTO */ }

    // construct time
    QDateTime dateAndTime = DabTables::dabTimeToUTC(dateHoursMinutes, secMsec).toOffsetFromUtc(60*(lto * 30));

    // convert to string
    return dateAndTime.toString(Qt::ISODateWithMs);
}

SPIDomElement::SPIDomElement()
{
    m_tag = SPIElement::Tag::_invalid;
}

SPIDomElement::SPIDomElement(const QDomElement &e, uint8_t tag)
    : m_element(e)
    , m_tag(SPIElement::Tag(tag))
{
}

QDomElement SPIDomElement::element() const
{
    return m_element;
}

void SPIDomElement::setElement(const QDomElement &newElement)
{
    m_element = newElement;
}

SPIElement::Tag SPIDomElement::tag() const
{
    return m_tag;
}

void SPIDomElement::setTag(uint8_t newTag)
{
    m_tag = SPIElement::Tag(newTag);
}
