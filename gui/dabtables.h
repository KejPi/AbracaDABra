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

#ifndef DABTABLES_H
#define DABTABLES_H

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QStringList>

enum class DabProtectionLevel
{
    PROTECTION_LEVEL_UNDEFINED = 0,
    UEP_1 = 1,
    UEP_2 = 2,
    UEP_3 = 3,
    UEP_4 = 4,
    UEP_5 = 5,
    UEP_MAX = UEP_5,

    EEP_MIN = (UEP_MAX + 1),
    EEP_1A = (0 + EEP_MIN),
    EEP_2A = (1 + EEP_MIN),
    EEP_3A = (2 + EEP_MIN),
    EEP_4A = (3 + EEP_MIN),

    EEP_1B = (4 + EEP_MIN),
    EEP_2B = (5 + EEP_MIN),
    EEP_3B = (6 + EEP_MIN),
    EEP_4B = (7 + EEP_MIN),
};

enum class DabTMId
{
    StreamAudio = 0,
    StreamData = 1,
    PacketData = 3
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
    EBULATIN = 0x0,  // [TSI TS 101 756 V2.4.1 Table 1 & 19]
    LATIN1 = 0x4,    // [TSI TS 101 756 V2.4.1 Table 19]
    UCS2 = 0x6,      // [TSI TS 101 756 V2.4.1 Table 1]
    UTF8 = 0xF,      // [TSI TS 101 756 V2.4.1 Table 1 & 19]
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

enum class DabAnnouncement
{
    Alarm = 0,
    Traffic = 1,
    Transport,
    Warning,
    News,
    Weather,
    Event,
    Special,
    Programme,
    Sport,
    Financial,
    AlarmTest,
    Undefined
};

typedef QMap<uint32_t, QString> dabChannelList_t;

class DabTables
{
public:
    // DabTables();
    enum
    {
        NUM_PTY = 32
    };
    static const dabChannelList_t channelList;
    static const uint16_t ebuLatin2UCS2[];
    static const QList<uint16_t> ASwValues;
    // static const QList<uint8_t> TIIPattern;
    static const uint8_t TIIPattern[][4];

    static QString convertToQString(const char *c, uint8_t charset, uint8_t len = 16);
    static QDateTime dabTimeToUTC(uint32_t dateHoursMinutes, uint16_t secMsec);
    static QString getPtyName(const uint8_t pty);
    static QString getPtyNameEnglish(const uint8_t pty);
    static QString getLangName(int lang);
    static QString getLangNameEnglish(int lang);
    static QString getCountryName(uint32_t SId);
    static QString getCountryNameEnglish(uint32_t SId);
    static QString getAnnouncementName(DabAnnouncement announcement);
    static QString getAnnouncementNameEnglish(DabAnnouncement announcement);
    static QString getUserApplicationName(DabUserApplicationType type);
    static QList<int> getTiiSubcarriers(int mainId, int subId);
};

#endif  // DABTABLES_H
