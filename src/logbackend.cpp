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

#include "androidfilehelper.h"
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
    auto fileName = QString("%2_%3.log").arg(QCoreApplication::applicationName(), QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));

#if ASK_FOR_PERMISSION_IF_NEEDED
    std::function<void(const QString &)> callback = [=](const QString &logPath)
    {
        if (logPath.isEmpty())
        {
            qCWarning(application) << "Error creating log export directory:" << AndroidFileHelper::instance().lastError();
            return;
        }

        QFile *logFile = AndroidFileHelper::instance().openFileForWriting(logPath, fileName, "text/plain");
        if (logFile)
        {
            QTextStream stream(logFile);
            for (int n = 0; n < m_logModel->rowCount(); ++n)
            {
                stream << m_logModel->data(m_logModel->index(n, 0)).toString() << Qt::endl;
            }
            stream.flush();
            logFile->close();
            delete logFile;

            qCInfo(application) << "Log file exported: " << QString("%1/%2").arg(logPath, fileName);
        }
        else
        {
            qCCritical(application) << "Unable to open file: " << QString("%1/%2").arg(logPath, fileName);
        }
    };
    AndroidFileHelper::instance().accessPath(m_settings->dataStoragePath, QString{}, callback);
#else
    const QString logPath = AndroidFileHelper::instance().getPath(m_settings->dataStoragePath, QString{});
    if (logPath.isEmpty())
    {
        qCWarning(application) << "Error creating log export directory:" << AndroidFileHelper::instance().lastError();
        return;
    }

    QFile *logFile = AndroidFileHelper::instance().openFileForWriting(logPath, fileName, "text/plain");
    if (logFile)
    {
        QTextStream stream(logFile);
        for (int n = 0; n < m_logModel->rowCount(); ++n)
        {
            stream << m_logModel->data(m_logModel->index(n, 0)).toString() << Qt::endl;
        }
        stream.flush();
        logFile->close();
        delete logFile;

        qCInfo(application) << "Log file exported: " << QString("%1/%2").arg(m_settings->dataStoragePath, fileName);
    }
    else
    {
        qCCritical(application) << "Unable to open file: " << QString("%1/%2").arg(m_settings->dataStoragePath, fileName);
    }
#endif
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
