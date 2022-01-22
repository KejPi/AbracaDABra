#ifndef USERAPPLICATION_H
#define USERAPPLICATION_H

#include <QObject>
#include "radiocontrol.h"
#include "motdecoder.h"

class UserApplication : public QObject
{
    Q_OBJECT
public:
    UserApplication(RadioControl * radioControlPtr, QObject *parent = nullptr);

    virtual void onNewMOTObject(const MOTObject & obj) = 0;
    virtual void onUserAppData(const RadioControlUserAppData & data) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void restart() = 0;    

protected:
    bool isRunning;
    RadioControl * radioControl;
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

public:
    SlideShowApp(RadioControl * radioControlPtr, QObject *parent = nullptr);
    ~SlideShowApp();
    void onNewMOTObject(const MOTObject & obj) override;
    void onUserAppData(const RadioControlUserAppData & data) override;
    void start() override;
    void stop() override;
    void restart() override;

signals:
    void newSlide(const QByteArray & data);

private:
    MOTDecoder * decoder;

};

#endif // USERAPPLICATION_H
