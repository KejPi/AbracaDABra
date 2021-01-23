#ifndef DLDECODER_H
#define DLDECODER_H

#include <QObject>

#define XPAD_DL_NUM_DG_MAX (8)
#define XPAD_DL_LEN_MAX  (XPAD_DL_NUM_DG_MAX*16)

#define DLDECODER_VERBOSE 1

class DLDecoder : public QObject
{
    Q_OBJECT
public:
    explicit DLDecoder(QObject *parent = nullptr);

public slots:
    void newDataGroup(const QByteArray & dataGroup);
    void reset();

signals:
    void dlComplete(const QString & dl);
private:
    quint8 charset;
    quint8 segmentCntr;
    quint8 toggle;
    QByteArray label;
    QByteArray dlCommand;
    bool crc16check(const QByteArray & data);
};

#endif // DLDECODER_H
