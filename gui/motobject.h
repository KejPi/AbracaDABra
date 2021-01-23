#ifndef MOTOBJECT_H
#define MOTOBJECT_H

#include <QObject>
#include <QByteArrayList>

class MOTObject
{

public:
    explicit MOTObject(QObject *parent = nullptr);
    MOTObject(uint16_t transportId, const uint8_t * segment, uint16_t segmenLen, bool lastFlag);
    uint16_t getId() const { return id; }
    void addBodySegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    bool isBodyComplete() const { return bodyComplete; }
    void getBody(QByteArray & b);
    void reset();
private:
    uint16_t id;

    uint32_t bodySize;
    uint16_t headerSize;
    uint16_t contentType;
    uint16_t contentSubType;

    QByteArrayList body;

    uint16_t numBodySegments;
    bool bodyComplete;
};

#endif // MOTOBJECT_H
