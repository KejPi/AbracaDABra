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

#include "logbackend.h"

#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QString>
#include <QUrl>

#include "settings.h"

Q_DECLARE_LOGGING_CATEGORY(application)

LogBackend::LogBackend(Settings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    m_logModel = new LogModel(this);
}

LogBackend::~LogBackend()
{
    delete m_logModel;
}

void LogBackend::setupDarkMode(bool darkModeEna)
{
    m_logModel->setupDarkMode(darkModeEna);
}

void LogBackend::saveLogToFile()
{
    auto localFile =
        QString("%1/%2_%3.log")
            .arg(m_settings->dataStoragePath, QCoreApplication::applicationName(), QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));
    QFile logFile(localFile);
    if (!logFile.open(QIODevice::WriteOnly))
    {
        qCCritical(application) << "Unable to open file: " << localFile;
        return;
    }

    qCInfo(application) << "Writing log to:" << localFile;

    QTextStream stream(&logFile);
    for (int n = 0; n < m_logModel->rowCount(); ++n)
    {
        stream << m_logModel->data(m_logModel->index(n, 0)).toString() << Qt::endl;
    }
    stream.flush();
    logFile.close();
}

void LogBackend::copyToClipboard()
{
    QString logText("");
    QTextStream stream(&logText);
    for (int n = 0; n < m_logModel->rowCount(); ++n)
    {
        stream << m_logModel->data(m_logModel->index(n, 0)).toString() << Qt::endl;
    }
    stream.flush();

    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(logText);
}
