#ifndef MOTOBJECT_H
#define MOTOBJECT_H

#include <QObject>
#include <QByteArrayList>
#include <QHash>

#define MOTOBJECT_VERBOSE 1

class MOTEntity
{
public:
    MOTEntity();
    bool isComplete() const;
    int size();
    void addSegment(const uint8_t * segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    QByteArray getData();
    void reset();
private:
    QByteArrayList segments;
    int_fast32_t numSegments;
};

class MOTObject
{

public:    
    MOTObject(uint_fast32_t transportId);
    uint16_t getId() const { return id; }
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag, bool isHeader = false);
    bool isComplete() const { return objectIsComplete; };
    QByteArray getBody();
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

    QHash<int, QByteArray> userAppParams;

    bool parseHeader(const QByteArray &headerData);
};

class MOTDirectory
{
public:
    MOTDirectory(uint_fast32_t transportId);
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
private:
    uint_fast32_t id;
    MOTEntity dir;

    bool parse(const QByteArray &dirData);
};

#endif // MOTOBJECT_H
