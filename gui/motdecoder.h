#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QObject>
#include <QByteArray>

#include "motobject.h"

#define MOTDECODER_VERBOSE 1

class MOTDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MOTDecoder(QObject *parent = nullptr);    

signals:
    void motObjectComplete(const QByteArray & b);

public slots:
    void newDataGroup(const QByteArray &dataGroup);
    void reset();   

private:
    QList<MOTObject> motObjList;
    int findMotObj(uint16_t transportId);
    int addMotObj(const MOTObject & obj);
    bool crc16check(const QByteArray & data);
};

#endif // MOTDECODER_H
