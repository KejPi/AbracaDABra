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


enum class RawFileInputFormat {
    SAMPLE_FORMAT_U8,
    SAMPLE_FORMAT_S16,
};

class RawFileWorker : public QThread
{
    Q_OBJECT
public:
    explicit RawFileWorker(QFile * inFile, RawFileInputFormat sFormat, QObject *parent = nullptr);
    void trigger();
    void stop();
protected:
    void run() override;
signals:
    void endOfFile();
private:
    QAtomicInt stopRequest = false;
    QSemaphore sem;
    QFile * inputFile = nullptr;
    QElapsedTimer elapsedTimer;
    qint64 lastTriggerTime = 0;
    RawFileInputFormat sampleFormat;
};


class RawFileInput : public InputDevice
{
    Q_OBJECT
public:
    explicit RawFileInput(QObject *parent = nullptr);
    ~RawFileInput();
    bool isAvailable() override { return true; }   // raw file is always available

public slots:
    void tune(uint32_t freq) override;
    void stop() override;
    void openDevice(const QString & fileName, const RawFileInputFormat &format = RawFileInputFormat::SAMPLE_FORMAT_U8);
    void setFileFormat(const RawFileInputFormat &format);

private:
    RawFileInputFormat sampleFormat;
    QFile * inputFile = nullptr;
    RawFileWorker * worker = nullptr;
    QTimer * inputTimer = nullptr;
    uint64_t getNumSamples();
    void rewind();
    void onEndOfFile() { emit error(InputDeviceErrorCode::EndOfFile); }
};


#endif // RAWFILEINPUT_H
