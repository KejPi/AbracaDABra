#ifndef MOTOBJECT_H
#define MOTOBJECT_H

#include <QObject>
#include <QByteArrayList>
#include <QHash>
#include <QSharedData>

#define MOTOBJECT_VERBOSE 0

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

class MOTObjectData : public QSharedData
{
public:
    MOTObjectData(int_fast32_t transportId);
    MOTObjectData(const MOTObjectData &other);
    ~MOTObjectData() { }

    int_fast32_t id;
    int32_t bodySize;
    bool objectIsComplete;
    bool objectIsObsolete;   // this is used to remove obosolete obcect when new MOT directory is received

    uint16_t contentType;
    uint16_t contentSubType;
    QString contentName;

    MOTEntity header;
    MOTEntity body;

    QHash<int, QByteArray> userAppParams;

    void parseHeader();
};

class MOTObject
{
public:    
    MOTObject(int_fast32_t transportId);

    MOTObject(const MOTObject &other) : d (other.d) { }

    uint16_t getId() const { return d->id; }
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag, bool isHeader = false);
    bool isComplete() const { return d->objectIsComplete; };
    QByteArray getBody();
    bool isObsolete() const { return d->objectIsObsolete; }
    void setObsolete(bool obsolete) { d->objectIsComplete = obsolete; };

    uint16_t getContentType() const;
    uint16_t getContentSubType() const;
    const QString &getContentName() const;

    // iterator access to user parameters
    typedef QHash<int, QByteArray>::const_iterator paramsIterator;

    MOTObject::paramsIterator paramsBegin() const { return d->userAppParams.cbegin(); }
    MOTObject::paramsIterator paramsEnd() const { return d->userAppParams.cend(); }
private:
    QSharedDataPointer<MOTObjectData> d;
};


class MOTObjectCache
{
public:   
    MOTObjectCache();
    ~MOTObjectCache();
    void clear();
    int size() const { return cache.size(); }

    // iterator access
    typedef QList<MOTObject>::iterator iterator;
    typedef QList<MOTObject>::const_iterator const_iterator;

    MOTObjectCache::iterator findMotObj(uint16_t transportId);   // find MOT object in the cache
    MOTObjectCache::iterator addMotObj(const MOTObject &obj);          // add MOT boject to cache
    void deleteMotObj(uint16_t transportId);        // delete mote object from the cache, this deletes the object completely

    void markAllObsolete();
    MOTObjectCache::iterator markObjObsolete(uint16_t transportId, bool obsolete = true);
    void deleteObsolete();

    MOTObjectCache::iterator begin() { return cache.begin(); }
    MOTObjectCache::iterator end() { return cache.end(); }
    MOTObjectCache::const_iterator cbegin() const { return cache.cbegin(); }
    MOTObjectCache::const_iterator cend() const { return cache.cend(); }
private:
    QList<MOTObject> cache;
};


class MOTDirectory
{
public:
    MOTDirectory(uint_fast32_t transportId, MOTObjectCache * cachePtr);
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
