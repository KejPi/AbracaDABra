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

enum class DabAudioMode
{
    DAB_AUDIO = 0x00,
    DABPLUS_AUDIO = 0x3F,
    AUDIO_UNDEF = 0xFF
};
Q_DECLARE_METATYPE(DabAudioMode);

enum class DabCharset
{
    EBULATIN = 0,
    UCS2 = 6,
    UTF8 = 15,
};

typedef QMap<uint32_t, QString> dabChannelList_t;

class DabTables
{
public:
    //DabTables();
    static const dabChannelList_t channelList;
    static const QStringList PTyNames;
    static const uint16_t ebuLatin2UCS2[];
    static QString convertToQString(const char *c, uint8_t charset, uint8_t len = 0);
    static QString getPtyName(const uint8_t pty);
    static QString getLangName(int lang);
    static QString getCountryName(uint32_t SId);
};

#endif // DABTABLES_H
