#include <QDir>
#include "inputdevicerecorder.h"

InputDeviceRecorder::InputDeviceRecorder()
{
    m_file = nullptr;
}

InputDeviceRecorder::~InputDeviceRecorder()
{
    stop();
}

void InputDeviceRecorder::setDeviceDescription(const InputDeviceDescription &desc)
{
    m_deviceDescription = desc;

    // 1 / (2 channels(IQ) * channel containerBits/8.0 * sampleRate/1000.0)
    m_bytes2ms = 8*1000.0/(2 * m_deviceDescription.sample.containerBits * m_deviceDescription.sample.sampleRate);

#if 0
    qDebug() << "name:" << m_deviceDescription.device.name;
    qDebug() << "model:" << m_deviceDescription.device.model;
    qDebug() << "sampleRate:" << m_deviceDescription.sample.sampleRate;
    qDebug() << "channelBits:" << m_deviceDescription.sample.channelBits;
    qDebug() << "channelContainer:" << m_deviceDescription.sample.channelContainer;
#endif
}

void InputDeviceRecorder::start(const QString &filename)
{  
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr == m_file)
    {
        m_file = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
        if (nullptr != m_file)
        {
            emit recording(true);
        }
        else
        {   // error
            emit recording(false);
        }
    }
    else
    { /* file is already opened */ }

    return;
}

void InputDeviceRecorder::stop()
{
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr != m_file)
    {
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;

        emit recording(false);
    }
}

void InputDeviceRecorder::writeBuffer(const uint8_t *buf, uint32_t len)
{
    std::lock_guard<std::mutex> guard(m_fileMutex);

    if (nullptr != m_file)
    {
        ssize_t bytes = fwrite(buf, 1, len, m_file);
        m_bytesRecorded += bytes;
        emit bytesRecorded(m_bytesRecorded, m_bytesRecorded * m_bytes2ms);
    }
}
