/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDir>
#include <QFileDialog>
#include "inputdevicerecorder.h"
#include "dabtables.h"
#include "config.h"

InputDeviceRecorder::InputDeviceRecorder()
{
    m_file = nullptr;
    m_recordingPath = QDir::homePath();
}

InputDeviceRecorder::~InputDeviceRecorder()
{
    stop();
}

const QString &InputDeviceRecorder::recordingPath() const
{
    return m_recordingPath;
}

void InputDeviceRecorder::setRecordingPath(const QString &recordingPath)
{
    m_recordingPath = recordingPath;
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
        // dialog needs parrent => prvided from caller widget
        QString fileName;
        if (m_xmlHeaderEna)
        {
            QString f = QString("%1/%2_%3.uff").arg(m_recordingPath,
                                                    QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"),
                                                    DabTables::channelList.value(m_frequency));

            fileName = QFileDialog::getSaveFileName(callerWidget,
                                                    tr("Record IQ stream (Raw File XML Header)"),
                                                    QDir::toNativeSeparators(f),
                                                    tr("Binary XML files")+" (*.uff)");
        }
        else
        {
            QString f = QString("%1/%2_%3.raw").arg(m_recordingPath,
                                                    QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"),
                                                    DabTables::channelList.value(m_frequency));

            fileName = QFileDialog::getSaveFileName(callerWidget,
                                                    tr("Record IQ stream"),
                                                    QDir::toNativeSeparators(f),
                                                    tr("Binary files")+" (*.raw)");
        }
        if (!fileName.isEmpty())
        {
            m_bytesRecorded = 0;
            m_recordingPath = QFileInfo(fileName).path(); // store path for next time

            m_file = fopen(QDir::toNativeSeparators(fileName).toUtf8().data(), "wb");
            if (nullptr != m_file)
            {
                startXmlHeader();
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
        if (m_xmlHeaderEna)
        {
            finishXmlHeader();

            // write xml header
            QByteArray bytearray = m_xmlHeader.toByteArray();
            fseek(m_file, 0, SEEK_SET);
            fwrite(bytearray.data(), 1, bytearray.size(), m_file);

            // add padding
            int paddingBytes = INPUTDEVICERECORDER_XML_PADDING - bytearray.size();
            char * padding = new char[paddingBytes];
            memset(padding, 0, paddingBytes);
            fwrite(padding, 1, paddingBytes, m_file);
            delete [] padding;
        }
        else { /* XML header is not enabled */ }
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

void InputDeviceRecorder::startXmlHeader()
{
    QDomDocument xmlHeader;
    QDomProcessingInstruction header = xmlHeader.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
    xmlHeader.appendChild(header);
    QDomElement root = xmlHeader.createElement("SDR");
    xmlHeader.appendChild(root);

    QDomElement recorder = xmlHeader.createElement("Recorder");
    recorder.setAttribute("Name", "AbracaDABra");
#if PROJECT_VERSION_RELEASE
    recorder.setAttribute("Version", QString("%1").arg(PROJECT_VER));
#else
    recorder.setAttribute("Version", QString("%1+").arg(PROJECT_VER));
#endif
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
    datablock.setAttribute("Count", QString("%1").arg(8 * m_bytesRecorded/m_deviceDescription.sample.containerBits));
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
