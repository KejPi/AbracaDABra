#ifndef INPUTDEVICERECORDER_H
#define INPUTDEVICERECORDER_H

#include <QObject>
#include <mutex>
#include "inputdevice.h"

class InputDeviceRecorder : public QObject
{
    Q_OBJECT
public:
    InputDeviceRecorder();
    ~InputDeviceRecorder();
    void setDeviceDescription(const InputDeviceDescription & desc);
    void start(QWidget *callerWidget);
    void stop();
    void writeBuffer(const uint8_t *buf, uint32_t len);
    void setCurrentFrequency(uint32_t frequency) { m_frequency = frequency; }

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
};

#endif // INPUTDEVICERECORDER_H
