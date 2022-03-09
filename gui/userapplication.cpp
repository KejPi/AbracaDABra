#include <QDebug>

#include "userapplication.h"
#include "dabtables.h"

UserApplication::UserApplication(RadioControl * radioControlPtr, QObject *parent) :
    QObject(parent), radioControl(radioControlPtr)
{    
    isRunning = false;
}

