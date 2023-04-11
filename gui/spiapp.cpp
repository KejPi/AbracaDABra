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
#if 0
        MOTObject::paramsIterator paramIt;
        for (paramIt = objIt->paramsBegin(); paramIt != objIt->paramsEnd(); ++paramIt)
        {
            switch (DabMotExtParameter(paramIt.key()))
            {
            case DabMotExtParameter::ProfileSubset:
                qDebug() << "\tProfileSubset";
                break;
            case DabMotExtParameter::CompressionType:
                qDebug() << "\tCompressionType";
                break;
            case DabMotExtParameter::CAInfo:
                qDebug() << "\tCAInfo";
                // ETSI TS 102 371 V3.2.1 (2016-05) [6.4.0 General] non CA capable devices shall discard encrypted objects
                continue;
                break;
            default:
                switch (Parameter(paramIt.key()))
                {
                case Parameter::ScopeStart:
                    qDebug() << "\tScopeEnd";
                    break;
                case Parameter::ScopeEnd:
                    qDebug() << "\tScopeStart";
                    break;
                case Parameter::ScopeID:
                    qDebug() << "\tScopeID";
                    break;
                default:
                    // not supported
                    break;
                }
                break;
            }
        }
#endif
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
#if 0
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
    //QByteArray::const_iterator dataIt = data.constBegin();
    const uint8_t * dataPtr = (uint8_t *) data.data();
    dataPtr += parseTag(dataPtr, SPI_APP_INVALID_TAG, data.size());
}


uint32_t SPIApp::parseTag(const uint8_t * dataPtr, uint8_t parent, int maxSize)
{
    qDebug("%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X",
            *dataPtr, *(dataPtr+1), *(dataPtr+2), *(dataPtr+3), *(dataPtr+4), *(dataPtr+5), *(dataPtr+6),
            *(dataPtr+7), *(dataPtr+8), *(dataPtr+9));

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
    qDebug("Tag = %2.2X, len = %d", tag, len);

    if (tag < 0x80)
    {   // element tags
        QString tagName("");

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
        }
            break;
        case SPIElement::Tag::epg:
            // not supported
            break;
        case SPIElement::Tag::serviceInformation:
            tagName = "serviceInformation";
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
            tagName = "defaultLanguage";
            break;
        case SPIElement::Tag::shortName:
            tagName = "shortName";
            break;
        case SPIElement::Tag::mediumName:
            tagName = "mediumName";
            break;
        case SPIElement::Tag::longName:
            tagName = "longName";
            break;
        case SPIElement::Tag::mediaDescription:
            tagName = "mediaDescription";
            break;
        case SPIElement::Tag::genre:
            tagName = "genre";
            break;
        case SPIElement::Tag::keywords:
            tagName = "keywords";
            break;
        case SPIElement::Tag::memberOf:
            tagName = "memberOf";
            break;
        case SPIElement::Tag::location:
            tagName = "location";
            break;
        case SPIElement::Tag::shortDescription:
            tagName = "shortDescription";
            break;
        case SPIElement::Tag::longDescription:
            tagName = "longDescription";
            break;
        case SPIElement::Tag::programme:
            tagName = "programme";
            break;
        case SPIElement::Tag::programmeGroups:
            tagName = "programmeGroups";
            break;
        case SPIElement::Tag::schedule:
            tagName = "schedule";
            break;
        case SPIElement::Tag::programmeGroup:
            tagName = "programmeGroup";
            break;
        case SPIElement::Tag::scope:
            tagName = "scope";
            break;
        case SPIElement::Tag::serviceScope:
            tagName = "serviceScope";
            break;
        case SPIElement::Tag::ensemble:
            tagName = "ensemble";
            break;
        case SPIElement::Tag::service:
            tagName = "service";
            break;
        case SPIElement::Tag::bearer_serviceID:
            tagName = "bearer_serviceID";
            break;
        case SPIElement::Tag::multimedia:
            tagName = "multimedia";
            break;
        case SPIElement::Tag::time:
            tagName = "time";
            break;
        case SPIElement::Tag::bearer:
            tagName = "bearer";
            break;
        case SPIElement::Tag::programmeEvent:
            tagName = "programmeEvent";
            break;
        case SPIElement::Tag::relativeTime:
            tagName = "relativeTime";
            break;
        case SPIElement::Tag::radiodns:
            tagName = "radiodns";
            break;
        case SPIElement::Tag::geolocation:
            tagName = "geolocation";
            break;
        case SPIElement::Tag::country:
            tagName = "country";
            break;
        case SPIElement::Tag::point:
            tagName = "point";
            break;
        case SPIElement::Tag::polygon:
            tagName = "polygon";
            break;
        case SPIElement::Tag::onDemand:
            tagName = "onDemand";
            break;
        case SPIElement::Tag::presentationTime:
            tagName = "presentationTime";
            break;
        case SPIElement::Tag::acquisitionTime:
            tagName = "acquisitionTime";
            break;
        default:
            // unknown
            bytesRead += len;
            break;
        }

        // parse attributes
        if (!tagName.isEmpty())
        {
            qDebug("<%s>", tagName.toLatin1().data());
            while (len > bytesRead)
            {
                bytesRead += parseTag(dataPtr+bytesRead, tag, len);
            }
            qDebug("</%s>", tagName.toLatin1().data());
        }


    }
    else
    {   // attributes
        switch (SPIElement::Tag(parent))
        {
        case SPIElement::Tag::epg:
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
            }
                break;
            case SPIElement::serviceInformation::attribute::creationTime:
                qDebug() << "attribute:creationTime";
                break;
            case SPIElement::serviceInformation::attribute::originator:
                qDebug() << "attribute:originator";
                break;
            case SPIElement::serviceInformation::attribute::serviceProvider:
                qDebug() << "attribute:serviceProvider";
                break;
            }
            break;
        case SPIElement::Tag::tokenTable:
            break;
        case SPIElement::Tag::defaultLanguage:
            break;
        case SPIElement::Tag::shortName:
            break;
        case SPIElement::Tag::mediumName:
            break;
        case SPIElement::Tag::longName:
            break;
        case SPIElement::Tag::mediaDescription:
            break;
        case SPIElement::Tag::genre:
            break;
        case SPIElement::Tag::keywords:
            break;
        case SPIElement::Tag::memberOf:
            break;
        case SPIElement::Tag::location:
            break;
        case SPIElement::Tag::shortDescription:
            break;
        case SPIElement::Tag::longDescription:
            break;
        case SPIElement::Tag::programme:
            break;
        case SPIElement::Tag::programmeGroups:
            break;
        case SPIElement::Tag::schedule:
            break;
        case SPIElement::Tag::programmeGroup:
            break;
        case SPIElement::Tag::scope:
            break;
        case SPIElement::Tag::serviceScope:
            break;
        case SPIElement::Tag::ensemble:
            switch (SPIElement::ensemble::attribute(tag))
            {
            case SPIElement::ensemble::attribute::id:
                qDebug() << "attribute:id";
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
                }
                else
                {  // short SId
                    sid = (*(dataPtr+4) << 8) | *(dataPtr+5);
                    qDebug("id=\"dab:c%2.2x:%4.4x.%4.4x.%d\"", ecc,eid, sid, scids);
                }
            }
        }
            break;
        case SPIElement::Tag::multimedia:
            switch (SPIElement::multimedia::attribute(tag))
            {
            case SPIElement::multimedia::attribute::mimeValue:
            {
                QString mime = QString::fromUtf8((const char *)(dataPtr+bytesRead), len);
                qDebug() << "mime =" << mime;
            }
                break;
            case SPIElement::multimedia::attribute::xml_lang:
                break;
            case SPIElement::multimedia::attribute::url:
            {
                QString url = QString::fromUtf8((const char *)(dataPtr+bytesRead), len);
                qDebug() << "url =" << url;
            }
                break;
            case SPIElement::multimedia::attribute::type:
                switch (*(dataPtr+bytesRead))
                {
                case 0x02:
                    qDebug() << "type=\"logo_unrestricted\"";
                    break;
                case 0x04:
                    qDebug() << "type=\"logo_colour_square\"";
                    break;
                case 0x06:
                    qDebug() << "type=\"logo_colour_rectangle\"";
                    break;
                }

                break;
            case SPIElement::multimedia::attribute::width:
                break;
            case SPIElement::multimedia::attribute::height:
                break;
            }
            break;
        case SPIElement::Tag::time:
            break;
        case SPIElement::Tag::bearer:
            break;
        case SPIElement::Tag::programmeEvent:
            break;
        case SPIElement::Tag::relativeTime:
            break;
        case SPIElement::Tag::radiodns:
            break;
        case SPIElement::Tag::geolocation:
            break;
        case SPIElement::Tag::country:
            break;
        case SPIElement::Tag::point:
            break;
        case SPIElement::Tag::polygon:
            break;
        case SPIElement::Tag::onDemand:
            break;
        case SPIElement::Tag::presentationTime:
            break;
        case SPIElement::Tag::acquisitionTime:
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
