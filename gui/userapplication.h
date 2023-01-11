#ifndef USERAPPLICATION_H
#define USERAPPLICATION_H

#include <QObject>
#include "radiocontrol.h"
#include "motobject.h"

#define USER_APPLICATION_VERBOSE 1

class UserApplication : public QObject
{
    Q_OBJECT
public:
    UserApplication(QObject *parent = nullptr);

    virtual void onNewMOTObject(const MOTObject & obj) = 0;
    virtual void onUserAppData(const RadioControlUserAppData & data) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void restart() = 0;    

signals:
    void resetTerminal();

protected:
    bool m_isRunning;
};

#endif // USERAPPLICATION_H
