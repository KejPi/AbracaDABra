#ifndef MOTOBJECT_H
#define MOTOBJECT_H

#include <QObject>
#include <QByteArrayList>
#include <QHash>

#define MOTOBJECT_VERBOSE 1

typedef QVector<uint8_t> uint8Vector_t;

class MOTEntity
{
public:
    MOTEntity();
    bool isComplete() const;
    int size();
    void addSegment(const uint8_t * segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    uint8Vector_t getData();
    void reset();
private:
    QList<uint8Vector_t> segments;
    int_fast32_t numSegments;
};

class MOTObject
{

public:    
    MOTObject(uint_fast32_t transportId);
    uint16_t getId() const { return id; }
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag, bool isHeader = false);
    bool isComplete() const { return objectIsComplete; };
    QVector<uint8_t> getBody();
private:
    uint_fast32_t id;
    int32_t bodySize;
    bool objectIsComplete;

    MOTEntity header;
    MOTEntity body;

    struct {
        uint16_t contentType;
        uint16_t contentSubType;
        QString ContentName;
    } headerParams;

    QHash<int, uint8Vector_t> userAppParams;

    bool parseHeader(const uint8Vector_t &headerData);
};

#endif // MOTOBJECT_H
