#ifndef DABTIME_H
#define DABTIME_H

#include <QObject>
#include <QDateTime>
#include <QTimer>

// singleton class
class DABTime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 secSinceEpoch READ secSinceEpoch WRITE setSecSinceEpoch NOTIFY secSinceEpochChanged FINAL)
public:    
    DABTime(const DABTime& obj) = delete;   // deleting copy constructor
    ~DABTime();
    static DABTime *getInstance();
    QDateTime currentTime() const { return m_dabTime; }
    Q_INVOKABLE QDate currentDate() const { return m_dabTime.date(); }
    Q_INVOKABLE bool isCurrentDate(const QDate & date) const { return date == currentDate(); }

    qint64 secSinceEpoch() const;
    void setSecSinceEpoch(qint64 newSecSinceEpoch);

    Q_INVOKABLE int secSinceMidnight() const { return m_dabTime.date().startOfDay().secsTo(m_dabTime); }

    void onDabTime(const QDateTime & d);
signals:
    void secSinceEpochChanged();

private:
    DABTime();
    void setTime(const QDateTime &time);
    void onTimerTimeout();

    static DABTime * m_instancePtr;
    QDateTime m_dabTime;
    QTimer * m_minuteTimer;
    qint64 m_secSinceEpoch;
};

#endif // DABTIME_H
