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

#ifndef DLDECODER_H
#define DLDECODER_H

#include <QHash>
#include <QObject>

#define XPAD_DL_NUM_DG_MAX (8)
#define XPAD_DL_LEN_MAX (XPAD_DL_NUM_DG_MAX * 16)

#define DLDECODER_VERBOSE 1

enum class DLPlusContentType
{
    DUMMY = 0,
    ITEM_TITLE = 1,
    ITEM_ALBUM = 2,
    ITEM_TRACKNUMBER = 3,
    ITEM_ARTIST = 4,
    ITEM_COMPOSITION = 5,
    ITEM_MOVEMENT = 6,
    ITEM_CONDUCTOR = 7,
    ITEM_COMPOSER = 8,
    ITEM_BAND = 9,
    ITEM_COMMENT = 10,
    ITEM_GENRE = 11,
    INFO_NEWS = 12,
    INFO_NEWS_LOCAL = 13,
    INFO_STOCKMARKET = 14,
    INFO_SPORT = 15,
    INFO_LOTTERY = 16,
    INFO_HOROSCOPE = 17,
    INFO_DAILY_DIVERSION = 18,
    INFO_HEALTH = 19,
    INFO_EVENT = 20,
    INFO_SCENE = 21,
    INFO_CINEMA = 22,
    INFO_TV = 23,
    // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
    // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
    INFO_DATE_TIME = 24,
    INFO_WEATHER = 25,
    INFO_TRAFFIC = 26,
    INFO_ALARM = 27,
    INFO_ADVERTISEMENT = 28,
    INFO_URL = 29,
    INFO_OTHER = 30,
    STATIONNAME_SHORT = 31,
    STATIONNAME_LONG = 32,
    PROGRAMME_NOW = 33,
    PROGRAMME_NEXT = 34,
    PROGRAMME_PART = 35,
    PROGRAMME_HOST = 36,
    PROGRAMME_EDITORIAL_STAFF = 37,
    // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
    // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
    PROGRAMME_FREQUENCY = 38,
    PROGRAMME_HOMEPAGE = 39,
    // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
    // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
    PROGRAMME_SUBCHANNEL = 40,
    PHONE_HOTLINE = 41,
    PHONE_STUDIO = 42,
    PHONE_OTHER = 43,
    SMS_STUDIO = 44,
    SMS_OTHER = 45,
    EMAIL_HOTLINE = 46,
    EMAIL_STUDIO = 47,
    EMAIL_OTHER = 48,
    MMS_OTHER = 49,
    CHAT = 50,
    CHAT_CENTER = 51,
    VOTE_QUESTION = 52,
    VOTE_CENTRE = 53,
    // rfu = 54
    // rfu = 55
    PRIVATE_1 = 56,
    PRIVATE_2 = 57,
    PRIVATE_3 = 58,
    DESCRIPTOR_PLACE = 59,
    DESCRIPTOR_APPOINTMENT = 60,
    DESCRIPTOR_IDENTIFIER = 61,
    DESCRIPTOR_PURCHASE = 62,
    DESCRIPTOR_GET_DATA = 63,
};
QDebug operator<<(QDebug debug, DLPlusContentType type);

class DLPlusObject
{
public:
    //! @brief Constructors
    DLPlusObject();
    DLPlusObject(const QString &newTag, DLPlusContentType newType);

    DLPlusContentType getType() const;
    void setType(DLPlusContentType newType);

    const QString &getTag() const;
    void setTag(const QString &newTag);

    bool isDelete() const;
    bool isItem() const;
    bool isDummy() const;

    bool operator==(const DLPlusObject &other) const;
    bool operator!=(const DLPlusObject &other) const;

private:
    DLPlusContentType type;
    QString tag;
};

class DLDecoder : public QObject
{
    Q_OBJECT
public:
    explicit DLDecoder(QObject *parent = nullptr);

public slots:
    void newDataGroup(const QByteArray &dataGroup);
    void reset();

signals:
    void resetTerminal();
    void dlItemRunning(bool);
    void dlItemToggle();
    void dlPlusObject(const DLPlusObject &);
    void dlComplete(const QString &);

private:
    uint8_t charset;
    uint8_t segmentCntr;

    int_fast8_t labelToggle;
    int_fast8_t cmdToggle;
    int_fast8_t cmdLink;
    int_fast8_t itemToggle;
    int_fast8_t itemRunnning;

    QByteArray label;
    QByteArray dlCommand;
    QString message;

    bool crc16check(const QByteArray &data);

    bool assembleDL(const QByteArray &dataGroup);
    bool assembleDLPlusCommand(const QByteArray &dataGroup);
    void parseDLPlusCommand();
};

#endif  // DLDECODER_H
