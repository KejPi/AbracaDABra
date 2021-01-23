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
    int findMotObj(quint16 transportId);
    int addMotObj(quint16 transportId, const quint8 *segment, quint16 segmenLen, bool lastFlag);
    bool crc16check(const QByteArray & data);
};

#endif // MOTDECODER_H
