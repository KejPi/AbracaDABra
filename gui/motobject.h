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
    QByteArray getData() const;
    void reset();
private:
    QByteArrayList m_segments;
    int_fast32_t m_numSegments;
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
    QByteArray getBody() const;
    bool isObsolete() const { return d->objectIsObsolete; }
    void setObsolete(bool obsolete) { d->objectIsObsolete = obsolete; };

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
    int size() const { return m_cache.size(); }

    // iterator access
    typedef QList<MOTObject>::iterator iterator;
    typedef QList<MOTObject>::const_iterator const_iterator;

    MOTObjectCache::iterator findMotObj(uint16_t transportId);   // find MOT object in the cache
    MOTObjectCache::iterator addMotObj(const MOTObject &obj);          // add MOT boject to cache
    void deleteMotObj(uint16_t transportId);        // delete mote object from the cache, this deletes the object completely

    void markAllObsolete();
    MOTObjectCache::iterator markObjObsolete(uint16_t transportId, bool obsolete = true);
    void deleteObsolete();

    MOTObjectCache::iterator begin() { return m_cache.begin(); }
    MOTObjectCache::iterator end() { return m_cache.end(); }
    MOTObjectCache::const_iterator cbegin() const { return m_cache.cbegin(); }
    MOTObjectCache::const_iterator cend() const { return m_cache.cend(); }
private:
    QList<MOTObject> m_cache;
};


class MOTDirectory
{
public:
    MOTDirectory(uint_fast32_t transportId, MOTObjectCache * cachePtr);
    bool addSegment(const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    void addObjectSegment(uint_fast32_t transportId, const uint8_t *segment, uint16_t segmentNum, uint16_t segmentSize, bool lastFlag);
    uint_fast32_t getTransportId() const { return m_id; }
    MOTObjectCache::const_iterator begin() const { return  m_carousel->cbegin(); }
    MOTObjectCache::const_iterator end() const { return  m_carousel->cend(); }
private:
    uint_fast32_t m_id;
    MOTEntity m_dir;
    MOTObjectCache * m_carousel;

    bool parse(const QByteArray &dirData);
};


#endif // MOTOBJECT_H
