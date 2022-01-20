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

signals:
    void motObjectComplete(const QByteArray & b);
    void newMOTObject(const MOTObject & obj);

public slots:
    void newDataGroup(const QByteArray &dataGroup);
    void reset();   

private:
    MOTDirectory * directory;
    MOTObjectCache * objCache;
};

#endif // MOTDECODER_H
