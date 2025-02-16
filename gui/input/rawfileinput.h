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

#ifndef RAWFILEINPUT_H
#define RAWFILEINPUT_H

#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QThread>
#include <QTimer>

#include "inputdevice.h"

#define RAWFILEINPUT_XML_PADDING 2048

enum class RawFileInputFormat
{
    SAMPLE_FORMAT_U8,
    SAMPLE_FORMAT_S16,
};

class RawFileWorker : public QThread
{
    Q_OBJECT
public:
    explicit RawFileWorker(QFile *inputFile, RawFileInputFormat sampleFormat, qint64 bytesRead, QObject *parent = nullptr);
    void trigger();
    void stop();
    bool isRunning();

protected:
    void run() override;
signals:
    void bytesRead(qint64 numBytes);
    void endOfFile(bool status);

private:
    QAtomicInt m_stopRequest = false;
    std::atomic<bool> m_watchdogFlag;
    QSemaphore m_semaphore;
    QFile *m_inputFile = nullptr;
    QElapsedTimer m_elapsedTimer;
    qint64 m_lastTriggerTime = 0;
    RawFileInputFormat m_sampleFormat;
    qint64 m_bytesRead;
};

class RawFileInput : public InputDevice
{
    Q_OBJECT
public:
    explicit RawFileInput(QObject *parent = nullptr);
    ~RawFileInput();
    bool openDevice(const QVariant &hwId = QVariant(), bool fallbackConnection = true) override;
    void tune(uint32_t freq) override;
    InputDevice::Capabilities capabilities() const override { return {}; }
    void setFile(const QString &fileName, const RawFileInputFormat &sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8);
    void setFileFormat(const RawFileInputFormat &sampleFormat);
    void startStopRecording(bool start) override { /* do nothing */ }
    void seek(int msec);

signals:
    void fileLength(int msec);
    void fileProgress(int msec);

private:
    RawFileInputFormat m_sampleFormat;
    QString m_fileName;
    QFile *m_inputFile = nullptr;
    RawFileWorker *m_worker = nullptr;
    QTimer *m_inputTimer = nullptr;
    QTimer m_watchdogTimer;
    void stop();
    void rewind();
    void onBytesRead(qint64 bytesRead);
    void onWatchdogTimeout();
    void onEndOfFile(bool status) { emit error(status ? InputDevice::ErrorCode::EndOfFile : InputDevice::ErrorCode::NoDataAvailable); }
    void parseXmlHeader(const QByteArray &xml);
};

#endif  // RAWFILEINPUT_H
