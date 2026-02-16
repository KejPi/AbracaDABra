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

#ifndef LOGBACKEND_H
#define LOGBACKEND_H

#include <QFontDatabase>

#include "logmodel.h"

class Settings;
class LogBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(LogModel *logModel READ logModel CONSTANT FINAL)

public:
    explicit LogBackend(Settings *settings, QObject *parent = nullptr);
    ~LogBackend();
    void setupDarkMode(bool darkModeEna);
    LogModel *logModel() const { return m_logModel; }

    Q_INVOKABLE void copyToClipboard();
    Q_INVOKABLE void clearLog() { m_logModel->clear(); }
    Q_INVOKABLE void saveLogToFile();

private:
    LogModel *m_logModel;
    Settings *m_settings = nullptr;
};

#endif  // LOGBACKEND_H
