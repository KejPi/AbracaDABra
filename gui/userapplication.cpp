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
    }
    isRunning = false;    
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

        // we can try to load data to Slide
        if (!slide.setPixmap(obj.getBody()))
        {   // loading of data failed
            return;
        }
        else
        { /* slide body is correct -> lets continue with params processing */ }

        slide.setContentName(obj.getContentName());
    }
        break;
    case 5:
        switch (obj.getContentSubType())
        {
        case 0:
            qDebug() << "MOT transport / Header update";
            break;
        case 1:
            qDebug() << "MOT transport / Header only";
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

    // emit slide to HMI
    emit newSlide(slide);
}

Slide::Slide()
{

}

QPixmap Slide::getPixmap() const
{
    return pixmap;
}

bool Slide::setPixmap(const QByteArray &data)
{
    return pixmap.loadFromData(data);
}

const QString &Slide::getContentName() const
{
    return contentName;
}

void Slide::setContentName(const QString &newContentName)
{
    contentName = newContentName;
}

const QString &Slide::getCategoryTitle() const
{
    return categoryTitle;
}

void Slide::setCategoryTitle(const QString &newCategoryTitle)
{
    categoryTitle = newCategoryTitle;
}

int Slide::getCategoryID() const
{
    return categoryID;
}

const QString &Slide::getClickThroughURL() const
{
    return clickThroughURL;
}

void Slide::setClickThroughURL(const QString &newClickThroughURL)
{
    clickThroughURL = newClickThroughURL;
}

void Slide::setCategoryID(int newCategoryID)
{
    categoryID = newCategoryID;
}

int Slide::getSlideID() const
{
    return slideID;
}

void Slide::setSlideID(int newSlideID)
{
    slideID = newSlideID;
}

const QString &Slide::getAlternativeLocationURL() const
{
    return alternativeLocationURL;
}

void Slide::setAlternativeLocationURL(const QString &newAlternativeLocationURL)
{
    alternativeLocationURL = newAlternativeLocationURL;
}
