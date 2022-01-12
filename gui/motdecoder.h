#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QObject>
#include <QByteArray>

#include "motobject.h"

#define MOTDECODER_VERBOSE 1

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
    MOTObjectCache * objCache;
};

#endif // MOTDECODER_H
