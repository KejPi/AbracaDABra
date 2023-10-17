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
#include <QStandardPaths>
#include <QSaveFile>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QNetworkProxyFactory>
#include <QLoggingCategory>
#include <QJsonDocument>

Q_LOGGING_CATEGORY(spiApp, "SPIApp", QtDebugMsg)

SPIApp::SPIApp(QObject *parent) : UserApplication(parent)
{
    //m_decoder = nullptr;
    m_dnsLookup = nullptr;
    m_netAccessManager = nullptr;
    m_useInternet = false;
    m_enaRadioDNS = false;    
    m_useDoH = false;
}

SPIApp::~SPIApp()
{
//    if (nullptr != m_decoder)
//    {
//        delete m_decoder;
//    }
    // delete all decoders
    for (const auto & decoder : m_decoderMap)
    {
        delete decoder;
    }
    m_decoderMap.clear();
    if (nullptr != m_dnsLookup)
    {
        delete m_dnsLookup;
    }
    if (nullptr != m_netAccessManager)
    {
        delete m_netAccessManager;
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

//    // create new decoder
//    m_decoder = new MOTDecoder();
//    connect(m_decoder, &MOTDecoder::newMOTObject, this, &SPIApp::onNewMOTObject);
//    connect(m_decoder, &MOTDecoder::newMOTDirectory, this, &SPIApp::onNewMOTDirectory);

    if (nullptr == m_dnsLookup)
    {
        m_dnsLookup = new QDnsLookup();
        connect(m_dnsLookup, &QDnsLookup::finished, this, &SPIApp::handleRadioDNSLookup);
    }

    if (nullptr == m_netAccessManager)
    {
        m_netAccessManager = new QNetworkAccessManager();
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        connect(m_netAccessManager, &QNetworkAccessManager::finished, this, &SPIApp::onFileDownloaded);

        QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
        QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1StringView("/cacheDir/");
        diskCache->setCacheDirectory(directory);
        m_netAccessManager->setCache(diskCache);
    }

    QNetworkProxyQuery npq(QUrl(QLatin1String("https://dns.google")));
    QList<QNetworkProxy> listOfProxies = QNetworkProxyFactory::systemProxyForQuery(npq);
    foreach (QNetworkProxy p, listOfProxies) {
        qCDebug(spiApp) << "Proxy hostname:" << p.hostName() << p.type();
    }

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    m_downloadReqQueue.clear();

    m_isRunning = true;
}

void SPIApp::stop()
{
    // get XPAD application decoder (SCID == 0xFFFF)
    MOTDecoder * decoderPtr = m_decoderMap.value(0xFFFF, nullptr);
    if (nullptr != decoderPtr)
    {
        m_parsedDirectoryIds.remove(decoderPtr->getDirectoryId());
        delete decoderPtr;
        m_decoderMap.remove(0xFFFF);
    }
    if (nullptr != m_dnsLookup)
    {
        delete m_dnsLookup;
        m_dnsLookup = nullptr;
    }
    if (nullptr != m_netAccessManager)
    {
        delete m_netAccessManager;
        m_netAccessManager = nullptr;
    }
    m_isRunning = false;
}

void SPIApp::restart()
{
    stop();
    start();
}

void SPIApp::reset()
{
    // delete all decoders
    for (const auto & decoder : m_decoderMap)
    {
        delete decoder;
    }
    m_decoderMap.clear();
    m_parsedDirectoryIds.clear();
    m_isRunning = false;

    // ask HMI to clear data
    emit resetTerminal();
}

void SPIApp::enable(bool ena)
{
    reset();
    if (ena)
    {
        start();
    }
    else
    {
        stop();
    }
}

void SPIApp::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_ueid = QString("%1").arg(ens.ueid, 6, 16, QChar('0'));
}

void SPIApp::onAudioServiceSelection(const RadioControlServiceComponent &s)
{
    m_sid = QString("%1").arg(s.SId.progSId(), 4, 16, QChar('0'));
    m_scids = QString("%1").arg(s.SCIdS);
    m_gcc = getGCC(s.SId);

    if (m_useInternet && m_enaRadioDNS)
    {   // query RadioDNS
        radioDNSLookup();
    }
}

void SPIApp::onUserAppData(const RadioControlUserAppData & data)
{
    if ((DabUserApplicationType::SPI == data.userAppType) && (m_isRunning))
    {
        // application is running and user application type matches
        MOTDecoder * decoderPtr = m_decoderMap.value(data.SCId, nullptr);
        if (nullptr == decoderPtr)
        {   // we do not have decoder for this channel
            // create new decoder
            decoderPtr = new MOTDecoder(this);
            // connect(decoderPtr, &MOTDecoder::newMOTObject, this, &SPIApp::onNewMOTObject);
            connect(decoderPtr, &MOTDecoder::newMOTDirectory, this, &SPIApp::onNewMOTDirectory);
            m_decoderMap[data.SCId] = decoderPtr;

            qCDebug(spiApp) << "Adding MOT decoder for SCID" << data.SCId;
        }

        // send data to decoder
        decoderPtr->newDataGroup(data.data);
    }
    else
    { /* do nothing */ }
}

void SPIApp::onNewMOTDirectory()
{
    MOTDecoder * decoderPtr = dynamic_cast<MOTDecoder *>(QObject::sender());
    Q_ASSERT(decoderPtr != nullptr);

    int decoderId = 0xF000;
    for (const auto & key : m_decoderMap.keys()) {
        if (m_decoderMap.value(key) == decoderPtr) {
            decoderId = key;
            break;
        }
    }

    if (decoderPtr->getDirectoryId() == m_parsedDirectoryIds.value(decoderId, 0))
    {   // directory is already processed
        // qCDebug(spiApp) << decoderId << ": Directory" << decoderPtr->getDirectoryId() << "was processed already, skipping";
        return;
    }

    MOTObjectCache::const_iterator objIt;
    int incompleteObjCount = 0;
    for (objIt = decoderPtr->directoryBegin(); objIt != decoderPtr->directoryEnd(); ++objIt)
    {
        if (!objIt->isComplete())
        {   // object not complete ==> carousel not complete
            incompleteObjCount += 1;
            // qCDebug(spiApp, "%d: Object %d is not complete", decoderId, objIt->getId());
        }
        else { /* object is complete */
            // qCDebug(spiApp, "%d: Object %d is complete: %s", decoderId, objIt->getId(), objIt->getContentName().toLocal8Bit().data());
        }
    }
    if (incompleteObjCount != 0) {
        qCDebug(spiApp, "%d: MOT directory NOT complete (missing %d / %d)", decoderId, incompleteObjCount, decoderPtr->size());
        return;
    }
    else
    {
        qCInfo(spiApp, "%d: MOT directory complete", decoderId);
        for (objIt = decoderPtr->directoryBegin(); objIt != decoderPtr->directoryEnd(); ++objIt)
        {
            qCDebug(spiApp, "%d:     Object %d -> %s", decoderId, objIt->getId(), objIt->getContentName().toLocal8Bit().data());
        }
    }

    qCDebug(spiApp, "%d: Processing MOT directory", decoderId);

    for (objIt = decoderPtr->directoryBegin(); objIt != decoderPtr->directoryEnd(); ++objIt)
    {
        if (objIt->isComplete())
        {
            switch (objIt->getContentType())
            {
#if 0  // storing of images
            case 2:
            {
                // logos
                switch (objIt->getContentSubType())
                {
                case 1:
                    qCDebug(spiApp) << "Image / JFIF (JPEG)" << objIt->getContentName();
                    break;
                case 3:
                {
                    qCDebug(spiApp) << "Image / PNG" << objIt->getContentName();
                    QSaveFile file(objIt->getContentName());
                    file.open(QIODevice::WriteOnly);
                    file.write(objIt->getBody());
                    file.commit();
                }
                    break;
                default:
                    qCDebug(spiApp) << "Image /" <<objIt->getContentSubType() << "not supported by SPI application";
                    return;
                }
            }
                break;
#endif
            case 7:
            {   // SPI content type/subtype values
                switch (objIt->getContentSubType())
                {
                case 0:
                    qCDebug(spiApp) << "\tService Information" << objIt->getContentName();
                    parseBinaryInfo(*objIt);
                    break;                    
                case 1:
                    qCDebug(spiApp) << "\tProgramme Information" << objIt->getContentName();
                    //parseBinaryInfo(*objIt);
                    break;
                case 2:
                    qCDebug(spiApp) << "\tGroup Information" << objIt->getContentName();
                    //parseBinaryInfo(*objIt);
                    break;
                default:
                    // not supported
                    continue;
                }
            }
            break;
            }
        }
    }

    if (incompleteObjCount == 0)
    {
        m_parsedDirectoryIds[decoderId] = decoderPtr->getDirectoryId();
    }
}

void SPIApp::onFileRequest(const QString &url, const QString &requestId)
{
    QString filename = url.mid(url.lastIndexOf('/')+1, url.size());
    for (const auto & decoder : m_decoderMap)
    {
        if (decoder->hasDirectory())
        {
            const auto it = decoder->find(filename);
            if (it != decoder->directoryEnd() && it->isComplete())
            {
                emit requestedFile(it->getBody(), requestId);
                return;
            }
        }
    }

    // not found -> try to download it
    if (QUrl(url).isValid())
    {
        downloadFile(url, requestId);
    }
}

void SPIApp::onNewMOTObject(const MOTObject & obj)
{
    Q_UNUSED(obj)
}

void SPIApp::parseBinaryInfo(const MOTObject &motObj)
{
    QString scopeId("");
    MOTObject::paramsIterator paramIt;
    for (paramIt = motObj.paramsBegin(); paramIt != motObj.paramsEnd(); ++paramIt)
    {
        switch (DabMotExtParameter(paramIt.key()))
        {
        case DabMotExtParameter::ProfileSubset:
            qCDebug(spiApp) << "\tProfileSubset";
            break;
        case DabMotExtParameter::CompressionType:
            qCDebug(spiApp) << "\tCompressionType - not supported";
            return;
            break;
        case DabMotExtParameter::CAInfo:
            // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.5 CAInfo] If this parameter is present a non CA-capable device shall discard this MOT object.
            qCDebug(spiApp) << "\tCAInfo";
            return;
            break;
        default:
            switch (Parameter(paramIt.key()))
            {
            case Parameter::ScopeStart:
                // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.6 ScopeStart] This parameter is used for Programme Information SPI objects only
                qCDebug(spiApp) << "\tScopeStart";
                break;
            case Parameter::ScopeEnd:
                // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.7 ScopeEnd] This parameter is used for Programme Information SPI objects only
                qCDebug(spiApp) << "\tScopeEnd";
                break;
            case Parameter::ScopeID:
                if (paramIt.value().size() >= 3)
                {
                    uint32_t ueid = (uint8_t(paramIt.value().at(0)) << 16) | (uint8_t(paramIt.value().at(1)) << 8) | uint8_t(paramIt.value().at(2));
                    scopeId = QString("%1").arg(ueid, 6, 16, QChar('0'));
                    qCDebug(spiApp, "\tScopeID: %6.6X", ueid);

//                    if (m_radioControl->getEnsembleUEID() != ueid)
//                    {
//                        qDebug("ScopeID: %6.6X is not current ensemble. Service info for current ensemble is only supported!", ueid);
//                        return;
//                    }
                }
                else
                {   // unexpected length
                   qCWarning(spiApp) << "\tScopeID: error";
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
    QDomProcessingInstruction header = m_xmldocument.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
    m_xmldocument.appendChild(header);
    const uint8_t * dataPtr = (uint8_t *) data.data();

    QDomElement empty;
    parseTag(dataPtr, empty, uint8_t(SPIElement::Tag::_invalid), data.size());

    emit xmlDocument(m_xmldocument.toString(), scopeId);
}


uint32_t SPIApp::parseTag(const uint8_t * dataPtr, QDomElement & parentElement, uint8_t parentTag, int maxSize)
{
    if (maxSize < 2)
    {   // not enough data
        return maxSize;
    }

    uint8_t tag = *dataPtr++;
    int len = *dataPtr++;
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
        len = *dataPtr++;
        len = (len << 8) | *dataPtr++;
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
        len = *dataPtr++;
        len = (len << 8) | *dataPtr++;
        len = (len << 8) | *dataPtr++;
        bytesRead += 3;

        if (maxSize < len + 5)
        {   // not enough data
            return maxSize;
        }
    }
    else
    { /* len < 0xFE */ }

    // we know that we have enough data here
    QDomElement element;
    if (tag < 0x80)
    {   // element tags
        switch (SPIElement::Tag(tag))
        {
        case SPIElement::Tag::CDATA:
        {   // ETSI TS 102 371 V3.2.1 [4.5.0]
            if (!parentElement.isNull())
            {
                if (("point" == parentElement.tagName()) || ("polygon" == parentElement.tagName()))
                {   // point or polygon are coded as doubleListType
                    QString str = getDoubleList(dataPtr, len);
                    parentElement.appendChild(m_xmldocument.createTextNode(str));
                }
                else
                {
                    QString str = getString(dataPtr, len, true);
                    parentElement.appendChild(m_xmldocument.createTextNode(str));
                    //parentElement.appendChild(m_xmldocument.createCDATASection(str));
                }
            }
            else { /* error - do nothing here */ }

            bytesRead += len;
        }
            break;
        case SPIElement::Tag::epg:
            element = m_xmldocument.createElement("epg");
            break;
        case SPIElement::Tag::serviceInformation:           
            element = m_xmldocument.createElement("serviceInformation");
            break;
        case SPIElement::Tag::tokenTable:
            m_tokenTable.clear();
            while (len > bytesRead)
            {
                uint8_t tokenId = *dataPtr++;
                uint8_t tokenLen = *dataPtr++;
                QString token = QString::fromUtf8((const char *)dataPtr, tokenLen);
                m_tokenTable.insert(tokenId, token);
                bytesRead += tokenLen+2;
                dataPtr += tokenLen;
            }
            break;
        case SPIElement::Tag::defaultLanguage:
        {
            // ETSI TS 102 371 V3.2.1 [4.11]
            // This element is not defined in the XML specification. This element can only occur within the top-level elements
            // serviceInformation and epg and, if present, it shall occur after the string token table (if present) and before any other child elements.
            // This element applies to all elements within the parent top-level element and all children of the parent element.
            // If the default language element is present, then whenever an xml:lang attribute with the same value as the default language occurs within an element,
            // it does not need to be encoded. Whenever a decoder finds a missing xml:lang attribute for an element, then it shall use the default language value.
            QString defaultLang = getString(dataPtr, len, false);
            parentElement.setAttribute("xml:lang", defaultLang);
            bytesRead += len;
        }
            break;
        case SPIElement::Tag::shortName:
            element = m_xmldocument.createElement("shortName");
            break;
        case SPIElement::Tag::mediumName:
            element = m_xmldocument.createElement("mediumName");
            break;
        case SPIElement::Tag::longName:
            element = m_xmldocument.createElement("longName");
            break;
        case SPIElement::Tag::mediaDescription:
            element = m_xmldocument.createElement("mediaDescription");
            break;
        case SPIElement::Tag::genre:
            element = m_xmldocument.createElement("genre");
            break;
        case SPIElement::Tag::keywords:
            element = m_xmldocument.createElement("keywords");
            break;
        case SPIElement::Tag::memberOf:
            element = m_xmldocument.createElement("memberOf");
            break;
        case SPIElement::Tag::link:
            element = m_xmldocument.createElement("link");
            break;
        case SPIElement::Tag::location:            
            element = m_xmldocument.createElement("location");
            break;
        case SPIElement::Tag::shortDescription:
            element = m_xmldocument.createElement("shortDescription");
            break;
        case SPIElement::Tag::longDescription:
            element = m_xmldocument.createElement("longDescription");
            break;
        case SPIElement::Tag::programme:
            element = m_xmldocument.createElement("programme");
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
            // ETSI TS 102 818 V3.4.1 (2022-05) Annex F: ensemble element has been replaced by the services element
            element = m_xmldocument.createElement("services");
            break;
        case SPIElement::Tag::service:
            element = m_xmldocument.createElement("service");
            break;
        case SPIElement::Tag::multimedia:
            element = m_xmldocument.createElement("multimedia");
            break;
        case SPIElement::Tag::time:
            element = m_xmldocument.createElement("time");
            break;
        case SPIElement::Tag::bearer_serviceID:
        case SPIElement::Tag::bearer:
            element = m_xmldocument.createElement("bearer");
            break;
        case SPIElement::Tag::programmeEvent:
            element = m_xmldocument.createElement("programmeEvent");
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
            dataPtr += len;
            break;
        }

        // parse attributes
        if (!element.tagName().isEmpty())
        {
            while (len > bytesRead)
            {
                uint32_t numBytes = parseTag(dataPtr, element, tag, len);
                bytesRead += numBytes;
                dataPtr += numBytes;
            }

            if (parentElement.isNull())
            {
                m_xmldocument.appendChild(element);
            }
            else
            {
                parentElement.appendChild(element);
            }
        }
    }
    else
    {   // attributes
        switch (SPIElement::Tag(parentTag))
        {
//        case SPIElement::Tag::epg:
//            break;
        case SPIElement::Tag::serviceInformation:
            switch (SPIElement::serviceInformation::attribute(tag))
            {
            case SPIElement::serviceInformation::attribute::version:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "version", dataPtr, len);
                break;
            case SPIElement::serviceInformation::attribute::creationTime:
                setAttribute_timePoint(parentElement, "creationTime", dataPtr, len);
                break;
            case SPIElement::serviceInformation::attribute::originator:
                setAttribute_string(parentElement, "originator", dataPtr, len, false);
                break;
            case SPIElement::serviceInformation::attribute::serviceProvider:
                setAttribute_string(parentElement, "serviceProvider", dataPtr, len, false);
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
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::mediumName:
            switch (SPIElement::mediumName::attribute(tag))
            {
            case SPIElement::mediumName::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::longName:
            switch (SPIElement::longName::attribute(tag))
            {
            case SPIElement::longName::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::shortDescription:
            switch (SPIElement::shortDescription::attribute(tag))
            {
            case SPIElement::shortDescription::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::longDescription:
            switch (SPIElement::longDescription::attribute(tag))
            {
            case SPIElement::longDescription::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
//        case SPIElement::Tag::mediaDescription:
//            break;
        case SPIElement::Tag::genre:
            switch (SPIElement::genre::attribute(tag))
            {
            case SPIElement::genre::attribute::href:
            {   // example: <genre href="urn:tva:metadata:ContentCS:2010:3.6.9">
                uint8_t cs = *dataPtr++ & 0x0F;
                if ((cs < 1) || (cs > 8))
                {   // Undefined. Genres with this CS shall be ignored.
                    break;
                }

                QString href;
                switch (cs)
                {
                case 1:
                    href = "urn:tva:metadata:IntentionCS:2010:1";
                    break;
                case 2:
                    href = "urn:tva:metadata:FormatCS:2010:2";
                    break;
                case 3:
                    href = "urn:tva:metadata:ContentCS:2010:3";
                    break;
                case 4:
                    href = "urn:tva:metadata:IntendedAudienceCS:2010:4";
                    break;
                case 5:
                    href = "urn:tva:metadata:OriginationCS:2010:5";
                    break;
                case 6:
                    href = "urn:tva:metadata:ContentAlertCS:2010:6";
                    break;
                case 7:
                    href = "urn:tva:metadata:MediaTypeCS:2010:7";
                    break;
                case 8:
                    href = "urn:tva:metadata:AtmosphereCS:2010:8";
                    break;
                }

                for (int n = 1; (n < len) && (n < 4); ++n)
                {
                    href += QString(".%1").arg(*dataPtr++);
                }

                parentElement.setAttribute("href", href);
            }
                break;
            case SPIElement::genre::attribute::type:
                switch (*dataPtr)
                {
                case 0x01:
                    parentElement.setAttribute("type", "main");
                    break;
                case 0x02:
                    parentElement.setAttribute("type", "secondary");
                    break;
                case 0x03:
                    parentElement.setAttribute("type", "other");
                    break;
                }
                break;
            }
            break;
        case SPIElement::Tag::keywords:
            switch (SPIElement::keywords::attribute(tag))
            {
            case SPIElement::keywords::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::memberOf:
            switch (SPIElement::memberOf::attribute(tag))
            {
            case SPIElement::memberOf::attribute::id:
                setAttribute_string(parentElement, "id", dataPtr, len, false);
                break;
            case SPIElement::memberOf::attribute::shortId:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.7.2]
                // All attributes defined as shortCRID type are encoded as a 24-bit unsigned integer.
                setAttribute_uint24(parentElement, "shortId", dataPtr, len);
                break;
            case SPIElement::memberOf::attribute::index:
                setAttribute_uint16(parentElement, "index", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::link:
            switch (SPIElement::link::attribute(tag))
            {
            case SPIElement::link::attribute::uri:
                setAttribute_string(parentElement, "uri", dataPtr, len, true);
                break;
            case SPIElement::link::attribute::mimeValue:
                setAttribute_string(parentElement, "mimeValue", dataPtr, len, false);
                break;
            case SPIElement::link::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            case SPIElement::link::attribute::description:
                setAttribute_string(parentElement, "description", dataPtr, len, true);
                break;
            case SPIElement::link::attribute::expiryTime:
                setAttribute_timePoint(parentElement, "expiryTime", dataPtr, len);
                break;
            }
            break;
//        case SPIElement::Tag::location:
//            break;
        case SPIElement::Tag::programme:
        case SPIElement::Tag::programmeEvent:
            switch (SPIElement::programme_programmeEvent::attribute(tag))
            {
            case SPIElement::programme_programmeEvent::attribute::id:
                setAttribute_string(parentElement, "id", dataPtr, len, false);
                break;
            case SPIElement::programme_programmeEvent::attribute::shortId:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.7.2]
                // All attributes defined as shortCRID type are encoded as a 24-bit unsigned integer.
                setAttribute_uint24(parentElement, "shortId", dataPtr, len);
                break;
            case SPIElement::programme_programmeEvent::attribute::version:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "version", dataPtr, len);
                break;
            case SPIElement::programme_programmeEvent::attribute::recommendation:
                switch (*dataPtr)
                {
                case 0x01:
                    parentElement.setAttribute("recommendation", "no");
                    break;
                case 0x02:
                    parentElement.setAttribute("recommendation", "yes");
                    break;
                }
                break;
            case SPIElement::programme_programmeEvent::attribute::broadcast:
                switch (*dataPtr)
                {
                case 0x01:
                    parentElement.setAttribute("broadcast", "on-air");
                    break;
                case 0x02:
                    parentElement.setAttribute("broadcast", "off-air");
                    break;
                }
                break;
            case SPIElement::programme_programmeEvent::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            }
            break;
        case SPIElement::Tag::programmeGroups:
        case SPIElement::Tag::schedule:
            switch (SPIElement::programmeGroups_schedule::attribute(tag))
            {
            case SPIElement::programmeGroups_schedule::attribute::version:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "version", dataPtr, len);
                break;
            case SPIElement::programmeGroups_schedule::attribute::creationTime:
                setAttribute_timePoint(parentElement, "creationTime", dataPtr, len);
                break;
            case SPIElement::programmeGroups_schedule::attribute::originator:
                setAttribute_string(parentElement, "originator", dataPtr, len, true);
                break;
            }
            break;
        case SPIElement::Tag::programmeGroup:
            switch (SPIElement::programmeGroup::attribute(tag))
            {
            case SPIElement::programmeGroup::attribute::id:
                setAttribute_string(parentElement, "id", dataPtr, len, false);
                break;
            case SPIElement::programmeGroup::attribute::shortId:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.7.2]
                // All attributes defined as shortCRID type are encoded as a 24-bit unsigned integer.
                setAttribute_uint24(parentElement, "shortId", dataPtr, len);
                break;
            case SPIElement::programmeGroup::attribute::version:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "version", dataPtr, len);
                break;
            case SPIElement::programmeGroup::attribute::type:
                switch (*dataPtr)
                {
                case 0x02:
                    parentElement.setAttribute("type", "series");
                    break;
                case 0x03:
                    parentElement.setAttribute("type", "show");
                    break;
                case 0x04:
                    parentElement.setAttribute("type", "programConcept");
                    break;
                case 0x05:
                    parentElement.setAttribute("type", "magazine");
                    break;
                case 0x06:
                    parentElement.setAttribute("type", "programCompilation");
                    break;
                case 0x07:
                    parentElement.setAttribute("type", "otherCollection");
                    break;
                case 0x08:
                    parentElement.setAttribute("type", "otherChoice");
                    break;
                case 0x09:
                    parentElement.setAttribute("type", "topic");
                    break;
                }
                break;
            case SPIElement::programmeGroup::attribute::numOfItems:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.4 numOfItems]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "numOfItems", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::scope:
            switch (SPIElement::scope::attribute(tag))
            {
            case SPIElement::scope::attribute::startTime:
                setAttribute_timePoint(parentElement, "startTime", dataPtr, len);
                break;
            case SPIElement::scope::attribute::stopTime:
                setAttribute_timePoint(parentElement, "stopTime", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::serviceScope:
            switch (SPIElement::serviceScope::attribute(tag))
            {
            case SPIElement::serviceScope::attribute::id:
                // When the domain of the id attribute matches the delivery system, it shall be encoded according to clause 4.7.6.
                // ==> bearerURI DAB
                setAttribute_dabBearerURI(parentElement, "id", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::ensemble:
            switch (SPIElement::ensemble::attribute(tag))
            {
            case SPIElement::ensemble::attribute::id:
                uint8_t ecc = *dataPtr++;
                uint16_t eid = *dataPtr++;
                eid = (eid << 8) | *dataPtr++;

                parentElement.setAttribute("id", QString("%1%2").arg(ecc, 2, 16, QChar('0')).arg(eid, 4, 16, QChar('0')));
                break;
            }
            break;
        case SPIElement::Tag::service:
            switch (SPIElement::service::attribute(tag))
            {
            case SPIElement::service::attribute::version:
                // ETSI TS 102 371 V3.2.1 (2016-05) [4.8.3 version]
                // Encoded as a 16-bit unsigned integer.
                setAttribute_uint16(parentElement, "version", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::bearer:
        case SPIElement::Tag::bearer_serviceID:
        {   // ETSI TS 102 371 V3.2.1 (2016-05) [4.15 bearer]
            // Of the attributes of the bearer element, only the id attribute shall be encoded.
            // When the domain of the id attribute matches the delivery system, it shall be encoded according to clause 4.7.6.
            // As a child element of onDemand, when the domain of the id attribute is http:,
            // it shall be encoded according to clause 4.7.6, using the url attribute tag in place of the id attribute tag.
            // NOTE: bearer elements that do not meet the criteria above are not encoded.
            switch (SPIElement::bearer::attribute(tag))
            {
            case SPIElement::bearer::attribute::id:
                // When the domain of the id attribute matches the delivery system, it shall be encoded according to clause 4.7.6.
                // ==> bearerURI DAB
                setAttribute_dabBearerURI(parentElement, "id", dataPtr, len);
                break;
            case SPIElement::bearer::attribute::url:
                setAttribute_string(parentElement, "url", dataPtr, len, false);
                break;
            }

        }
            break;
        case SPIElement::Tag::multimedia:
            switch (SPIElement::multimedia::attribute(tag))
            {
            case SPIElement::multimedia::attribute::mimeValue:
                setAttribute_string(parentElement, "mimeValue", dataPtr, len, false);
                break;
            case SPIElement::multimedia::attribute::xml_lang:
                setAttribute_string(parentElement, "xml:lang", dataPtr, len, false);
                break;
            case SPIElement::multimedia::attribute::url:
                setAttribute_string(parentElement, "url", dataPtr, len, true);
                break;
            case SPIElement::multimedia::attribute::type:
                switch (*dataPtr)
                {
                case 0x02:
                    parentElement.setAttribute("type", "logo_unrestricted");
                    break;
                case 0x04:
                    parentElement.setAttribute("type", "logo_colour_square");
                    break;
                case 0x06:
                    parentElement.setAttribute("type", "logo_colour_rectangle");
                    break;
                }

                break;
            case SPIElement::multimedia::attribute::width:
                setAttribute_uint16(parentElement, "width", dataPtr, len);
                break;
            case SPIElement::multimedia::attribute::height:
                setAttribute_uint16(parentElement, "height", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::time:
        case SPIElement::Tag::relativeTime:
            switch (SPIElement::time_relativeTime::attribute(tag))
            {
            case SPIElement::time_relativeTime::attribute::time:
                setAttribute_timePoint(parentElement, "time", dataPtr, len);
                break;
            case SPIElement::time_relativeTime::attribute::duration:
                setAttribute_duration(parentElement, "duration", dataPtr, len);
                break;
            case SPIElement::time_relativeTime::attribute::actualTime:
                setAttribute_timePoint(parentElement, "actualTime", dataPtr, len);
                break;
            case SPIElement::time_relativeTime::attribute::actualDuration:
                setAttribute_duration(parentElement, "actualDuration", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::radiodns:
            switch (SPIElement::radiodns::attribute(tag))
            {
            case SPIElement::radiodns::attribute::fqdn:
                setAttribute_string(parentElement, "fqdn", dataPtr, len, true);
                break;
            case SPIElement::radiodns::attribute::serviceIdentifier:
                setAttribute_string(parentElement, "serviceIdentifier", dataPtr, len, true);
                break;
            }
            break;
        case SPIElement::Tag::geolocation:
            switch (SPIElement::geolocation::attribute(tag))
            {
            case SPIElement::geolocation::attribute::xml_id:
                setAttribute_string(parentElement, "xml:id", dataPtr, len, true);
                break;
            case SPIElement::geolocation::attribute::ref:
                setAttribute_string(parentElement, "ref", dataPtr, len, true);
                break;
            }
            break;
//        case SPIElement::Tag::country:
//            break;
//        case SPIElement::Tag::point:
//            break;
//        case SPIElement::Tag::polygon:
//            break;
//        case SPIElement::Tag::onDemand:
//            break;
        case SPIElement::Tag::presentationTime:
            switch (SPIElement::presentationTime::attribute(tag))
            {
            case SPIElement::presentationTime::attribute::start:
                setAttribute_timePoint(parentElement, "start", dataPtr, len);
                break;
            case SPIElement::presentationTime::attribute::end:
                setAttribute_timePoint(parentElement, "end", dataPtr, len);
                break;
            case SPIElement::presentationTime::attribute::duration:
                setAttribute_duration(parentElement, "duration", dataPtr, len);
                break;
            }
            break;
        case SPIElement::Tag::acquisitionTime:
            switch (SPIElement::acquisitionTime::attribute(tag))
            {
            case SPIElement::acquisitionTime::attribute::start:
                setAttribute_timePoint(parentElement, "start", dataPtr, len);
                break;
            case SPIElement::acquisitionTime::attribute::end:
                setAttribute_timePoint(parentElement, "end", dataPtr, len);
                break;
            }
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

void SPIApp::setAttribute_string(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len, bool doReplaceTokens)
{
    QString str = getString(dataPtr, len, doReplaceTokens);
    element.setAttribute(name, str);
}

QString SPIApp::getTime(const uint8_t *dataPtr, int len)
{
    if (len < 4)
    {   // not enough data
        qCDebug(spiApp) << "not enough data";

        return QString();
    }
    else { /* enough data */}

    uint32_t dateHoursMinutes = *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;
    dateHoursMinutes = (dateHoursMinutes << 8) + *dataPtr++;

    uint16_t secMsec = 0;
    if (dateHoursMinutes & (1 << 11))
    {   // UTC flag
        secMsec = *dataPtr << 8;
        dataPtr += 2;  // rfu
    }
    else { /* short form */ }

    int8_t lto = 0;
    if (dateHoursMinutes & (1 << 12))
    {   // LTO flag
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

QString SPIApp::getDoubleList(const uint8_t *dataPtr, int len)
{   // ETSI TS 102 371 V3.2.1 (2016-05) [4.7.7]
    // All elements defined as georss:doubleList type are encoded as follows:
    // Each coordinate pair is coded as two 24-bit signed integers: the first coordinate of the pair
    // is the latitude value which is coded as a 24-bit signed integer,
    // representing the value of latitude multiplied by 92 000 (i.e. in 1/92 000 of a degree, ca. 2,4 m);
    // the second coordinate of the pair is the longitude value which is coded as a 24-bit signed integer,
    // representing the value of longitude multiplied by 46 000 (i.e. in 1/46 000 of a degree, ca. 2,4 m).

    QString doubleList;

    while (len >= 6)
    {
        uint32_t var = *dataPtr++;
        var = (var << 8) | *dataPtr++;
        var = (var << 8) | *dataPtr++;
        var = var << 8;   // sign extension
        float lat = var / (92000.0 * 256);

        var = *dataPtr++;
        var = (var << 8) | *dataPtr++;
        var = (var << 8) | *dataPtr++;
        var = var << 8;   // sign extension
        float lon = var / (46000.0 * 256);

        len -= 6;

        doubleList += QString("%1 %2 ").arg(lat).arg(lon);
    }

    return doubleList;
}

void SPIApp::setAttribute_timePoint(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len)
{
    QString str = getTime(dataPtr, len);

    if (!str.isEmpty())
    {
        element.setAttribute(name, str);
    }
    else { /* string is empty => some error happened */ }

}

void SPIApp::setAttribute_uint16(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len)
{
    if (len < 2)
    {   // not enough data
        qCDebug(spiApp) << "not enough data";
        return;
    }
    else { /* OK */ }

    uint16_t val = *dataPtr++;
    val = (val << 8) | *dataPtr++;
    element.setAttribute(name, val);
}

void SPIApp::setAttribute_uint24(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len)
{
    if (len < 3)
    {   // not enough data
        qCDebug(spiApp) << "not enough data";
        return;
    }
    else { /* OK */ }

    uint32_t val = *dataPtr++;
    val = (val << 8) | *dataPtr++;
    val = (val << 8) | *dataPtr++;
    element.setAttribute(name, val);
}

void SPIApp::setAttribute_duration(QDomElement &element, const QString &name, const uint8_t *dataPtr, int len)
{   // ETSI TS 102 371 V3.2.1 (2016-05) [4.7.5 Duration type]
    // All attributes defined as duration type are encoded as a 16-bit unsigned integer,
    // representing the duration in seconds from 0 to 65 535 (just over 18 hours).

    // ETSI TS 102 818 V3.3.1 (2020-08) [5.2.5 duration type]
    // Duration is based on the ISO 8601 [2] format: PTnHnMnS, where "T" represents the date/time separator,
    // "nH" the number of hours, "nM" the number of minutes and "nS" the number of seconds.

    if (len < 2)
    {   // not enough data
        qCDebug(spiApp) << "not enough data";
        return;
    }
    else { /* OK */ }

    uint16_t numSec = *dataPtr++;
    numSec = (numSec << 8) | *dataPtr++;

    QTime time = QTime(0, 0).addSecs(numSec);
    element.setAttribute(name, time.toString(Qt::ISODate));
}

void SPIApp::setAttribute_dabBearerURI(QDomElement &element, const QString &name, const uint8_t *dataPtr, int len)
{
    if (len >= 6)
    {
        uint8_t longSId = *dataPtr & 0x10;

        uint8_t scids = *dataPtr++ & 0x0F;
        uint8_t ecc = *dataPtr++;
        uint16_t eid = *dataPtr++;
        eid = (eid << 8) | *dataPtr++;
        if (longSId)
        {  // long SId
            if (len >= 8)
            {
                uint32_t sid = *dataPtr++;
                sid = (sid << 8) | *dataPtr++;
                sid = (sid << 8) | *dataPtr++;
                sid = (sid << 8) | *dataPtr;
                element.setAttribute(name, QString("dab:%1%2:%3.%4.%5")
                                                     .arg((sid >> 20) & 0x0F, 1, 16)
                                                     .arg(ecc, 2, 16, QChar('0'))
                                                     .arg(eid, 4, 16, QChar('0'))
                                                     .arg(sid, 8, 16, QChar('0'))
                                                     .arg(scids)
                                           );
            }
            else { /* not enough data */ }
        }
        else
        {  // short SId
            uint32_t sid = *dataPtr++;
            sid = (sid << 8) | *dataPtr;
            element.setAttribute(name, QString("dab:%1%2:%3.%4.%5")
                                                 .arg((sid >> 12) & 0x0F, 1, 16)
                                                 .arg(ecc, 2, 16, QChar('0'))
                                                 .arg(eid, 4, 16, QChar('0'))
                                                 .arg(sid, 4, 16, QChar('0'))
                                                 .arg(scids)
                                       );
        }
    }
}

void SPIApp::radioDNSLookup()
{
    if (m_useDoH)
    {   // DNS over http
        QString cnameUrl = QString("https://dns.google/resolve?name=%1&type=CNAME").arg(getRadioDNSFQDN());
        qDebug() << "DNS CNAME query:" << cnameUrl;
        downloadFile(cnameUrl, "DOH_CNAME", false);
    }
    else {
        m_dnsLookup->setType(QDnsLookup::CNAME);
        m_dnsLookup->setName(getRadioDNSFQDN());
        m_dnsLookup->lookup();
    }
}

QString SPIApp::getRadioDNSFQDN() const
{
    return QString("%1.%2.%3.%4.dab.radiodns.org")
        .arg(m_scids)
        .arg(m_sid)
        .arg(m_ueid.last(4))
        .arg(m_gcc);
}

QString SPIApp::getGCC(const DabSId & sid) const
{
    return QString("%1%2").arg(QString("%1").arg(sid.progSId(), 4, 16, QChar('0')).at(0))
        .arg(sid.ecc(), 2, 16, QChar('0'));
}

void SPIApp::handleRadioDNSLookup()
{
    // Check the lookup succeeded.
    if (m_dnsLookup->error() != QDnsLookup::NoError)
    {
        qCWarning(spiApp) << "DNS lookup failed:" << m_dnsLookup->name();
        return;
    }

    // Handle the results.
    if (m_dnsLookup->canonicalNameRecords().count() > 0)
    {
        const auto & record = m_dnsLookup->canonicalNameRecords().at(0);

        qCDebug(spiApp) << "canonicalNameRecord:" << record.name() << record.value();
        m_dnsLookup->setType(QDnsLookup::SRV);
        m_dnsLookup->setName("_radioepg._tcp." + record.value());
        m_dnsLookup->lookup();
    }
    else if (m_dnsLookup->serviceRecords().count() > 0)
    {
        const auto & record = m_dnsLookup->serviceRecords().at(0);
        qCDebug(spiApp) << "serviceRecord:" << record.name() << record.target();

        downloadFile(QString("http://%1/radiodns/spi/3.1/SI.xml").arg(record.target()), "XML");
    }
}

void SPIApp::downloadFile(const QString &url, const QString &requestId, bool useCache)
{
    qCDebug(spiApp) << Q_FUNC_INFO << url;
    if (!m_useInternet)
    {   // do nothing, internet connection is not enabled
        return;
    }

    if (m_downloadReqQueue.isEmpty()) {
        m_downloadReqQueue.enqueue({url, requestId});

        QNetworkRequest request;
        if (useCache)
        {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        }
        request.setUrl(QUrl(url));
        m_netAccessManager->get(request);
    }
    else
    {
        m_downloadReqQueue.enqueue({url, requestId});
    }
}

void SPIApp::onFileDownloaded(QNetworkReply *reply)
{    
    QString requestId = m_downloadReqQueue.head().second;
    if (requestId == "XML") {
        QByteArray data = reply->readAll();
        emit xmlDocument(QString(data), m_ueid);
    }
    else if (requestId == "DOH_CNAME")
    {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        qDebug() << qPrintable(jsonDoc.toJson());

        QVariantMap map = jsonDoc.toVariant().toMap();
        if (map.contains("Answer"))
        {
            QVariantList answer = map["Answer"].toList();
            if (answer.length() > 0)
            {
                QString data = answer.at(0).toMap().value("data", QVariant("")).toString();
                if (!data.isEmpty())
                {
                    if (data.endsWith('.'))
                    {
                        data = data.first(data.length()-1);
                    }
                    QString srvUrl = QString("https://dns.google/resolve?name=_radioepg._tcp.%1&type=SRV").arg(data);
                    qDebug() << srvUrl;
                    downloadFile(srvUrl, "DOH_SRV", false);
                }
            }
        }
    }
    else if (requestId == "DOH_SRV")
    {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        qDebug() << qPrintable(jsonDoc.toJson());

        QVariantMap map = jsonDoc.toVariant().toMap();
        if (map.contains("Answer"))
        {
            QVariantList answer = map["Answer"].toList();
            if (answer.length() > 0)
            {
                QString data = answer.at(0).toMap().value("data", QVariant("")).toString();
                if (!data.isEmpty())
                {
                    static const QRegularExpression re("[\\d\\s]+\\s+(.*\\w)\\.?");
                    QRegularExpressionMatch match = re.match(data);
                    if (match.hasMatch())
                    {
                        downloadFile(QString("http://%1/radiodns/spi/3.1/SI.xml").arg(match.captured(1)), "XML");
                    }
                }
            }
        }

    }
    else
    {   // logo
        emit requestedFile(reply->readAll(), requestId);
    }

    m_downloadReqQueue.dequeue();

    if (!m_downloadReqQueue.isEmpty()) {
        QNetworkRequest request;
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        request.setUrl(QUrl(m_downloadReqQueue.head().first));
        m_netAccessManager->get(request);
    }

    reply->deleteLater();
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
