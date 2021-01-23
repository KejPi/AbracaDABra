#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QObject>
#include <QByteArray>

#include "motobject.h"

#define MOTDECODER_VERBOSE 0

typedef QList<MOTObject>::Iterator motObjListIterator;
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
    int addMotObj(uint16_t transportId, const uint8_t *segment, uint16_t segmenLen, bool lastFlag);
    bool crc16check(const QByteArray & data);
};

#endif // MOTDECODER_H
