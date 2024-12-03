/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDebug>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include "updatechecker.h"


Q_LOGGING_CATEGORY(updater, "UpdateCheck", QtWarningMsg)


UpdateChecker::UpdateChecker(QObject *parent)
    : QObject{parent}
{}

UpdateChecker::~UpdateChecker()
{
    qDebug() << Q_FUNC_INFO;
    if (m_netAccessManager != nullptr)
    {
        m_netAccessManager->deleteLater();
    }
}

void UpdateChecker::check()
{
    if (m_netAccessManager == nullptr)
    {
        m_netAccessManager = new QNetworkAccessManager();
#ifdef QT_DEBUG
        connect(m_netAccessManager, &QNetworkAccessManager::sslErrors, this, [this](QNetworkReply *reply, const QList<QSslError> &errors) {
            for (const auto & e : errors)
            {
                qDebug() << e;
            }
        });
#endif
        connect(m_netAccessManager, &QNetworkAccessManager::finished, this, &UpdateChecker::onFileDownloaded);
        connect(m_netAccessManager, &QNetworkAccessManager::destroyed, this, [this]() {
            m_netAccessManager = nullptr;
        });

        QNetworkRequest request;
        request.setUrl(QUrl("https://api.github.com/repos/kejpi/AbracaDABra/releases/latest"));

        QSslConfiguration sslConfiguration = QSslConfiguration::defaultConfiguration();
        sslConfiguration.setCaCertificates(QSslConfiguration::systemCaCertificates());
        sslConfiguration.setProtocol(QSsl::AnyProtocol);
        request.setSslConfiguration(sslConfiguration);
        request.setTransferTimeout(10*1000);
        m_netAccessManager->get(request);
    }
}

QString UpdateChecker::version() const
{
    return m_version;
}

bool UpdateChecker::isPreRelease() const
{
    return m_isPreRelease;
}

QString UpdateChecker::releaseNotes() const
{
    return m_releaseNotes;
}

void UpdateChecker::onFileDownloaded(QNetworkReply *reply)
{
    bool result = false;
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();
        if (!data.isEmpty())
        {
            result = parseResponse(data);
        }
    }
    else
    {
        qCWarning(updater) << reply->errorString();
    }
    reply->deleteLater();
    m_netAccessManager->deleteLater();

    emit finished(result);
}

bool UpdateChecker::parseResponse(const QByteArray &data)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isNull() && jsonDoc.isObject())
    {
        auto value = jsonDoc.object().value("tag_name");
        if (!value.isUndefined())
        {
            m_version = value.toString("");
        }

        value = jsonDoc.object().value("prerelease");
        if (!value.isUndefined())
        {
            m_isPreRelease = value.toBool(false);
        }

        value = jsonDoc.object().value("body");
        if (!value.isUndefined())
        {
            m_releaseNotes = value.toString("");
        }

        return true;
    }
    qCWarning(updater) << "Failed to parse server response";
    return false;
}
