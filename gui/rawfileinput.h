#ifndef RAWFILEINPUT_H
#define RAWFILEINPUT_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTimer>

#include "inputdevice.h"


enum class RawFileInputFormat {
    SAMPLE_FORMAT_U8,
    SAMPLE_FORMAT_S16,
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

signals:
    void numberOfSamples(uint64_t nsamples);
    void endOfFile();

private:
    RawFileInputFormat sampleFormat;
    QFile * inputFile;
    QTimer * inputTimer;
    uint64_t getNumSamples();
    void rewind();

    friend void getSamples(float _Complex buffer[], uint16_t len);
private slots:
    void readSamples();
};


#endif // RAWFILEINPUT_H
