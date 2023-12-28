#include "dabtime.h"

DABTime* DABTime ::m_instancePtr = nullptr;

DABTime *DABTime::getInstance()
{
    if (m_instancePtr == nullptr)
    {
        m_instancePtr = new DABTime();
        return m_instancePtr;
    }
    else
    {
        return m_instancePtr;
    }
}

DABTime::DABTime() : QObject(nullptr)
{
    m_minuteTimer = new QTimer();
    m_minuteTimer->setInterval(1000*60); // 1 minute
}

DABTime::~DABTime()
{
    m_minuteTimer->stop();
    delete m_minuteTimer;
}

void DABTime::setTime(const QDateTime & time)
{
    m_dabTime = time;
    setSecSinceEpoch(m_dabTime.toSecsSinceEpoch());
}

void DABTime::onTimerTimeout()
{
    if (m_dabTime.isValid())
    {
        setTime(m_dabTime.addSecs(60));
    }
}


void DABTime::onDabTime(const QDateTime &d)
{
    setTime(d);
    m_minuteTimer->start();
}

qint64 DABTime::secSinceEpoch() const
{
    return m_secSinceEpoch;
}

void DABTime::setSecSinceEpoch(qint64 newSecSinceEpoch)
{
    if (m_secSinceEpoch == newSecSinceEpoch)
        return;
    m_secSinceEpoch = newSecSinceEpoch;
    emit secSinceEpochChanged();
}
