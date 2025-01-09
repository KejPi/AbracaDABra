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

#ifndef SLIDESHOWAPP_H
#define SLIDESHOWAPP_H

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSharedData>

#include "motdecoder.h"
#include "radiocontrol.h"
#include "userapplication.h"

class SlideData : public QSharedData
{
public:
    SlideData();
    SlideData(const SlideData &other);
    ~SlideData() {}

    QPixmap pixmap;
    QByteArray rawData;
    QString contentName;
    QString categoryTitle;
    QString clickThroughURL;
    QString alternativeLocationURL;
    QString format;
    int transportID;
    int categoryID;
    int slideID;
    int numBytes;
};

class Slide
{
public:
    Slide();
    Slide(const Slide &other) : d(other.d) {}

    QPixmap getPixmap() const;
    bool setPixmap(const QByteArray &data);
    bool setPixmap(const QPixmap &pixmap);

    const QString &getContentName() const;
    void setContentName(const QString &newContentName);

    const QString &getCategoryTitle() const;
    void setCategoryTitle(const QString &newCategoryTitle);

    const QString &getClickThroughURL() const;
    void setClickThroughURL(const QString &newClickThroughURL);

    const QString &getFormat() const;
    void setFormat(const QString &newFormat);

    int getCategoryID() const;
    void setCategoryID(int newCategoryID);

    int getSlideID() const;
    void setSlideID(int newSlideID);

    int getTransportID() const;
    void setTransportID(int newTransportID);

    const QByteArray &getRawData() const { return d->rawData; }
    int getNumBytes() const { return d->numBytes; }

    const QString &getAlternativeLocationURL() const;
    void setAlternativeLocationURL(const QString &newAlternativeLocationURL);

    bool isDecategorizeRequested() const;
    bool isEmpty() const { return d->contentName.isEmpty(); }

    bool operator==(const Slide &other) const;

private:
    QSharedDataPointer<SlideData> d;
};

class SlideShowApp : public UserApplication
{
    Q_OBJECT
    enum class Parameter
    {
        ExpireTime = 0x04,
        TriggerTime = 0x05,
        ContentName = 0x0C,
        CategoryID = 0x25,
        CategoryTitle = 0x26,
        ClickThroughURL = 0x27,
        AlternativeLocationURL = 0x28,
        Alert = 0x29
    };
    class Category;

public:
    SlideShowApp(QObject *parent = nullptr);
    ~SlideShowApp();
    void onNewMOTObject(const MOTObject &obj) override;
    void onUserAppData(const RadioControlUserAppData &data) override;
    void start() override;
    void stop() override;
    void restart() override;
    void setDataDumping(const Settings::UADumpSettings &settings) override;

    void getCurrentCatSlide(int catId);
    void getNextCatSlide(int catId, bool forward = true);
signals:
    // this signal is emitted anytime when new slide is received
    void currentSlide(const Slide &slide);

    // catSLS signals
    void categoryUpdate(int catId, const QString &title);
    void catSlide(const Slide &slide, int catId, int slideIdx, int numSlides);

    void catSlsAvailable(bool isAvailable);

private:
    MOTDecoder *m_decoder;
    QHash<QString, Slide> m_cache;
    QHash<int, Category> m_catSls;

    void addSlideToCategory(const Slide &slide);
    void removeSlideFromCategory(const Slide &slide);
    void dumpSlide(const Slide &slide);
};

class SlideShowApp::Category
{
public:
    Category(QString &categoryTitle);
    const QString &getTitle() const;
    void setTitle(const QString &newTitle);
    int insertSlide(const Slide &s);
    int removeSlide(int id);
    int size() const;
    QString getCurrentSlide() const;
    QString getNextSlide(bool moveForward = true);
    int getCurrentIndex() const;

private:
    struct SlideItem
    {
        int id;
        QString contentName;
    };

    int m_currentSlide;
    int m_currentSlideIdx;
    QString m_title;
    QList<SlideItem> m_slidesList;
};

#endif  // SLIDESHOWAPP_H
