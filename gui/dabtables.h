#ifndef DABTABLES_H
#define DABTABLES_H

#include <QObject>
#include <QStringList>
#include <QMap>

enum class DabProtectionLevel
{
    PROTECTION_LEVEL_UNDEFINED = 0,
    UEP_1 = 1,
    UEP_2 = 2,
    UEP_3 = 3,
    UEP_4 = 4,
    UEP_5 = 5,
    UEP_MAX = UEP_5,

    EEP_MIN = (UEP_MAX+1),
    EEP_1A =  (0+EEP_MIN),
    EEP_2A =  (1+EEP_MIN),
    EEP_3A =  (2+EEP_MIN),
    EEP_4A =  (3+EEP_MIN),

    EEP_1B =  (4+EEP_MIN),
    EEP_2B =  (5+EEP_MIN),
    EEP_3B =  (6+EEP_MIN),
    EEP_4B =  (7+EEP_MIN),
};

enum class DabTMId
{
    StreamAudio = 0,
    StreamData  = 1,
    PacketData  = 3
};

// ETSI TS 101 756 V2.4.1 table 2a and 2b
enum class DabAudioDataSCty
{
    DAB_AUDIO = 0,
    TDC = 5,
    MPEG2TS = 24,
    MOT = 60,
    PROPRIETARY_SERVICE = 61,
    DABPLUS_AUDIO = 63,
    ADSCTY_UNDEF = 0xFF
};

// ETSI TS 101 756 V2.4.1 table 16
enum class DabUserApplicationType
{
    SlideShow = 0x2,
    TPEG = 0x4,
    SPI = 0x7,
    DMB = 0x9,
    Filecasting = 0xD,
    FIS = 0xE,
    Journaline = 0x44A
};

enum class DabCharset
{
    EBULATIN = 0x0,       // [TSI TS 101 756 V2.4.1 Table 1 & 19]
    LATIN1 = 0x4,         // [TSI TS 101 756 V2.4.1 Table 19]
    UCS2 = 0x6,           // [TSI TS 101 756 V2.4.1 Table 1]
    UTF8 = 0xF,           // [TSI TS 101 756 V2.4.1 Table 1 & 19]
};

enum class DabMotExtParameter
{
    PermitOutdatedVersions = 0x01,
    TriggerTime = 0x05,
    RetransmissionDistance = 0x07,
    Expiration = 0x09,
    Priority = 0x0A,
    Label = 0x0B,
    ContentName = 0x0C,
    UniqueBodyVersion = 0x0D,
    MimeType = 0x10,
    CompressionType = 0x11,
    AdditionalHeader = 0x20,
    ProfileSubset = 0x21,
    CAInfo = 0x23,
    CAReplacementObject = 0x24
};

enum class DabAnnoucement
{
    Alarm = 0,
    Trafic = 1,
    Transport,
    Warning,
    News,
    Weather,
    Event,
    Special,
    Programme,
    Sport,
    Financial,
};

typedef QMap<uint32_t, QString> dabChannelList_t;

class DabTables
{
public:
    //DabTables();
    static const dabChannelList_t channelList;
    static const QStringList PTyNames;
    static const uint16_t ebuLatin2UCS2[];
    static const QList<uint16_t> ASwValues;
    static QString convertToQString(const char *c, uint8_t charset, uint8_t len = 16);
    static QString getPtyName(const uint8_t pty);
    static QString getLangName(int lang);
    static QString getCountryName(uint32_t SId);
    static QString getAnnouncementName(uint8_t bit);
};

#endif // DABTABLES_H
