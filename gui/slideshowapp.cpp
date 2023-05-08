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

#include "dabtables.h"
#include "slideshowapp.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(slideShowApp, "SlideShowApp", QtInfoMsg)

SlideShowApp::SlideShowApp(QObject *parent) : UserApplication(parent)
{
    m_decoder = nullptr;
}

SlideShowApp::~SlideShowApp()
{
    if (nullptr != m_decoder)
    {
        delete m_decoder;
    }
}

void SlideShowApp::start()
{   // does nothing is application is already running
    if (m_isRunning)
    {   // do nothign, application is running
        return;
    }
    else
    { /* not running */ }

    // create new decoder
    m_decoder = new MOTDecoder();
    connect(m_decoder, &MOTDecoder::newMOTObject, this, &SlideShowApp::onNewMOTObject);

    m_isRunning = true;
}

void SlideShowApp::stop()
{
    if (nullptr != m_decoder)
    {
        delete m_decoder;
        m_decoder = nullptr;
    }
    m_isRunning = false;

    // clear cache
    m_cache.clear();

    // clear categories
    m_catSls.clear();

    // ask HMI to clear SLS
    emit resetTerminal();
}

void SlideShowApp::restart()
{
    stop();
    start();
}

void SlideShowApp::onUserAppData(const RadioControlUserAppData & data)
{
    if ((DabUserApplicationType::SlideShow == data.userAppType) && (m_isRunning))
    {
        // application is running and user application type matches
        // data is for this application
        // send data to decoder
        m_decoder->newDataGroup(data.data);
    }
    else
    { /* do nothing */ }
}

void SlideShowApp::onNewMOTObject(const MOTObject & obj)
{
#if (USER_APPLICATION_VERBOSE > 1)
    qCDebug(slideShowApp) << Q_FUNC_INFO << obj.getId() << obj.getContentName();
#endif
    Slide slide;

    // ETSI TS 101 756 V2.4.1 Table 17: Content type and content subtypes
    switch (obj.getContentType())
    {
    // this is not necessary
    case 2:
    {
#if (USER_APPLICATION_VERBOSE > 1)
        switch (obj.getContentSubType())
        {
        case 1:
            qCDebug(slideShowApp) << "Image / JFIF (JPEG)";
            break;
        case 3:
            qCDebug(slideShowApp) << "Image / PNG";
            break;
        default:
            qCDebug(slideShowApp) << "Image /" << obj.getContentSubType() << "not supported by Slideshow application";
            return;
        }
#endif
        slide.setContentName(obj.getContentName());
    }
        break;
    case 5:
        switch (obj.getContentSubType())
        {
        case 0:
#if (USER_APPLICATION_VERBOSE > 0)
            qCDebug(slideShowApp) << "MOT transport / Header update";
#endif
            // ETSI TS 101 499 V3.1.1 [6.2.3]
            // The MOT transport ContentSubType Header update is used to signal an update to parameters
            // of a previously received object.
            break;
        case 1:
            // ETSI TS 101 499 V3.1.1 [6.2.3]
            // The MOT transport ContentSubType Header only is used to signal an object with no associated
            // body data. It should thus only be relevant to IP-connected devices which can acquire
            // the image data over IP.
#if (USER_APPLICATION_VERBOSE > 0)
            qCDebug(slideShowApp) << "MOT transport / Header only";
#endif
            // ignoring this
            return;
            break;
        default:
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "MOT transport /" << obj.getContentSubType() << "not supported by Slideshow application";
#endif
            return;
        }
        break;
    default:
    {
#if (USER_APPLICATION_VERBOSE > 1)
        qCDebug(slideShowApp) << "ContentType" << obj.getContentSubType() << "not supported by Slideshow application";
#endif
        return;
    }
    }

    MOTObject::paramsIterator it = obj.paramsBegin();
    while (it != obj.paramsEnd())
    {
        switch (Parameter(it.key()))
        {
        case Parameter::ExpireTime:
            if (it.value().size() >= 4)
            {
                if (0 == (it.value().at(0) & 0x80))
                {   // ETSI EN 301 234 V2.1.1 [6.2.4.1]
                    // Validity flag = 0: "Now"; MJD and UTC shall be ignored and be set to 0
#if (USER_APPLICATION_VERBOSE > 1)
                    qCDebug(slideShowApp) << "ExpireTime: NOW";
#endif
                }
                else
                {
                    QString paramsDataStr;
                    for (int d = 0; d < it.value().size(); ++d)
                    {
                        paramsDataStr += QString("%1 ").arg((uint8_t) it.value().at(d), 2, 16, QLatin1Char('0'));
                    }
#if (USER_APPLICATION_VERBOSE > 0)
                    qCDebug(slideShowApp) << "ExpireTime:" << paramsDataStr;
#endif
                }
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qCDebug(slideShowApp) << "ExpireTime: error";
#endif
            }
            break;
        case Parameter::TriggerTime:
            if (it.value().size() >= 4)
            {
                if (0 == (it.value().at(0) & 0x80))
                {   // ETSI EN 301 234 V2.1.1 [6.2.4.1]
                    // Validity flag = 0: "Now"; MJD and UTC shall be ignored and be set to 0
#if (USER_APPLICATION_VERBOSE > 1)
                    qCDebug(slideShowApp) << "TriggerTime: NOW";
#endif
                }
                else
                {
                    QString paramsDataStr;
                    for (int d = 0; d < it.value().size(); ++d)
                    {
                        paramsDataStr += QString("%1 ").arg((uint8_t) it.value().at(d), 2, 16, QLatin1Char('0'));
                    }
#if (USER_APPLICATION_VERBOSE > 0)
                    qCDebug(slideShowApp) << "TriggerTime:" << paramsDataStr;
#endif
                }
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qCDebug(slideShowApp) << "TriggerTime: error";
#endif
            }
            break;
        case Parameter::CategoryID:
            // ETSI EN 301 234 V2.1.1 [6.2.1]
            // An MOT decoder evaluating an MOT parameter with fixed DataField length shall not check
            // if the MOT parameter length is equal to what the MOT decoder expects. It shall always check
            // if the length of the DataField is equal or greater than what the MOT decoder expects!
            if (it.value().size() >= 2)
            {
#if (USER_APPLICATION_VERBOSE > 1)
                qCDebug(slideShowApp, "CategoryID: %d/%d", uint8_t(it.value().at(0)), uint8_t(it.value().at(1)));
#endif
                slide.setCategoryID(uint8_t(it.value().at(0)));
                slide.setSlideID(uint8_t(it.value().at(1)));
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qCDebug(slideShowApp) << "CategoryID: error";
#endif
            }
            break;
        case Parameter::CategoryTitle:
            slide.setCategoryTitle(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "CategoryTitle:" << slide.getCategoryTitle();
#endif
            break;
        case Parameter::ClickThroughURL:
            slide.setClickThroughURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "ClickThroughURL:" << slide.getClickThroughURL();
#endif
            break;
        case Parameter::AlternativeLocationURL:
            slide.setAlternativeLocationURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "AlternativeLocationURL:" << slide.getAlternativeLocationURL();
#endif
            break;
        case Parameter::Alert:
#if (USER_APPLICATION_VERBOSE > 0)
            if (it.value().size() >= 1)
            {
                qCDebug(slideShowApp) << "Alert: %d" << uint8_t(it.value().at(0));
            }
            else
            {   // unexpected length
               qCDebug(slideShowApp) << "Alert: error";
            }
#endif
            break;
        default:
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "Parameter"<< it.key() << "not supported by Slideshow application";
#endif
            break;
        }

        ++it;
    }

    // we can try to load data to Slide
    if (!slide.setPixmap(obj.getBody()))
    {   // loading of data failed
        return;
    }
    else
    { /* slide body is correct */ }

    // now we have parsed params -> check for potential request to decategorize
    if (slide.isDecategorizeRequested())
    {   // decategorize
        // An 8-bit number that uniquely identifies a Category. CategoryID shall not be 0x00,
        // except to remove a previously delivered slide from a category.

        // ETSI TS 101 499 V3.1.1 [6.2.2 ContentName]
        // This parameter is used for uniquely identifying the object for the purposes of cache management.

        // first check if item is already in cache
        QHash<QString, Slide>::const_iterator cacheIt = m_cache.constFind(slide.getContentName());
        if (m_cache.cend() != cacheIt)
        {   // item is in the cache -> decategorize and delete from cache
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "Item" << obj.getContentName() << "is already in cache";
#endif
            // remove old slide from category
            removeSlideFromCategory(*cacheIt);

            // delete slide from cache
            m_cache.erase(cacheIt);
        }
        else
        { /* item was is not in cache - do nothing */ }
    }
    else
    {   // ETSI TS 101 499 V3.1.1 [6.2.2 ContentName]
        // This parameter is used for uniquely identifying the object for the purposes of cache management.

        // first check if item is already in cache
        QHash<QString, Slide>::const_iterator cacheIt = m_cache.constFind(slide.getContentName());
        if (m_cache.cend() != cacheIt)
        {   // item is already received
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "Item" << obj.getContentName() << "is already in cache";
#endif
            // ETSI TS 101 499 V3.1.1 [6.3 Updating parameters]
            // The CategoryID/SlideID parameter is used to change the category and/or ordering of a previously delivered slide or to decategorize the slide.
            // The slide being updated shall receive the updated CategoryID/SlideID value. Any other slide with the same CategoryID/SlideID value shall be decategorized.

            // check if categoryID and slideID matchech previously received slide
            if ((cacheIt->getCategoryID() != slide.getCategoryID()) || (cacheIt->getSlideID() != slide.getSlideID()))
            {   // change of category
#if (USER_APPLICATION_VERBOSE > 1)
                qCDebug(slideShowApp) << "Changing slide category";
#endif
                // remove old slide from category
                removeSlideFromCategory(*cacheIt);

                // delete old slide from cache
                m_cache.erase(cacheIt);
            }
            else
            {   // new slide is identical - emit it for SLS
                emit currentSlide(slide);
                return;
            }
        }
        else
        { /* item was is not in cache yet */ }

        // adding new slide to cache
        m_cache.insert(slide.getContentName(), slide);
#if (USER_APPLICATION_VERBOSE > 1)
        qCDebug(slideShowApp) << "Cache contains" << m_cache.size() << "slides";
#endif
        // adding new slide to category
        addSlideToCategory(slide);

#if (USER_APPLICATION_VERBOSE > 1)
//---------------------------------
        // print categories
        qCDebug(slideShowApp) << "Categories:";
        for (QHash<int, Category>::iterator catSlsIt = m_catSls.begin(); catSlsIt != m_catSls.end(); ++catSlsIt)
        {
            qCDebug(slideShowApp) << catSlsIt->getTitle() << "[" << catSlsIt.key() << "]";
            qCDebug(slideShowApp) << "Current slide index is" << catSlsIt->getCurrentIndex();
            QString slideName = catSlsIt->getCurrentSlide();
            for (int n = 0; n < catSlsIt->size(); ++n)
            {
                QHash<QString, Slide>::const_iterator cacheIt = m_cache.constFind(slideName);
                if (m_cache.cend() != cacheIt)
                {   // item is in cache
                    qCDebug(slideShowApp) << QString("   Slide %1/%2: %3 [%4]")
                                .arg(n+1).arg(catSlsIt->size()).arg(slideName).arg(cacheIt->getSlideID());
                }
                else
                {
                    qCDebug(slideShowApp) << "   Slide #" << n << slideName << "in not in cache";
                }
                slideName = catSlsIt->getNextSlide();
            }
        }
//---------------------------------
#endif // (USER_APPLICATION_VERBOSE > 1)
    }

    // emit slide to HMI
    emit currentSlide(slide);
}

void SlideShowApp::addSlideToCategory(const Slide & slide)
{
    QHash<int, Category>::iterator catIt = m_catSls.find(slide.getCategoryID());
    if (m_catSls.end() != catIt)
    {   // category already exists
#if (USER_APPLICATION_VERBOSE > 1)
        qCDebug(slideShowApp) << "Category" << catIt->getTitle() << "exists, adding slide ID" << slide.getSlideID();
#endif
        catIt->insertSlide(slide);

        // ETSI TS 101 499 V3.1.1 [5.3.5.3 Category Title]
        // A CategoryTitle is updated by providing an object with an existing CategoryID and
        // a new value for CategoryTitle.
        if (catIt->getTitle() != slide.getCategoryTitle())
        {   // rename category
            catIt->setTitle(slide.getCategoryTitle());
        }
        else
        { /* do nothing */ }
    }
    else
    {   // category does not exist yet
        QString catName = slide.getCategoryTitle();
        Category newCat = Category(catName);
        newCat.insertSlide(slide);
        m_catSls.insert(slide.getCategoryID(), newCat);
#if (USER_APPLICATION_VERBOSE > 1)
        qCDebug(slideShowApp) << "New category" << catName << "created";
#endif

        if (1 == m_catSls.size())
        {  // this is the first category -> signal that CatSLS is available
            emit catSlsAvailable(true);
        }
    }

    emit categoryUpdate(slide.getCategoryID(), slide.getCategoryTitle());
}

void SlideShowApp::removeSlideFromCategory(const Slide & slide)
{
    // remove slide from current category
    QHash<int, Category>::iterator catSlsIt = m_catSls.find(slide.getCategoryID());
    if (catSlsIt != m_catSls.end())
    {
        catSlsIt->removeSlide(slide.getSlideID());
        if (0 == catSlsIt->size())
        {   // category is empty -> removing category
            m_catSls.erase(catSlsIt);

            emit categoryUpdate(slide.getCategoryID(), QString());

            if (m_catSls.isEmpty())
            {  // there are no more categories -> disable catSls
                emit catSlsAvailable(false);
            }
        }
        else
        { /* there is still something in category */ }
    }
    else
    { /* category not found */ }
}

void SlideShowApp::getCurrentCatSlide(int catId)
{
    QHash<int, SlideShowApp::Category>::const_iterator catSlsIt = m_catSls.constFind(catId);
    if (catSlsIt != m_catSls.cend())
    { // category found
        QHash<QString, Slide>::const_iterator cacheIt = m_cache.constFind(catSlsIt->getCurrentSlide());
        if (m_cache.cend() != cacheIt)
        {   // found in cache
            emit catSlide(cacheIt.value(), catId, catSlsIt->getCurrentIndex(), catSlsIt->size());
        }
        else
        { /* not found in cache - this should not happen */ }
    }
    else
    { // category not found
        emit categoryUpdate(catId, QString());
    }
}

void SlideShowApp::getNextCatSlide(int catId, bool forward)
{
    QHash<int, SlideShowApp::Category>::iterator catSlsIt = m_catSls.find(catId);
    if (catSlsIt != m_catSls.end())
    { // category found
        QHash<QString, Slide>::const_iterator cacheIt = m_cache.constFind(catSlsIt->getNextSlide(forward));
        if (m_cache.cend() != cacheIt)
        {   // found in cache
            emit catSlide(cacheIt.value(), catId, catSlsIt->getCurrentIndex(), catSlsIt->size());
        }
        else
        { /* not found in cache - this should not happen */ }
    }
    else
    { // category not found
        emit categoryUpdate(catId, QString());
    }
}

SlideData::SlideData()
{
    pixmap = QPixmap(0,0);         // this creates NULL pixmap
    contentName = QString("");
    categoryTitle = QString("");
    clickThroughURL = QString("");
    alternativeLocationURL = QString("");
    categoryID = 0;
    slideID = 0;
}

SlideData::SlideData(const SlideData & other) :
    pixmap(other.pixmap),
    contentName(other.contentName),
    categoryTitle(other.categoryTitle),
    clickThroughURL(other.clickThroughURL),
    alternativeLocationURL(other.alternativeLocationURL),
    categoryID(other.categoryID),
    slideID(other.slideID)
{
}

Slide::Slide()
{
    d = new SlideData();
}

QPixmap Slide::getPixmap() const
{
    return d->pixmap;
}

bool Slide::setPixmap(const QByteArray &data)
{
    return d->pixmap.loadFromData(data);
}

const QString &Slide::getContentName() const
{
    return d->contentName;
}

void Slide::setContentName(const QString &newContentName)
{
    d->contentName = newContentName;
}

const QString &Slide::getCategoryTitle() const
{
    return d->categoryTitle;
}

void Slide::setCategoryTitle(const QString &newCategoryTitle)
{
    d->categoryTitle = newCategoryTitle;
}

int Slide::getCategoryID() const
{
    return d->categoryID;
}

const QString &Slide::getClickThroughURL() const
{
    return d->clickThroughURL;
}

void Slide::setClickThroughURL(const QString &newClickThroughURL)
{
    d->clickThroughURL = newClickThroughURL;
}

void Slide::setCategoryID(int newCategoryID)
{
    d->categoryID = newCategoryID;
}

int Slide::getSlideID() const
{
    return d->slideID;
}

void Slide::setSlideID(int newSlideID)
{
    d->slideID = newSlideID;
}

const QString &Slide::getAlternativeLocationURL() const
{
    return d->alternativeLocationURL;
}

void Slide::setAlternativeLocationURL(const QString &newAlternativeLocationURL)
{
    d->alternativeLocationURL = newAlternativeLocationURL;
}

bool Slide::isDecategorizeRequested() const
{
    return ((0 == d->categoryID) && (0 == d->slideID));
}

bool Slide::operator==(const Slide & other) const
{
    return ( (other.d->contentName == d->contentName)
            && (other.d->categoryID == d->categoryID)
            && (other.d->slideID == d->slideID)
            && (other.d->categoryTitle == d->categoryTitle)
           );
}

SlideShowApp::Category::Category(QString & categoryTitle) :
    m_title(categoryTitle)
{
    m_currentSlide = 0;   // this is invalid slide ID
    m_currentSlideIdx = -1;
}

const QString &SlideShowApp::Category::getTitle() const
{
    return m_title;
}

void SlideShowApp::Category::setTitle(const QString &newTitle)
{
    m_title = newTitle;
}

// function returns position of inserted slide
int SlideShowApp::Category::insertSlide(const Slide &s)
{
    SlideItem item = { s.getSlideID(), s.getContentName() };

    for (int idx = 0; idx < m_slidesList.size(); ++idx)
    {
        if (m_slidesList.at(idx).id == item.id)
        {   // replace item
            m_slidesList.replace(idx, item);
            return idx;
        }
        if (m_slidesList.at(idx).id > item.id)
        {   // insert here
            m_slidesList.insert(idx, item);
            if (m_currentSlideIdx >= idx)
            {
                m_currentSlideIdx += 1;
            }
            return idx;
        }
     }

     // if it comes here we need to append the item
    // it could be last or even very first slide
    m_slidesList.append(item);
    if (m_currentSlideIdx < 0)
    {   // set current index
        m_currentSlideIdx = 0;
    }
    return m_slidesList.size()-1;
}

// function returns position of removed slide
int SlideShowApp::Category::removeSlide(int id)
{
    if (0 == m_slidesList.size())
    {   // no items in category -> this shoudl not happen
        return -1;
    }

    // find the slide in category
    for (int idx = 0; idx < m_slidesList.size(); ++idx)
    {
        if (m_slidesList.at(idx).id == id)
        {   // found
            m_slidesList.remove(idx);
            if (m_slidesList.isEmpty())
            {   // empty category
                m_currentSlideIdx = -1;
                return idx;
            }

            // update current index
            if (m_currentSlideIdx == idx)
            {   // removed current -> emit signal
#if (USER_APPLICATION_VERBOSE > 1)
                qCDebug(slideShowApp) << "Current slide removed";
#endif
            }
            if (m_currentSlideIdx > idx)
            {   // decrement current
                if (--m_currentSlideIdx < 0)
                {   // removed current -> setting current to last
                    m_currentSlideIdx = m_slidesList.size()-1;
                }
            }
#if (USER_APPLICATION_VERBOSE > 1)
            qCDebug(slideShowApp) << "Current slide is" << m_currentSlideIdx;
#endif
            return idx;
        }
    }

    // not found
    return -1;
}

QString SlideShowApp::Category::getCurrentSlide() const
{
    if (!m_slidesList.isEmpty())
    {
        return m_slidesList.at(m_currentSlideIdx).contentName;
    }
    else
    { /* empty category */ }

    return QString();
}

QString SlideShowApp::Category::getNextSlide(bool moveForward)
{
    if (m_slidesList.isEmpty())
    {
        return QString();
    }

    if (moveForward)
    {
        if (++m_currentSlideIdx >= m_slidesList.size())
        {
            m_currentSlideIdx = 0;
        }
    }
    else
    {
        if (--m_currentSlideIdx < 0)
        {
            m_currentSlideIdx = m_slidesList.size()-1;
        }
    }

    return m_slidesList.at(m_currentSlideIdx).contentName;
}

int SlideShowApp::Category::size() const
{
    return m_slidesList.size();
}

int SlideShowApp::Category::getCurrentIndex() const
{
    return m_currentSlideIdx;
}
