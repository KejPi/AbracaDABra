#ifndef INPUTDEVICERECORDER_H
#define INPUTDEVICERECORDER_H

#include <QObject>
#include <mutex>
#include <QDomDocument>
#include "inputdevice.h"

#define INPUTDEVICERECORDER_XML_PADDING 2048

class InputDeviceRecorder : public QObject
{
    Q_OBJECT
public:
    InputDeviceRecorder();
    ~InputDeviceRecorder();
    const QString &recordingPath() const;
    void setRecordingPath(const QString &recordingPath);
    void setDeviceDescription(const InputDeviceDescription & desc);
    void start(QWidget *callerWidget);
    void stop();
    void writeBuffer(const uint8_t *buf, uint32_t len);
    void setCurrentFrequency(uint32_t frequency) { m_frequency = frequency; }
    void setXmlHeaderEnabled(bool ena) { m_xmlHeaderEna = ena; }
signals:
    void recording(bool isActive);
    void bytesRecorded(uint64_t bytes, uint64_t ms);

private:
    InputDeviceDescription m_deviceDescription;
    FILE * m_file;
    std::mutex m_fileMutex;
    uint64_t m_bytesRecorded = 0;
    float m_bytes2ms;
    uint32_t m_frequency;
    QString m_recordingPath;
    bool m_xmlHeaderEna = true;
    QDomDocument m_xmlHeader;
    void startXmlHeader();
    void finishXmlHeader();
};

#endif // INPUTDEVICERECORDER_H
