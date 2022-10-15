#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QObject>
#include <QByteArray>

#include "motobject.h"

#define MOTDECODER_VERBOSE 0

class MOTDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MOTDecoder(QObject *parent = nullptr);
    ~MOTDecoder();
    void newDataGroup(const QByteArray &dataGroup);
    void reset();
    MOTObjectCache::const_iterator directoryBegin() const { return  m_directory->begin(); }
    MOTObjectCache::const_iterator directoryEnd() const { return  m_directory->end(); }

signals:
    void newMOTObject(const MOTObject & obj);
    void newMOTDirectory();

private:
    MOTDirectory * m_directory;
    MOTObjectCache * m_objCache;
};

#endif // MOTDECODER_H
