#ifndef USERAPPLICATION_H
#define USERAPPLICATION_H

#include <QObject>
#include "motobject.h"

class UserApplication : public QObject
{
    Q_OBJECT
public:
    UserApplication(QObject *parent = nullptr);

    virtual void onNewMOTObject(const MOTObject & obj) = 0;
};

class SlideShowApp : public UserApplication
{
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

public:
    SlideShowApp(QObject *parent = nullptr);
    void onNewMOTObject(const MOTObject & obj) override;
};

#endif // USERAPPLICATION_H
