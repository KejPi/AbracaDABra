#include <QDir>
#include <QFileDialog>
#include "inputdevicerecorder.h"
#include "dabtables.h"

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

void InputDeviceRecorder::start(QWidget * callerWidget)
{  
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr == m_file)
    {
        QString f = QString("%1/%2_%3.raw").arg(m_recordingPath,
                QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"),
                DabTables::channelList.value(m_frequency));

        // dialog needs parrent => prvided from caller widget
        QString fileName = QFileDialog::getSaveFileName(callerWidget,
                                                        tr("Record IQ stream"),
                                                        QDir::toNativeSeparators(f),
                                                        tr("Binary files (*.raw)"));
        if (!fileName.isEmpty())
        {
            m_recordingPath = QFileInfo(fileName).path(); // store path for next time

            m_file = fopen(QDir::toNativeSeparators(fileName).toUtf8().data(), "wb");
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
        {
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
