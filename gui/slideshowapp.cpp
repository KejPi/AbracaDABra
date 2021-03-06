#include "dabtables.h"
#include "slideshowapp.h"

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

    // clear categories
    catSls.clear();

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
#if (USER_APPLICATION_VERBOSE > 1)
    qDebug() << Q_FUNC_INFO << obj.getId() << obj.getContentName();
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
            qDebug() << "Image / JFIF (JPEG)";
            break;
        case 3:
            qDebug() << "Image / PNG";
            break;
        default:
            qDebug() << "Image /" << obj.getContentSubType() << "not supported by Slideshow application";
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
            qDebug() << "MOT transport / Header update";
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
            qDebug() << "MOT transport / Header only";
#endif
            // ignoring this
            return;
            break;
        default:
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "MOT transport /" << obj.getContentSubType() << "not supported by Slideshow application";
#endif
            return;
        }
        break;
    default:
    {
#if (USER_APPLICATION_VERBOSE > 1)
        qDebug() << "ContentType" << obj.getContentSubType() << "not supported by Slideshow application";
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
                    qDebug() << "ExpireTime: NOW";
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
                    qDebug() << "ExpireTime:" << paramsDataStr;
#endif
                }
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qDebug() << "ExpireTime: error";
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
                    qDebug() << "TriggerTime: NOW";
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
                    qDebug() << "TriggerTime:" << paramsDataStr;
#endif
                }
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qDebug() << "TriggerTime: error";
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
                qDebug("CategoryID: %d/%d", uint8_t(it.value().at(0)), uint8_t(it.value().at(1)));
#endif
                slide.setCategoryID(uint8_t(it.value().at(0)));
                slide.setSlideID(uint8_t(it.value().at(1)));
            }
            else
            {   // unexpected length
#if (USER_APPLICATION_VERBOSE > 1)
               qDebug() << "CategoryID: error";
#endif
            }
            break;
        case Parameter::CategoryTitle:
            slide.setCategoryTitle(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "CategoryTitle:" << slide.getCategoryTitle();
#endif
            break;
        case Parameter::ClickThroughURL:
            slide.setClickThroughURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "ClickThroughURL:" << slide.getClickThroughURL();
#endif
            break;
        case Parameter::AlternativeLocationURL:
            slide.setAlternativeLocationURL(DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size()));
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "AlternativeLocationURL:" << slide.getAlternativeLocationURL();
#endif
            break;
        case Parameter::Alert:
#if (USER_APPLICATION_VERBOSE > 0)
            if (it.value().size() >= 1)
            {
                qDebug() << "Alert: %d" << uint8_t(it.value().at(0));
            }
            else
            {   // unexpected length
               qDebug() << "Alert: error";
            }
#endif
            break;
        default:
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "Parameter"<< it.key() << "not supported by Slideshow application";
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
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slide.getContentName());
        if (cache.cend() != cacheIt)
        {   // item is in the cache -> decategorize and delete from cache
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "Item" << obj.getContentName() << "is already in cache";
#endif
            // remove old slide from category
            removeSlideFromCategory(*cacheIt);

            // delete slide from cache
            cache.erase(cacheIt);
        }
        else
        { /* item was is not in cache - do nothing */ }
    }
    else
    {   // ETSI TS 101 499 V3.1.1 [6.2.2 ContentName]
        // This parameter is used for uniquely identifying the object for the purposes of cache management.

        // first check if item is already in cache
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slide.getContentName());
        if (cache.cend() != cacheIt)
        {   // item is already received
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "Item" << obj.getContentName() << "is already in cache";
#endif
            // ETSI TS 101 499 V3.1.1 [6.3 Updating parameters]
            // The CategoryID/SlideID parameter is used to change the category and/or ordering of a previously delivered slide or to decategorize the slide.
            // The slide being updated shall receive the updated CategoryID/SlideID value. Any other slide with the same CategoryID/SlideID value shall be decategorized.

            // check if categoryID and slideID matchech previously received slide
            if ((cacheIt->getCategoryID() != slide.getCategoryID()) || (cacheIt->getSlideID() != slide.getSlideID()))
            {   // change of category
#if (USER_APPLICATION_VERBOSE > 1)
                qDebug() << "Changing slide category";
#endif
                // remove old slide from category
                removeSlideFromCategory(*cacheIt);

                // delete old slide from cache
                cache.erase(cacheIt);
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
        cache.insert(slide.getContentName(), slide);
#if (USER_APPLICATION_VERBOSE > 1)
        qDebug() << "Cache contains" << cache.size() << "slides";
#endif
        // adding new slide to category
        addSlideToCategory(slide);

#if (USER_APPLICATION_VERBOSE > 1)
//---------------------------------
        // print categories
        qDebug() << "Categories:";
        for (QHash<int, Category>::iterator catSlsIt = catSls.begin(); catSlsIt != catSls.end(); ++catSlsIt)
        {
            qDebug() << catSlsIt->getTitle() << "[" << catSlsIt.key() << "]";
            qDebug() << "Current slide index is" << catSlsIt->getCurrentIndex();
            QString slideName = catSlsIt->getCurrentSlide();
            for (int n = 0; n < catSlsIt->size(); ++n)
            {
                QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(slideName);
                if (cache.cend() != cacheIt)
                {   // item is in cache
                    qDebug() << QString("   Slide %1/%2: %3 [%4]")
                                .arg(n+1).arg(catSlsIt->size()).arg(slideName).arg(cacheIt->getSlideID());
                }
                else
                {
                    qDebug() << "   Slide #" << n << slideName << "in not in cache";
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
    QHash<int, Category>::iterator catIt = catSls.find(slide.getCategoryID());
    if (catSls.end() != catIt)
    {   // category already exists
#if (USER_APPLICATION_VERBOSE > 1)
        qDebug() << "Category" << catIt->getTitle() << "exists, adding slide ID" << slide.getSlideID();
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
        catSls.insert(slide.getCategoryID(), newCat);
#if (USER_APPLICATION_VERBOSE > 1)
        qDebug() << "New category" << catName << "created";
#endif

        if (1 == catSls.size())
        {  // this is the first category -> signal that CatSLS is available
            emit catSlsAvailable(true);
        }
    }

    emit categoryUpdate(slide.getCategoryID(), slide.getCategoryTitle());
}

void SlideShowApp::removeSlideFromCategory(const Slide & slide)
{
    // remove slide from current category
    QHash<int, Category>::iterator catSlsIt = catSls.find(slide.getCategoryID());
    if (catSlsIt != catSls.end())
    {
        catSlsIt->removeSlide(slide.getSlideID());
        if (0 == catSlsIt->size())
        {   // category is empty -> removing category
            catSls.erase(catSlsIt);

            emit categoryUpdate(slide.getCategoryID(), QString());

            if (catSls.isEmpty())
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
    QHash<int, SlideShowApp::Category>::const_iterator catSlsIt = catSls.constFind(catId);
    if (catSlsIt != catSls.cend())
    { // category found
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(catSlsIt->getCurrentSlide());
        if (cache.cend() != cacheIt)
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
    QHash<int, SlideShowApp::Category>::iterator catSlsIt = catSls.find(catId);
    if (catSlsIt != catSls.end())
    { // category found
        QHash<QString, Slide>::const_iterator cacheIt = cache.constFind(catSlsIt->getNextSlide(forward));
        if (cache.cend() != cacheIt)
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
    slideItem item = { s.getSlideID(), s.getContentName() };

    for (int idx = 0; idx < slidesList.size(); ++idx)
    {
        if (slidesList.at(idx).id == item.id)
        {   // replace item
            slidesList.replace(idx, item);
            return idx;
        }
        if (slidesList.at(idx).id > item.id)
        {   // insert here
            slidesList.insert(idx, item);
            if (currentSlideIdx >= idx)
            {
                currentSlideIdx += 1;
            }
            return idx;
        }
     }

     // if it comes here we need to append the item
    // it could be last or even very first slide
    slidesList.append(item);
    if (currentSlideIdx < 0)
    {   // set current index
        currentSlideIdx = 0;
    }
    return slidesList.size()-1;
}

// function returns position of removed slide
int SlideShowApp::Category::removeSlide(int id)
{
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
            if (slidesList.isEmpty())
            {   // empty category
                currentSlideIdx = -1;
                return idx;
            }

            // update current index
            if (currentSlideIdx == idx)
            {   // removed current -> emit signal
#if (USER_APPLICATION_VERBOSE > 1)
                qDebug() << "Current slide removed";
#endif
            }
            if (currentSlideIdx > idx)
            {   // decrement current
                if (--currentSlideIdx < 0)
                {   // removed current -> setting current to last
                    currentSlideIdx = slidesList.size()-1;
                }
            }
#if (USER_APPLICATION_VERBOSE > 1)
            qDebug() << "Current slide is" << currentSlideIdx;
#endif
            return idx;
        }
    }

    // not found
    return -1;
}

QString SlideShowApp::Category::getCurrentSlide() const
{
    if (!slidesList.isEmpty())
    {
        return slidesList.at(currentSlideIdx).contentName;
    }
    else
    { /* empty category */ }

    return QString();
}

QString SlideShowApp::Category::getNextSlide(bool moveForward)
{
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
}

int SlideShowApp::Category::size() const
{
    return slidesList.size();
}

int SlideShowApp::Category::getCurrentIndex() const
{
    return currentSlideIdx;
}
