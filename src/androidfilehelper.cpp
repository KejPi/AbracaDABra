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

#include "androidfilehelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTextStream>
#include <QUrl>
#include <cerrno>
#include <cstring>

#ifdef Q_OS_ANDROID
#include <unistd.h>

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

Q_DECLARE_LOGGING_CATEGORY(application)

bool AndroidFileHelper::isContentUri(const QString &path)
{
    return path.startsWith("content://");
}

bool AndroidFileHelper::takePersistablePermission(const QString &treeUri)
{
    if (!isContentUri(treeUri))
    {
        // Not a content URI, no permission needed
        return true;
    }

#ifdef Q_OS_ANDROID
    QJniObject jTreeUri = QJniObject::fromString(treeUri);
    QJniObject context = QNativeInterface::QAndroidApplication::context();

    jboolean result = QJniObject::callStaticMethod<jboolean>("org/qtproject/abracadabra/FileHelper", "takePersistablePermission",
                                                             "(Landroid/content/Context;Ljava/lang/String;)Z", context.object<jobject>(),
                                                             jTreeUri.object<jstring>());

    if (!result)
    {
        m_lastError = "Failed to take persistable permission for the selected folder.";
        qCWarning(application) << m_lastError;
    }
    else
    {
        qCInfo(application) << "Took persistable permission for:" << treeUri;
    }
    return result;
#else
    return true;
#endif
}

void AndroidFileHelper::accessPath(const QString &basePath, const QString &relativePath, std::function<void(const QString &)> callback)
{
    m_relativePath = relativePath;
    m_callback = callback;

    if (isContentUri(basePath) && !hasWritePermission(basePath))
    {
        // we need to ask for permissions here
        emit requestPermissions(basePath);
        return;
    }
    pathGranted(basePath);
}

void AndroidFileHelper::pathGranted(const QString &basePath)
{
    if (!m_callback)
    {
        return;
    }

    if (basePath.isEmpty() == false)
    {
        QString path = buildSubdirPath(basePath, m_relativePath);
        if (!mkpath(basePath, m_relativePath))
        {
            qCCritical(application) << "Failed to create directory:" << lastError();
            m_lastError = "Failed to create path: " + path;
            m_callback(QString{});
            return;
        }

        if (!hasWritePermission(path))
        {
            qCCritical(application) << "No permission to write to:" << path;
            qCCritical(application) << "Please select a new data storage folder in settings.";
            m_lastError = "No permission to write path: " + path;
            m_callback(QString{});
            return;
        }

        // success
        m_callback(path);
    }
    else
    {  // error
        m_callback(QString{});
    }
    m_callback = nullptr;
}

bool AndroidFileHelper::hasWritePermission(const QString &treeUri)
{
    if (!isContentUri(treeUri))
    {
        // For regular file paths, permission is determined at write time
        return true;
    }

#ifdef Q_OS_ANDROID
    QJniObject jTreeUri = QJniObject::fromString(treeUri);
    QJniObject context = QNativeInterface::QAndroidApplication::context();

    jboolean result = QJniObject::callStaticMethod<jboolean>("org/qtproject/abracadabra/FileHelper", "hasPermission",
                                                             "(Landroid/content/Context;Ljava/lang/String;)Z", context.object<jobject>(),
                                                             jTreeUri.object<jstring>());

    if (!result)
    {
        m_lastError = "No write permission for the selected folder. Please select a new data storage folder in settings.";
    }
    return result;
#else
    return true;
#endif
}

QString AndroidFileHelper::getPath(const QString &basePath, const QString &relativePath)
{
    const QString path = AndroidFileHelper::instance().buildSubdirPath(basePath, relativePath);

    // check if the path is a content URI and if we have permission
    if (isContentUri(basePath) && !hasWritePermission(basePath))
    {
        qCCritical(application) << "No permission to write to:" << basePath;
        qCCritical(application) << "Please select a new data storage folder in settings.";
        m_lastError = "No permission to write path: " + basePath;
        return QString{};
    }

    // Ensure directory exists and is writable
    if (!mkpath(basePath, relativePath))
    {
        qCCritical(application) << "Failed to create directory:" << lastError();
        m_lastError = "Failed to create path: " + path;
        return QString{};
    }

    if (!hasWritePermission(path))
    {
        qCCritical(application) << "No permission to write to:" << path;
        qCCritical(application) << "Please select a new data storage folder in settings.";
        m_lastError = "No permission to write path: " + path;
        return QString{};
    }
    return path;
}

bool AndroidFileHelper::mkpath(const QString &basePath, const QString &relativePath)
{
    m_lastError.clear();

#ifdef Q_OS_ANDROID
    if (isContentUri(basePath))
    {
        // Ensure we have permission first
        if (!hasWritePermission(basePath))
        {
            return false;
        }

        QString normalized = relativePath;
        if (normalized.startsWith('/'))
        {
            normalized = normalized.mid(1);
        }

        QJniObject jBasePath = QJniObject::fromString(basePath);
        QJniObject jSubPath = QJniObject::fromString(normalized);
        QJniObject context = QNativeInterface::QAndroidApplication::context();

        jboolean result = QJniObject::callStaticMethod<jboolean>("org/qtproject/abracadabra/FileHelper", "makeDirectories",
                                                                 "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z",
                                                                 context.object<jobject>(), jBasePath.object<jstring>(), jSubPath.object<jstring>());

        if (!result)
        {
            m_lastError = QString("Failed to create directories: %1/%2").arg(basePath, normalized);
            qCWarning(application) << m_lastError;
        }
        return result;
    }
#endif

    // Standard file system handling
    QString target = basePath;
    if (!relativePath.isEmpty())
    {
        if (target.endsWith('/'))
        {
            target.chop(1);
        }
        target += "/" + relativePath;
    }

    if (!QDir().mkpath(target))
    {
        m_lastError = QString("Failed to create directory: %1").arg(target);
        qCWarning(application) << m_lastError;
        return false;
    }

    return true;
}

QString AndroidFileHelper::buildSubdirPath(const QString &basePath, const QString &subdir)
{
    if (isContentUri(basePath))
    {
        QString encoded = QUrl::toPercentEncoding(subdir, "", "/");
        return basePath.endsWith('%') ? basePath + "2F" + encoded : basePath + "%2F" + encoded;
    }
    return basePath.endsWith('/') ? basePath + subdir : basePath + "/" + subdir;
}

bool AndroidFileHelper::writeTextFile(const QString &basePath, const QString &fileName, const QString &content, const QString &mimeType,
                                      bool overwriteExisting)
{
    m_lastError.clear();

#ifdef Q_OS_ANDROID
    if (isContentUri(basePath))
    {
        return writeUsingSAF(basePath, fileName, content, mimeType, overwriteExisting);
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!overwriteExisting && QFile::exists(fullPath))
    {
        m_lastError = QString("File already exists: %1").arg(fullPath);
        qCWarning(application) << m_lastError;
        return false;
    }

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        m_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(application) << m_lastError;
        return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file.errorString());
        qCWarning(application) << m_lastError;
        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    qCInfo(application) << "File written successfully:" << fullPath;
    return true;
}

bool AndroidFileHelper::writeBinaryFile(const QString &basePath, const QString &fileName, const QByteArray &data, const QString &mimeType,
                                        bool overwriteExisting)
{
    m_lastError.clear();

#ifdef Q_OS_ANDROID
    if (isContentUri(basePath))
    {
        return writeBinaryUsingSAF(basePath, fileName, data, mimeType, overwriteExisting);
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!overwriteExisting && QFile::exists(fullPath))
    {
        m_lastError = QString("File already exists: %1").arg(fullPath);
        qCWarning(application) << m_lastError;
        return false;
    }

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        m_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(application) << m_lastError;
        return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file.errorString());
        qCWarning(application) << m_lastError;
        return false;
    }

    file.write(data);
    file.close();

    qCInfo(application) << "File written successfully:" << fullPath;
    return true;
}

QFile *AndroidFileHelper::openFileForWriting(const QString &basePath, const QString &fileName, const QString &mimeType)
{
    m_lastError.clear();

#ifdef Q_OS_ANDROID
    if (isContentUri(basePath))
    {
        QJniObject jBasePath = QJniObject::fromString(basePath);
        QJniObject jFileName = QJniObject::fromString(fileName);
        QJniObject jMimeType = QJniObject::fromString(mimeType);
        QJniObject context = QNativeInterface::QAndroidApplication::context();

        jint fd = QJniObject::callStaticMethod<jint>("org/qtproject/abracadabra/FileHelper", "openFileForWriting",
                                                     "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                                     context.object<jobject>(), jBasePath.object<jstring>(), jFileName.object<jstring>(),
                                                     jMimeType.object<jstring>());

        if (fd < 0)
        {
            m_lastError = QString("Failed to open file via SAF: %1/%2").arg(basePath, fileName);
            qCWarning(application) << m_lastError;
            return nullptr;
        }

        // Create QFile from native file descriptor
        QFile *file = new QFile();
        if (!file->open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle))
        {
            m_lastError = QString("Failed to wrap file descriptor: %1").arg(file->errorString());
            qCWarning(application) << m_lastError;
            ::close(fd);
            delete file;
            return nullptr;
        }

        qCInfo(application) << "Opened file for writing via SAF:" << basePath << "/" << fileName;
        return file;
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        m_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(application) << m_lastError;
        return nullptr;
    }

    QFile *file = new QFile(fullPath);
    if (!file->open(QIODevice::WriteOnly))
    {
        m_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file->errorString());
        qCWarning(application) << m_lastError;
        delete file;
        return nullptr;
    }

    qCInfo(application) << "Opened file for writing:" << fullPath;
    return file;
}

FILE *AndroidFileHelper::openFileForWritingRaw(const QString &basePath, const QString &fileName, const QString &mimeType)
{
    m_lastError.clear();

#ifdef Q_OS_ANDROID
    if (isContentUri(basePath))
    {
        QJniObject jBasePath = QJniObject::fromString(basePath);
        QJniObject jFileName = QJniObject::fromString(fileName);
        QJniObject jMimeType = QJniObject::fromString(mimeType);
        QJniObject context = QNativeInterface::QAndroidApplication::context();

        jint fd = QJniObject::callStaticMethod<jint>("org/qtproject/abracadabra/FileHelper", "openFileForWriting",
                                                     "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                                     context.object<jobject>(), jBasePath.object<jstring>(), jFileName.object<jstring>(),
                                                     jMimeType.object<jstring>());

        if (fd < 0)
        {
            m_lastError = QString("Failed to open file via SAF: %1/%2").arg(basePath, fileName);
            qCWarning(application) << m_lastError;
            return nullptr;
        }

        // Wrap file descriptor in FILE* using fdopen
        FILE *file = fdopen(fd, "wb");
        if (file == nullptr)
        {
            m_lastError = QString("Failed to fdopen file descriptor: %1").arg(strerror(errno));
            qCWarning(application) << m_lastError;
            ::close(fd);
            return nullptr;
        }

        qCInfo(application) << "Opened file for raw writing via SAF:" << basePath << "/" << fileName;
        return file;
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        m_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(application) << m_lastError;
        return nullptr;
    }

    FILE *file = fopen(QDir::toNativeSeparators(fullPath).toUtf8().data(), "wb");
    if (file == nullptr)
    {
        m_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, strerror(errno));
        qCWarning(application) << m_lastError;
        return nullptr;
    }

    qCInfo(application) << "Opened file for raw writing:" << fullPath;
    return file;
}

QString AndroidFileHelper::lastError() const
{
    return m_lastError;
}

#ifdef Q_OS_ANDROID
bool AndroidFileHelper::writeUsingSAF(const QString &treeUri, const QString &fileName, const QString &content, const QString &mimeType,
                                      bool overwriteExisting)
{
    QJniObject jTreeUri = QJniObject::fromString(treeUri);
    QJniObject jFileName = QJniObject::fromString(fileName);
    QJniObject jMimeType = QJniObject::fromString(mimeType);
    QJniObject jContent = QJniObject::fromString(content);
    QJniObject context = QNativeInterface::QAndroidApplication::context();

    jboolean success = QJniObject::callStaticMethod<jboolean>(
        "org/qtproject/abracadabra/FileHelper", "writeFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Z", context.object<jobject>(),
        jTreeUri.object<jstring>(), jFileName.object<jstring>(), jMimeType.object<jstring>(), jContent.object<jstring>(), overwriteExisting);

    if (success)
    {
        qCInfo(application) << "File written via SAF:" << treeUri << "/" << fileName;
    }
    else
    {
        m_lastError = QString("Failed to write file via SAF: %1/%2").arg(treeUri, fileName);
        qCWarning(application) << m_lastError;
    }

    return success;
}

bool AndroidFileHelper::writeBinaryUsingSAF(const QString &treeUri, const QString &fileName, const QByteArray &data, const QString &mimeType,
                                            bool overwriteExisting)
{
    QJniObject jTreeUri = QJniObject::fromString(treeUri);
    QJniObject jFileName = QJniObject::fromString(fileName);
    QJniObject jMimeType = QJniObject::fromString(mimeType);
    QJniObject context = QNativeInterface::QAndroidApplication::context();

    // Convert QByteArray to Java byte array
    jbyteArray jData = QJniEnvironment().jniEnv()->NewByteArray(data.size());
    QJniEnvironment().jniEnv()->SetByteArrayRegion(jData, 0, data.size(), reinterpret_cast<const jbyte *>(data.constData()));

    jboolean success = QJniObject::callStaticMethod<jboolean>("org/qtproject/abracadabra/FileHelper", "writeBinaryFile",
                                                              "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;[BLjava/lang/String;Z)Z",
                                                              context.object<jobject>(), jTreeUri.object<jstring>(), jFileName.object<jstring>(),
                                                              jData, jMimeType.object<jstring>(), overwriteExisting);

    QJniEnvironment().jniEnv()->DeleteLocalRef(jData);

    if (success)
    {
        qCInfo(application) << "Binary file written via SAF:" << treeUri << "/" << fileName;
    }
    else
    {
        m_lastError = QString("Failed to write binary file via SAF: %1/%2").arg(treeUri, fileName);
        qCWarning(application) << m_lastError;
    }

    return success;
}
#endif
