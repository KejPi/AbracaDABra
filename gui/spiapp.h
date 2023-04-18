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

#ifndef SPIAPP_H
#define SPIAPP_H

#include <QObject>
#include <QDomDocument>

#include "radiocontrol.h"
#include "motdecoder.h"
#include "userapplication.h"

//#define SPI_APP_INVALID_TAG 0x7F

class SPIDomElement;

class SPIApp : public UserApplication
{
    Q_OBJECT

    enum class Parameter
    {
        ScopeStart = 0x25,
        ScopeEnd = 0x26,
        ScopeID = 0x27,
    };

public:
    SPIApp(QObject *parent = nullptr);
    ~SPIApp();
    void onNewMOTObject(const MOTObject & obj) override;
    void onUserAppData(const RadioControlUserAppData & data) override;
    void onNewMOTDirectory();
    void start() override;
    void stop() override;
    void restart() override;
private:
    MOTDecoder * m_decoder;

    void parseServiceInfo(const MOTObject & motObj);
    uint32_t parseTag(const uint8_t * dataPtr, QDomElement & parentElement, uint8_t parentTag, int maxSize);
    const uint8_t * parseAttributes(const uint8_t * attrPtr, uint8_t tag, int maxSize);
    QString getString(const uint8_t *dataPtr, int len, bool doReplaceTokens = true);
    QString getTime(const uint8_t *dataPtr, int len);
    QString getDoubleList(const uint8_t *dataPtr, int len);

    void setAttribute_string(QDomElement & element, const QString &name, const uint8_t *dataPtr, int len, bool doReplaceTokens);
    void setAttribute_timePoint(QDomElement & element, const QString &name, const uint8_t *dataPtr, int len);
    void setAttribute_uint16(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len);
    void setAttribute_uint24(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len);
    void setAttribute_duration(QDomElement & element, const QString & name, const uint8_t *dataPtr, int len);

    QHash<uint8_t, QString> m_tokenTable;
    QString m_defaultLanguage;

    QDomDocument m_xmldocument;
};

namespace SPIElement
{
    enum class Tag
    {
        CDATA = 0x01,
        epg = 0x02,
        serviceInformation = 0x03,
        tokenTable = 0x04,
        defaultLanguage = 0x06,
        shortName = 0x10,
        mediumName= 0x11,
        longName = 0x12,
        mediaDescription = 0x13,
        genre = 0x14,
        keywords= 0x16,
        memberOf = 0x17,
        link = 0x18,
        location = 0x19,
        shortDescription = 0x1A,
        longDescription = 0x1B,
        programme = 0x1C,
        programmeGroups = 0x20,
        schedule = 0x21,
        programmeGroup = 0x23,
        scope = 0x24,
        serviceScope = 0x25,
        ensemble = 0x26,
        service = 0x28,
        bearer_serviceID = 0x29,
        multimedia = 0x2B,
        time = 0x2C,
        bearer = 0x2D,
        programmeEvent = 0x2E,
        relativeTime = 0x2F,
        radiodns = 0x31,
        geolocation = 0x32,
        country = 0x33,
        point = 0x34,
        polygon = 0x35,
        onDemand = 0x36,
        presentationTime = 0x37,
        acquisitionTime = 0x38,
        _invalid = 0x7F
    };

    namespace serviceInformation
    {
        enum class attribute
        {
            version = 0x80,
            creationTime = 0x81,
            originator = 0x82,
            serviceProvider = 0x83
        };
    }
    namespace ensemble
    {
        enum class attribute
        {
            id = 0x80,
        };
    }
    namespace service
    {
        enum class attribute
        {
            version = 0x80,
        };
    }
    namespace multimedia
    {
        enum class attribute
        {
            mimeValue = 0x80,
            xml_lang = 0x81,
            url = 0x82,
            type = 0x83,
            width = 0x84,
            height = 0x85
        };
    }
    namespace shortName
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace mediumName
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace longName
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace shortDescription
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace longDescription
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace genre
    {
        enum class attribute
        {
            href = 0x80,
            type = 0x81,
        };
    }
    namespace keywords
    {
        enum class attribute
        {
            xml_lang = 0x80,
        };
    }
    namespace link
    {
        enum class attribute
        {
            uri = 0x80,
            mimeValue = 0x81,
            xml_lang = 0x82,
            description = 0x83,
            expiryTime = 0x84,
        };
    }
    namespace memberOf
    {
        enum class attribute
        {
            id = 0x80,
            shortId = 0x81,
            index = 0x82,
        };
    }
    namespace programme_programmeEvent
    {
        enum class attribute
        {
            id = 0x80,
            shortId = 0x81,
            version = 0x82,
            recommendation = 0x83,
            broadcast = 0x84,
            // Not used 0x85
            xml_lang = 0x86,
            // Not used 0x87
        };
    }
    namespace programmeGroups_schedule
    {
        enum class attribute
        {
            version = 0x80,
            creationTime = 0x81,
            originator = 0x82,
        };
    }
    namespace programmeGroup
    {
        enum class attribute
        {
            id = 0x80,
            shortId = 0x81,
            version = 0x82,
            type = 0x83,
            numOfItems = 0x84,
        };
    }
    namespace scope
    {
        enum class attribute
        {
            startTime = 0x80,
            stopTime = 0x81,
        };
    }
    namespace serviceScope
    {
        enum class attribute
        {
            id = 0x80,
        };
    }
    namespace bearer
    {
        enum class attribute
        {
            id = 0x80,
            url = 0x82,
        };
    }
    namespace time_relativeTime
    {
        enum class attribute
        {
            time = 0x80,
            duration = 0x81,
            actualTime = 0x82,
            actualDuration = 0x83,
        };
    }
    namespace radiodns
    {
        enum class attribute
        {
            fqdn = 0x80,
            serviceIdentifier = 0x81,
        };
    }
    namespace geolocation
    {
        enum class attribute
        {
            xml_id = 0x80,
            ref = 0x81,
        };
    }
    namespace presentationTime
    {
        enum class attribute
        {
            start = 0x80,
            end = 0x81,
            duration = 0x82,
        };
    }
    namespace acquisitionTime
    {
        enum class attribute
        {
            start = 0x80,
            end = 0x81,
        };
    }

}

class SPIDomElement : public QDomElement
{
public:
    SPIDomElement();
    SPIDomElement(const QDomElement & e, uint8_t tag);
    QDomElement element() const;
    void setElement(const QDomElement &newElement);
    SPIElement::Tag tag() const;
    void setTag(uint8_t newTag);
private:
    QDomElement m_element;
    SPIElement::Tag m_tag;
};


#endif // SPIAPP_H
