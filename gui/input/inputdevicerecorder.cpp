/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "inputdevicerecorder.h"

#include <QDir>
#include <QLoggingCategory>
#include <QTimer>

#include "androidfilehelper.h"
#include "config.h"
#include "dabtables.h"
#include "settings.h"

Q_LOGGING_CATEGORY(inputDeviceRecorder, "InputDeviceRecorder", QtInfoMsg)

InputDeviceRecorder::InputDeviceRecorder(Settings *settings) : m_settings(settings)
{
    m_file = nullptr;
}

InputDeviceRecorder::~InputDeviceRecorder()
{
    stop();
}

void InputDeviceRecorder::setDeviceDescription(const InputDevice::Description &desc)
{
    m_deviceDescription = desc;

    // 1 / (2 channels(IQ) * channel containerBits/8.0 * sampleRate/1000.0)
    m_bytes2ms = 8 * 1000.0 / (2 * m_deviceDescription.sample.containerBits * m_deviceDescription.sample.sampleRate);
    m_bytesPerSec = (2 * m_deviceDescription.sample.containerBits * m_deviceDescription.sample.sampleRate) / 8;

    qCDebug(inputDeviceRecorder) << "name:" << m_deviceDescription.device.name;
    qCDebug(inputDeviceRecorder) << "model:" << m_deviceDescription.device.model;
    qCDebug(inputDeviceRecorder) << "sampleRate:" << m_deviceDescription.sample.sampleRate;
    qCDebug(inputDeviceRecorder) << "channelBits:" << m_deviceDescription.sample.channelBits;
    qCDebug(inputDeviceRecorder) << "channelContainer:" << m_deviceDescription.sample.channelContainer;
}

void InputDeviceRecorder::start(int timeoutSec)
{
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr == m_file)
    {
        const QString rawPath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, RAW_DIR_NAME);

        // Ensure directory exists and is writable
        if (!AndroidFileHelper::mkpath(m_settings->dataStoragePath, RAW_DIR_NAME))
        {
            qCCritical(inputDeviceRecorder) << "Failed to create raw recording directory:" << AndroidFileHelper::lastError();
            emit recording(false);
            return;
        }

        if (!AndroidFileHelper::hasWritePermission(rawPath))
        {
            qCCritical(inputDeviceRecorder) << "No permission to write to:" << rawPath;
            qCCritical(inputDeviceRecorder) << "Please select a new data storage folder in settings.";
            emit recording(false);
            return;
        }

        QString fileName;
        if (m_xmlHeaderEna)
        {
            fileName =
                QString("%1_%2.uff").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(m_frequency));
        }
        else
        {
            fileName =
                QString("%1_%2.raw").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(m_frequency));
        }

        m_bytesRecorded = 0;
        m_bytesToRecord = timeoutSec * m_bytesPerSec;

        m_file = AndroidFileHelper::openFileForWritingRaw(rawPath, fileName, "application/octet-stream");
        if (nullptr != m_file)
        {
            if (m_xmlHeaderEna)
            {
                startXmlHeader();
                char *padding = new char[INPUTDEVICERECORDER_XML_PADDING];
                memset(padding, 0, INPUTDEVICERECORDER_XML_PADDING);
                fwrite(padding, 1, INPUTDEVICERECORDER_XML_PADDING, m_file);
                delete[] padding;
            }
            qCInfo(inputDeviceRecorder) << "IQ recording starts, timeout:" << timeoutSec << "sec, file:" << QString("%1/%2").arg(rawPath, fileName);
            emit recording(true);
        }
        else
        {  // error
            emit recording(false);
        }
    }
    else
    { /* file is already opened */
    }

    return;
}

void InputDeviceRecorder::stop()
{
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr != m_file)
    {
        if (m_xmlHeaderEna)
        {
            finishXmlHeader();

            // write xml header
            QByteArray bytearray = m_xmlHeader.toByteArray();
            fseek(m_file, 0, SEEK_SET);
            fwrite(bytearray.data(), 1, bytearray.size(), m_file);
        }
        else
        { /* XML header is not enabled */
        }
        fflush(m_file);
        fclose(m_file);
        m_file = nullptr;

        qCInfo(inputDeviceRecorder) << "IQ recording stopped";
        emit recording(false);
    }
}

void InputDeviceRecorder::writeBuffer(const uint8_t *buf, uint32_t len)
{
    std::lock_guard<std::mutex> guard(m_fileMutex);

    if (nullptr != m_file)
    {
        if (m_bytesToRecord == 0)
        {
            ssize_t bytes = fwrite(buf, 1, len, m_file);
            m_bytesRecorded += bytes;
            emit bytesRecorded(m_bytesRecorded, m_bytesRecorded * m_bytes2ms);
            return;
        }

        // else some timeout was set
        {
            if ((m_bytesRecorded + len) < m_bytesToRecord)
            {  // no timeout yet
                ssize_t bytes = fwrite(buf, 1, len, m_file);
                m_bytesRecorded += bytes;
                emit bytesRecorded(m_bytesRecorded, m_bytesRecorded * m_bytes2ms);
                return;
            }
            // else timeout
            {
                ssize_t bytes = fwrite(buf, 1, m_bytesToRecord - m_bytesRecorded, m_file);
                m_bytesRecorded += bytes;
                emit bytesRecorded(m_bytesRecorded, m_bytesRecorded * m_bytes2ms);
                if (m_bytesRecorded >= m_bytesToRecord)
                {
                    qCInfo(inputDeviceRecorder) << "IQ recording timeout reached";

                    // this is to avoid deadlock due to mutex
                    QTimer::singleShot(1, this, [this]() { stop(); });
                }
            }
        }
    }
}

void InputDeviceRecorder::startXmlHeader()
{
    QDomDocument xmlHeader;
    QDomProcessingInstruction header = xmlHeader.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
    xmlHeader.appendChild(header);
    QDomElement root = xmlHeader.createElement("SDR");
    xmlHeader.appendChild(root);

    QDomElement recorder = xmlHeader.createElement("Recorder");
    recorder.setAttribute("Name", "AbracaDABra");
    recorder.setAttribute("Version", QString("%1").arg(PROJECT_VER));
    root.appendChild(recorder);

    QDomElement device = xmlHeader.createElement("Device");
    device.setAttribute("Name", m_deviceDescription.device.name);
    device.setAttribute("Model", m_deviceDescription.device.model);
    root.appendChild(device);

    QDomElement time = xmlHeader.createElement("Time");
    time.setAttribute("Value", QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss"));
    time.setAttribute("Unit", "UTC");
    root.appendChild(time);

    QDomElement sample = xmlHeader.createElement("Sample");
    QDomElement sampleRate = xmlHeader.createElement("Samplerate");
    sampleRate.setAttribute("Value", QString("%1").arg(m_deviceDescription.sample.sampleRate));
    sampleRate.setAttribute("Unit", "Hz");
    sample.appendChild(sampleRate);

    QDomElement channels = xmlHeader.createElement("Channels");
    channels.setAttribute("Bits", QString("%1").arg(m_deviceDescription.sample.channelBits));
    channels.setAttribute("Container", m_deviceDescription.sample.channelContainer);
#if (Q_BYTE_ORDER == Q_LITTLE_ENDIAN)
    channels.setAttribute("Ordering", "LSB");
#else
    channels.setAttribute("Ordering", "MSB");
#endif

    QDomElement channelI = xmlHeader.createElement("Channel");
    channelI.setAttribute("Value", "I");
    channels.appendChild(channelI);
    QDomElement channelQ = xmlHeader.createElement("Channel");
    channelQ.setAttribute("Value", "Q");
    channels.appendChild(channelQ);
    sample.appendChild(channels);
    root.appendChild(sample);

    m_xmlHeader = xmlHeader;
}

void InputDeviceRecorder::finishXmlHeader()
{
    QDomElement datablocks = m_xmlHeader.createElement("Datablocks");
    QDomElement datablock = m_xmlHeader.createElement("Datablock");
    datablock.setAttribute("Number", "1");
    datablock.setAttribute("Count", QString("%1").arg(8 * m_bytesRecorded / m_deviceDescription.sample.containerBits));
    datablock.setAttribute("Unit", "Channel");
    datablock.setAttribute("Offset", QString("%1").arg(INPUTDEVICERECORDER_XML_PADDING));

    QDomElement frequency = m_xmlHeader.createElement("Frequency");
    frequency.setAttribute("Value", QString("%1").arg(m_frequency));
    frequency.setAttribute("Unit", "kHz");
    datablock.appendChild(frequency);
    QDomElement modulation = m_xmlHeader.createElement("Modulation");
    modulation.setAttribute("Value", "DAB");
    datablock.appendChild(modulation);
    datablocks.appendChild(datablock);
    m_xmlHeader.childNodes().at(1).appendChild(datablocks);
}
