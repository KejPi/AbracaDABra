#include <QDebug>

#include "userapplication.h"
#include "dabtables.h"

UserApplication::UserApplication(QObject *parent) : QObject(parent)
{

}


SlideShowApp::SlideShowApp(QObject *parent) : UserApplication(parent)
{

}

void SlideShowApp::onNewMOTObject(const MOTObject & obj)
{
    qDebug() << Q_FUNC_INFO << obj.getId() << obj.headerParams.contentName;

    // ETSI TS 101 756 V2.4.1 Table 17: Content type and content subtypes
    switch (obj.headerParams.contentType)
    {
    case 2:
    {
        switch (obj.headerParams.contentSubType)
        {
        case 1:
            qDebug() << "Image / JFIF (JPEG)";
            break;
        case 3:
            qDebug() << "Image / PNG";
            break;
        default:
            qDebug() << "Image /" << obj.headerParams.contentSubType << "not supported by Slideshow application";
        }
    }
        break;
    case 5:
        switch (obj.headerParams.contentSubType)
        {
        case 0:
            qDebug() << "MOT transport / Header update";
            break;
        case 1:
            qDebug() << "MOT transport / Header only";
            break;
        default:
            qDebug() << "MOT transport /" << obj.headerParams.contentSubType << "not supported by Slideshow application";
        }
        break;
    default:
    {
        qDebug() << "ContentType" << obj.headerParams.contentType << "not supported by Slideshow application";
    }
    }

   QHash<int, QByteArray>::const_iterator it = obj.userAppParams.constBegin();
    while (it != obj.userAppParams.constEnd())
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
            }
            else
            {   // unexpected length
               qDebug() << "CategoryID: error";
            }
            break;
        case Parameter::CategoryTitle:
            qDebug() << "CategoryTitle:" << DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size());
            break;
        case Parameter::ClickThroughURL:
            qDebug() << "ClickThroughURL:" << DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size());
            break;
        case Parameter::AlternativeLocationURL:
            qDebug() << "AlternativeLocationURL:" << DabTables::convertToQString(it.value().data(), uint8_t(DabCharset::UTF8), it.value().size());
            break;
        case Parameter::Alert:
            qDebug() << "Alert:";
            break;
        default:
            qDebug() << "Parameter"<< it.key() << "not supported by Slideshow application";
        }

        ++it;
    }
}
