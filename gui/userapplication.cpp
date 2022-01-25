#include <QDebug>

#include "userapplication.h"
#include "dabtables.h"

UserApplication::UserApplication(RadioControl * radioControlPtr, QObject *parent) :
    QObject(parent), radioControl(radioControlPtr)
{    
    isRunning = false;
}

SlideShowApp::SlideShowApp(RadioControl * radioControlPtr, QObject *parent) : UserApplication(radioControlPtr, parent)
{   
    decoder = nullptr;
    connect(radioControl, &RadioControl::userAppData, this, &SlideShowApp::onUserAppData);
}

SlideShowApp::~SlideShowApp()
{
    if (nullptr != decoder)
    {
        delete decoder;
    }

    // delete all categories
    for (QHash<int, Category*>::const_iterator it = catSls.cbegin(); it != catSls.cend(); ++it)
    {
        delete *it;
    }
}

void SlideShowApp::start()
{   // does nothing is application is alraeady running
    if (isRunning)
    {   // do nothign, application is running
        return;
    }
    else
    { /* not running */ }

    // create new decoder
    decoder = new MOTDecoder();
    connect(decoder, &MOTDecoder::newMOTObject, this, &SlideShowApp::onNewMOTObject);

    isRunning = true;
}

void SlideShowApp::stop()
{
    if (nullptr != decoder)
    {
        delete decoder;
        decoder = nullptr;
    }
    isRunning = false;    

    // clear cache
    cache.clear();

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
    if ((DabUserApplicationType::SlideShow == data.userAppType) && (isRunning))
    {
        // application is running and user application type matches
        // data is for this application
        // send data to decoder
        decoder->newDataGroup(data.data);
    }
    else
    { /* do nothing */ }
}

void SlideShowApp::onNewMOTObject(const MOTObject & obj)
{
    qDebug() << Q_FUNC_INFO << obj.getId() << obj.getContentName();

    Slide slide;

    // ETSI TS 101 756 V2.4.1 Table 17: Content type and content subtypes
    switch (obj.getContentType())
    {
    // this is not necessary
    case 2:
    {
        switch (obj.getContentSubType())
        {
        case 1:
            qDebug() << "Image / JFIF (JPEG)";
            break;
        case 3:
            qDebug() << "Image / PNG";
            break;
        default:
            qDebug() << "Image /" << obj.getContentSubType() << "not supported by Slideshow application";
            return;
        }
        slide.setContentName(obj.getContentName());
    }
        break;
    case 5:
        switch (obj.getContentSubType())
        {
        case 0:
            qDebug() << "MOT transport / Header update";            
            // ETSI TS 101 499 V3.1.1 [6.2.3]
            // The MOT transport ContentSubType Header update is used to signal an update to parameters
            // of a previously received object.
            break;
        case 1:
            // ETSI TS 101 499 V3.1.1 [6.2.3]
            // The MOT transport ContentSubType Header only is used to signal an object with no associated
            // body data. It should thus only be relevant to IP-connected devices which can acquire
            // the image data over IP.
            qDebug() << "MOT transport / Header only";

            // ignoring this
            return;
            break;
        default:
            qDebug() << "MOT transport /" << obj.getContentSubType() << "not supported by Slideshow application";
            return;
        }
        break;
    default:
    {
        qDebug() << "ContentType" << obj.getContentSubType() << "not supported by Slideshow application";
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
                    qDebug() << "ExpireTime: NOW";
                }
                else
                {
                    QString paramsDataStr;
                    for (int d = 0; d < it.value().size(); ++d)
                    {
                        paramsDataStr += QString("%1 ").arg((uint8_t) it.value().at(d), 2, 16, QLatin1Char('0'));
                    }
                    qDebug() << "ExpireTime:" << paramsDataStr;
                }
            }
            else
            {   // unexpected length
               qDebug() << "ExpireTime: error";
            }
            break;
        case Parameter::TriggerTime:
            if (it.value().size() >= 4)
            {
                if (0 == (it.value().at(0) & 0x80))
                {   // ETSI EN 301 234 V2.1.1 [6.2.4.1]
                    // Validity flag = 0: "Now"; MJD and UTC shall be ignored and be set to 0
                    qDebug() << "TriggerTime: NOW";
                }
                else
                {
                    QString paramsDataStr;
                    for (int d = 0; d < it.value().size(); ++d)
                    {
                        paramsDataStr += QString("%1 ").arg((uint8_t) it.value().at(d), 2, 16, QLatin1Char('0'));
                    }
                    qDebug() << "TriggerTime:" << paramsDataStr;
                }
            }
            else
            {   // unexpected length
               qDebug() << "TriggerTime: error";
            }
            break;
        case Parameter::CategoryID:
            // ETSI EN 301 234 V2.1.1 [6.2.1]
            // An MOT decoder evaluating an MOT parameter with fixed DataField length shall not check
            // if the MOT parameter length is equal to what the MOT decoder expects. It shall always check
            // if the length of the DataField is equal or greater than what the MOT decoder expects!
            if (it.value().size() >= 2)
            {
                qDebug("CategoryID: %d/%d", uint8_t(it.value().at(0)), uint8_t(it.value().at(1)));
                slide.setCategoryID(uint8_t(it.value().at(0)));
                slide.setSlideID(uint8_t(it.value().at(1)));
            }
            else
            {   // unexpected length
               qDebug() << "CategoryID: error";
            }
            break;
        case Parameter::CategoryTitle:            
            slide.setCategoryTitle(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
            qDebug() << "CategoryTitle:" << slide.getCategoryTitle();
            break;
        case Parameter::ClickThroughURL:
            slide.setClickThroughURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
            qDebug() << "ClickThroughURL:" << slide.getClickThroughURL();
            break;
        case Parameter::AlternativeLocationURL:
            slide.setAlternativeLocationURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
            qDebug() << "AlternativeLocationURL:" << slide.getAlternativeLocationURL();
            break;
        case Parameter::Alert:
            if (it.value().size() >= 1)
            {
                qDebug() << "Alert: %d" << uint8_t(it.value().at(0));
            }
            else
            {   // unexpected length
               qDebug() << "Alert: error";
            }

            break;
        default:
            qDebug() << "Parameter"<< it.key() << "not supported by Slideshow application";
        }

        ++it;
    }

    // now we have parsed params -> check for potential request to decategorize
    if (slide.isDecategorizeRequested())
    {   // decategorize
        // An 8-bit number that uniquely identifies a Category. CategoryID shall not be 0x00,
        // except to remove a previously delivered slide from a category.

        // ETSI TS 101 499 V3.1.1 [6.2.2 ContentName]
        // This parameter is used for uniquely identifying the object for the purposes of cache management.

        // first check if item is already in cache
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slide.getContentName());
        if (cache.cend() != cacheIt)
        {   // item is in the cache -> decategorize and delete from cache
            qDebug() << "Item" << obj.getContentName() << "is already in cache";

            // remove old slide from category
            removeSlideFromCategory(*cacheIt);

            // delete slide from cache
            cache.erase(cacheIt);
        }
        else
        { /* item was is not in cache - do nothing */ }

        // we can try to load data to Slide
        if (!slide.setPixmap(obj.getBody()))
        {   // loading of data failed
            return;
        }
        else
        { /* slide body is correct */ }
    }
    else
    {   // ETSI TS 101 499 V3.1.1 [6.2.2 ContentName]
        // This parameter is used for uniquely identifying the object for the purposes of cache management.

        // first check if item is already in cache
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slide.getContentName());
        if (cache.cend() != cacheIt)
        {   // item is already received
            qDebug() << "Item" << obj.getContentName() << "is already in cache";

            // ETSI TS 101 499 V3.1.1 [6.3 Updating parameters]
            // The CategoryID/SlideID parameter is used to change the category and/or ordering of a previously delivered slide or to decategorize the slide.
            // The slide being updated shall receive the updated CategoryID/SlideID value. Any other slide with the same CategoryID/SlideID value shall be decategorized.

            // check if categoryID and slideID matchech previously received slide
            if ((cacheIt->getCategoryID() != slide.getCategoryID()) || (cacheIt->getSlideID() != slide.getSlideID()))
            {   // change of category
                qDebug() << "Changing slide category";
                // remove old slide from category
                removeSlideFromCategory(*cacheIt);

                // delete old slide from cache
                cache.erase(cacheIt);
            }
            else
            {   // new slide is identical - do nothing
                return;
            }
        }
        else
        { /* item was is not in cache yet */ }

        // we can try to load data to Slide
        if (!slide.setPixmap(obj.getBody()))
        {   // loading of data failed
            return;
        }
        else
        { /* slide body is correct */ }

        // adding new slide to cache
        cache.insert(slide.getContentName(), slide);      

        qDebug() << "Cache contains" << cache.size() << "slides";

        // adding new slide to category
        addSlideToCategory(slide);

//---------------------------------
        // print categories
        qDebug() << "Categories:";
        for (QHash<int, Category*>::iterator catSlsIt = catSls.begin(); catSlsIt != catSls.end(); ++catSlsIt)
        {
            qDebug() << (*catSlsIt)->getTitle() << "[" << catSlsIt.key() << "]";
            QString slideName = (*catSlsIt)->getFirstSlide();
            for (int n = 0; n < (*catSlsIt)->size(); ++n)
            {
                QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slideName);
                if (cache.cend() != cacheIt)
                {   // item is in cache
                    qDebug() << QString("   Slide %1/%2: %3 [%4]")
                                .arg(n+1).arg((*catSlsIt)->size()).arg(slideName).arg(cacheIt->getSlideID());
                }
                else
                {
                    qDebug() << "   Slide #" << n << slideName << "in not in cache";
                }
                slideName = (*catSlsIt)->getNextSlide();
            }
        }
//---------------------------------

    }

    // emit slide to HMI
    emit newSlide(slide);
}

void SlideShowApp::addSlideToCategory(const Slide & slide)
{
    QHash<int, Category*>::iterator catIt = catSls.find(slide.getCategoryID());
    if (catSls.end() != catIt)
    {   // category already exists
        qDebug() << "Category" << (*catIt)->getTitle() << "exists, adding slide ID" << slide.getSlideID();
        (*catIt)->insertSlide(slide);

        // ETSI TS 101 499 V3.1.1 [5.3.5.3 Category Title]
        // A CategoryTitle is updated by providing an object with an existing CategoryID and
        // a new value for CategoryTitle.
        if ((*catIt)->getTitle() != slide.getCategoryTitle())
        {   // rename category
            (*catIt)->setTitle(slide.getCategoryTitle());

            emit newCategory(catIt.key(), (*catIt)->getTitle());
        }
        else
        { /* do nothing */ }
    }
    else
    {   // category does not exist yet
        QString catName = slide.getCategoryTitle();
        Category * newCat = new  Category(catName);
        newCat->insertSlide(slide);
        catSls.insert(slide.getCategoryID(), newCat);

        qDebug() << "New category" << catName << "created";

        emit newCategory(slide.getCategoryID(), newCat->getTitle());
    }
}

void SlideShowApp::removeSlideFromCategory(const Slide & slide)
{
    // remove slide from current category
    QHash<int, Category*>::iterator catSlsIt = catSls.find(slide.getCategoryID());
    if (catSlsIt != catSls.end())
    {
        (*catSlsIt)->removeSlide(slide.getSlideID());
        if (0 == (*catSlsIt)->size())
        {   // category is empty -> removing category
            emit categoryRemoved(catSlsIt.key());
            delete (*catSlsIt);
            catSls.erase(catSlsIt);
        }
        else
        { /* there is still something in category */ }
    }
    else
    { /* category not found */ }
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
    //qDebug() << Q_FUNC_INFO << contentName;
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
    title(categoryTitle)
{
    currentSlide = 0;   // this is invalid slide ID
    currentSlideIdx = -1;
}

const QString &SlideShowApp::Category::getTitle() const
{
    return title;
}

void SlideShowApp::Category::setTitle(const QString &newTitle)
{
    title = newTitle;
}

// function returns position of inserted slide
int SlideShowApp::Category::insertSlide(const Slide &s)
{
#if 0
    slides.insert(s.getSlideID(), s.getContentName());

    // find the position
    QMap<int, QString>::const_iterator it = slides.constFind(s.getSlideID());
    return std::distance(slides.cbegin(), it) + 1;
#else
    slideItem item = { s.getSlideID(), s.getContentName() };

    for (int idx = 0; idx < slidesList.size(); ++idx)
    {
        if (slidesList.at(idx).id == item.id)
        {   // replace item
            slidesList.replace(idx, item);
            return idx;
        }
        if (slidesList.at(idx).id < item.id)
        {
            slidesList.insert(idx, item);
            if (currentSlideIdx >= idx)
            {
                currentSlideIdx += 1;
            }
            return idx;
        }
     }

     // if it comes here we need to append the item
    slidesList.append(item);
    return slidesList.size()-1;
#endif
}

// function returns position of removed slide
int SlideShowApp::Category::removeSlide(int id)
{
#if 0
    if (0 == slides.size())
    {   // no items in category
        return 0;
    }

    // find index of slide to be removed
    QMap<int,QString>::const_iterator it = slides.constFind(id);
    int idx = std::distance(slides.cbegin(), it);

    if (id == currentSlide)
    {   // current slide is being removed -> need to shift current slide
        it = slides.constFind(currentSlide);
        if (slides.cend() == ++it)
        {   // it was last slide, get first slide
            it = slides.constBegin();
        }
        currentSlide = it.key();
    }
    slides.remove(id);

    if (0 == slides.size())
    {   // category is empty - settign current slide to 0 that is invalid ID
        currentSlide = 0;
    }

    return idx+1;
#else
    if (0 == slidesList.size())
    {   // no items in category -> this shoudl not happen
        return -1;
    }

    // find the slide in category
    for (int idx = 0; idx < slidesList.size(); ++idx)
    {
        if (slidesList.at(idx).id == id)
        {   // found
            slidesList.remove(idx);

            // update current index
            if (currentSlideIdx == idx)
            {   // removed current -> emit signal
                qDebug() << "Current slide removed";
            }
            if (currentSlideIdx > idx)
            {   // decrement current
                if (--currentSlideIdx < 0)
                {   // removed current -> setting current to last
                    currentSlideIdx = slidesList.size()-1;
                }
            }
            qDebug() << "Current slide is" << currentSlideIdx;
            return idx;
        }
    }

    // not found
    return -1;
#endif
}

QString SlideShowApp::Category::getFirstSlide()
{
#if 0
    if (0 != slides.size())
    {
        currentSlide = slides.constBegin().key();
        currentSlideIdx = 1;
        return slides.first();
    }
    else
    { /* empty category */ }

    return QString();
#else
    if (!slidesList.isEmpty())
    {
        currentSlideIdx = 0;
        return slidesList.at(0).contentName;
    }
    else
    { /* empty category */ }

    return QString();
#endif
}

QString SlideShowApp::Category::getNextSlide(bool moveForward)
{
#if 0
    QMap<int,QString>::const_iterator it = slides.constFind(currentSlide);
    if (slides.cend() != it)
    {   // found
        // go to next slide
        if (moveForward)
        {
            if (slides.cend() == ++it)
            {   // it was last slide, return first slide
                it = slides.constBegin();
            }
        }
        else
        {
            if (slides.cbegin() == it)
            {   // current slide is first
                it = slides.constEnd();
            }
            else
            { }
            --it;

        }
        currentSlide = it.key();
        return *it;
    }
    else
    {  // invalid current slide - returning first slide
        it = slides.constBegin();
        if (slides.cend() != it)
        {  // found at lest one
            currentSlide = it.key();
            return *it;
        }
    }

    // this may happen when category is empty
    return QString();
#else
    if (slidesList.isEmpty())
    {
        return QString();
    }

    if (moveForward)
    {
        if (++currentSlideIdx >= slidesList.size())
        {
            currentSlideIdx = 0;
        }
    }
    else
    {
        if (--currentSlideIdx < 0)
        {
            currentSlideIdx = slidesList.size()-1;
        }
    }

    return slidesList.at(currentSlideIdx).contentName;
#endif
}

int SlideShowApp::Category::size() const
{
#if 0
    return slides.size();
#else
    return slidesList.size();
#endif
}

int SlideShowApp::Category::getCurrentIndex() const
{
    return currentSlideIdx;
}
