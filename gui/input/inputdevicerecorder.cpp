/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
#include <QFileDialog>
#include <QLoggingCategory>
#include <QTimer>

#include "config.h"
#include "dabtables.h"

Q_LOGGING_CATEGORY(inputDeviceRecorder, "InputDeviceRecorder", QtInfoMsg)

InputDeviceRecorder::InputDeviceRecorder()
{
    m_file = nullptr;
    m_recordingPath = QDir::homePath();
}

InputDeviceRecorder::~InputDeviceRecorder()
{
    stop();
}

const QString InputDeviceRecorder::recordingPath() const
{
    return m_recordingPath;
}

void InputDeviceRecorder::setRecordingPath(const QString &recordingPath)
{
    m_recordingPath = recordingPath;
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

void InputDeviceRecorder::start(QWidget *callerWidget, int timeoutSec)
{
    std::lock_guard<std::mutex> guard(m_fileMutex);
    if (nullptr == m_file)
    {
        // dialog needs parrent => prvided from caller widget
        QString fileName;
        if (m_xmlHeaderEna)
        {
            QString f =
                QString("%1/%2_%3.uff")
                    .arg(m_recordingPath, QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(m_frequency));

            fileName = QFileDialog::getSaveFileName(callerWidget, tr("Record IQ stream (Raw File XML Header)"), QDir::toNativeSeparators(f),
                                                    tr("Binary XML files") + " (*.uff)");
        }
        else
        {
            QString f =
                QString("%1/%2_%3.raw")
                    .arg(m_recordingPath, QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(m_frequency));

            fileName =
                QFileDialog::getSaveFileName(callerWidget, tr("Record IQ stream"), QDir::toNativeSeparators(f), tr("Binary files") + " (*.raw)");
        }
        if (!fileName.isEmpty())
        {
            m_bytesRecorded = 0;
            m_bytesToRecord = timeoutSec * m_bytesPerSec;
            m_recordingPath = QFileInfo(fileName).path();  // store path for next time

            m_file = fopen(QDir::toNativeSeparators(fileName).toUtf8().data(), "wb");
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
                qCInfo(inputDeviceRecorder) << "IQ recording starts, timeout:" << timeoutSec << "sec, file:" << fileName;
                emit recording(true);
            }
            else
            {  // error
                emit recording(false);
            }
        }
        else
        {
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
