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

#ifndef INPUTDEVICERECORDER_H
#define INPUTDEVICERECORDER_H

#include <QDomDocument>
#include <QObject>
#include <mutex>

#include "inputdevice.h"

#define INPUTDEVICERECORDER_XML_PADDING 2048

class InputDeviceRecorder : public QObject
{
    Q_OBJECT
public:
    InputDeviceRecorder();
    ~InputDeviceRecorder();
    const QString recordingPath() const;
    void setRecordingPath(const QString &recordingPath);
    void setDeviceDescription(const InputDeviceDescription &desc);
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
    FILE *m_file;
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

#endif  // INPUTDEVICERECORDER_H
