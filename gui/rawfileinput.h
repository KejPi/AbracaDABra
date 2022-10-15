#ifndef RAWFILEINPUT_H
#define RAWFILEINPUT_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QSemaphore>


#include "inputdevice.h"


enum class RawFileInputFormat
{
    SAMPLE_FORMAT_U8,
    SAMPLE_FORMAT_S16,
};

class RawFileWorker : public QThread
{
    Q_OBJECT
public:
    explicit RawFileWorker(QFile * inputFile, RawFileInputFormat sampleFormat, QObject *parent = nullptr);
    void trigger();
    void stop();
protected:
    void run() override;
signals:
    void endOfFile();
private:
    QAtomicInt m_stopRequest = false;
    QSemaphore m_semaphore;
    QFile * m_inputFile = nullptr;
    QElapsedTimer m_elapsedTimer;
    qint64 m_lastTriggerTime = 0;
    RawFileInputFormat m_sampleFormat;
};


class RawFileInput : public InputDevice
{
    Q_OBJECT
public:
    explicit RawFileInput(QObject *parent = nullptr);
    ~RawFileInput();
    bool openDevice() override;

public slots:
    void tune(uint32_t freq) override;
    void setFile(const QString & fileName, const RawFileInputFormat & sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8);
    void setFileFormat(const RawFileInputFormat & sampleFormat);

private:
    RawFileInputFormat m_sampleFormat;
    QString m_fileName;
    QFile * m_inputFile = nullptr;
    RawFileWorker * m_worker = nullptr;
    QTimer * m_inputTimer = nullptr;
    void stop();
    void rewind();
    void onEndOfFile() { emit error(InputDeviceErrorCode::EndOfFile); }
};


#endif // RAWFILEINPUT_H
