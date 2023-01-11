#include <QDebug>

#include "userapplication.h"

UserApplication::UserApplication(QObject *parent) :
    QObject(parent)
{    
    m_isRunning = false;
}

