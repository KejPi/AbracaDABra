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
    QDate currentDate() const { return m_dabTime.date(); }

    void onDabTime(const QDateTime & d);

    qint64 secSinceEpoch() const;
    void setSecSinceEpoch(qint64 newSecSinceEpoch);


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
