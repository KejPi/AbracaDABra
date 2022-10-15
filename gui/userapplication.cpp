#include <QDebug>

#include "userapplication.h"

UserApplication::UserApplication(RadioControl * radioControlPtr, QObject *parent) :
    QObject(parent), m_radioControl(radioControlPtr)
{    
    m_isRunning = false;
}

