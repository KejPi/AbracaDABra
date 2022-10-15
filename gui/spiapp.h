#ifndef SPIAPP_H
#define SPIAPP_H

#include <QObject>
#include "dabtables.h"
#include "radiocontrol.h"
#include "motdecoder.h"
#include "userapplication.h"

#define SPI_APP_INVALID_TAG 0x7F

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
    SPIApp(RadioControl * radioControlPtr, QObject *parent = nullptr);
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
    uint32_t parseTag(const uint8_t * dataPtr, uint8_t parent, int maxSize);
    const uint8_t * parseAttributes(const uint8_t * attrPtr, uint8_t tag, int maxSize);

    QHash<uint8_t, QString> m_tokenTable;
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
        memberOf = 0x18,
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
        acquisitionTime = 0x38
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
}


#endif // SPIAPP_H
