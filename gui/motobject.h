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
    bool isObsolete() const { return objectIsObsolete; }
    void setObsolete(bool obsolete) { objectIsComplete = obsolete; };
private:
    uint_fast32_t id;
    int32_t bodySize;
    bool objectIsComplete;
    bool objectIsObsolete;   // this is used to remove obosolete obcect when new MOT directory is received

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

class MOTObjectCache
{
public:
    MOTObjectCache();
    ~MOTObjectCache();
    void clear();
    int size() const { return cache.size(); }
    MOTObject * findMotObj(uint16_t transportId);   // find MOT object in the cache
    MOTObject * addMotObj(MOTObject *obj);          // add MOT boject to cache
    void deleteMotObj(uint16_t transportId);        // delete mote object from the cache, this deletes the object completely

    void markAllObsolete();
    MOTObject *markObjObsolete(uint16_t transportId, bool obsolete = true);
    void deleteObsolete();
private:
    QList<MOTObject*> cache;
};


class MOTDirectory
{
public:
    MOTDirectory(uint_fast32_t transportId, MOTObjectCache * cachePtr);
    ~MOTDirectory();
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    void addObjectSegment(uint_fast32_t transportId, const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    uint_fast32_t getTransportId() const { return id; }
private:
    uint_fast32_t id;
    MOTEntity dir;
    MOTObjectCache * carousel;

    bool parse(const QByteArray &dirData);
};


#endif // MOTOBJECT_H
