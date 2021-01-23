#ifndef MOTOBJECT_H
#define MOTOBJECT_H

#include <QObject>
#include <QByteArrayList>

class MOTObject
{

public:
    explicit MOTObject(QObject *parent = nullptr);
    MOTObject(quint16 transportId, const quint8 * segment, quint16 segmenLen, bool lastFlag);
    quint16 getId() const { return id; }
    void addBodySegment(const quint8 *segment, quint16 segmentNum, quint16 segmentSize, bool lastFlag);
    bool isBodyComplete() const { return bodyComplete; }
    void getBody(QByteArray & b);
    void reset();
private:
    quint16 id;

    quint32 bodySize;
    quint16 headerSize;
    quint16 contentType;
    quint16 contentSubType;

    QByteArrayList body;

    quint16 numBodySegments;
    bool bodyComplete;
};

#endif // MOTOBJECT_H
