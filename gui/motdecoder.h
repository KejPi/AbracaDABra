#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QObject>
#include <QByteArray>

#include "motobject.h"

#define MOTDECODER_VERBOSE 1

class MOTObjectCache
{
public:
    MOTObjectCache();
    ~MOTObjectCache();
    void clear();
    int size() const { return cache.size(); }
    MOTObject * findMotObj(uint16_t transportId);
    MOTObject * addMotObj(MOTObject *obj);
    void deleteMotObj(uint16_t transportId);
private:
    QList<MOTObject*> cache;
};


class MOTDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MOTDecoder(QObject *parent = nullptr);
    ~MOTDecoder();

signals:
    void motObjectComplete(const QByteArray & b);

public slots:
    void newDataGroup(const QByteArray &dataGroup);
    void reset();   

private:
    MOTDirectory * directory;
    MOTObjectCache objCache;
    bool crc16check(const QByteArray & data);
};

#endif // MOTDECODER_H
