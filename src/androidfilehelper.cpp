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

Q_LOGGING_CATEGORY(fileHelper, "fileHelper", QtInfoMsg)

QString AndroidFileHelper::s_lastError;

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
        s_lastError = "Failed to take persistable permission for the selected folder.";
        qCWarning(fileHelper) << s_lastError;
    }
    else
    {
        qCInfo(fileHelper) << "Took persistable permission for:" << treeUri;
    }
    return result;
#else
    return true;
#endif
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
        s_lastError = "No write permission for the selected folder. Please select a new data storage folder in settings.";
    }
    return result;
#else
    return true;
#endif
}

bool AndroidFileHelper::mkpath(const QString &basePath, const QString &relativePath)
{
    s_lastError.clear();

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
            s_lastError = QString("Failed to create directories: %1/%2").arg(basePath, normalized);
            qCWarning(fileHelper) << s_lastError;
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
        s_lastError = QString("Failed to create directory: %1").arg(target);
        qCWarning(fileHelper) << s_lastError;
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
    s_lastError.clear();

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
        s_lastError = QString("File already exists: %1").arg(fullPath);
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        s_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        s_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file.errorString());
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    qCInfo(fileHelper) << "File written successfully:" << fullPath;
    return true;
}

bool AndroidFileHelper::writeBinaryFile(const QString &basePath, const QString &fileName, const QByteArray &data, const QString &mimeType,
                                        bool overwriteExisting)
{
    s_lastError.clear();

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
        s_lastError = QString("File already exists: %1").arg(fullPath);
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        s_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        s_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file.errorString());
        qCWarning(fileHelper) << s_lastError;
        return false;
    }

    file.write(data);
    file.close();

    qCInfo(fileHelper) << "File written successfully:" << fullPath;
    return true;
}

QFile *AndroidFileHelper::openFileForWriting(const QString &basePath, const QString &fileName, const QString &mimeType)
{
    s_lastError.clear();

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
            s_lastError = QString("Failed to open file via SAF: %1/%2").arg(basePath, fileName);
            qCWarning(fileHelper) << s_lastError;
            return nullptr;
        }

        // Create QFile from native file descriptor
        QFile *file = new QFile();
        if (!file->open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle))
        {
            s_lastError = QString("Failed to wrap file descriptor: %1").arg(file->errorString());
            qCWarning(fileHelper) << s_lastError;
            ::close(fd);
            delete file;
            return nullptr;
        }

        qCInfo(fileHelper) << "Opened file for writing via SAF:" << basePath << "/" << fileName;
        return file;
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        s_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(fileHelper) << s_lastError;
        return nullptr;
    }

    QFile *file = new QFile(fullPath);
    if (!file->open(QIODevice::WriteOnly))
    {
        s_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, file->errorString());
        qCWarning(fileHelper) << s_lastError;
        delete file;
        return nullptr;
    }

    qCInfo(fileHelper) << "Opened file for writing:" << fullPath;
    return file;
}

FILE *AndroidFileHelper::openFileForWritingRaw(const QString &basePath, const QString &fileName, const QString &mimeType)
{
    s_lastError.clear();

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
            s_lastError = QString("Failed to open file via SAF: %1/%2").arg(basePath, fileName);
            qCWarning(fileHelper) << s_lastError;
            return nullptr;
        }

        // Wrap file descriptor in FILE* using fdopen
        FILE *file = fdopen(fd, "wb");
        if (file == nullptr)
        {
            s_lastError = QString("Failed to fdopen file descriptor: %1").arg(strerror(errno));
            qCWarning(fileHelper) << s_lastError;
            ::close(fd);
            return nullptr;
        }

        qCInfo(fileHelper) << "Opened file for raw writing via SAF:" << basePath << "/" << fileName;
        return file;
    }
#endif

    // Standard file system handling (desktop and Android app-private storage)
    QString fullPath = QString("%1/%2").arg(basePath, fileName);

    if (!QDir().mkpath(QFileInfo(fullPath).path()))
    {
        s_lastError = QString("Failed to create directory: %1").arg(QFileInfo(fullPath).path());
        qCWarning(fileHelper) << s_lastError;
        return nullptr;
    }

    FILE *file = fopen(QDir::toNativeSeparators(fullPath).toUtf8().data(), "wb");
    if (file == nullptr)
    {
        s_lastError = QString("Failed to open file for writing: %1 - %2").arg(fullPath, strerror(errno));
        qCWarning(fileHelper) << s_lastError;
        return nullptr;
    }

    qCInfo(fileHelper) << "Opened file for raw writing:" << fullPath;
    return file;
}

QString AndroidFileHelper::lastError()
{
    return s_lastError;
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
        qCInfo(fileHelper) << "File written via SAF:" << treeUri << "/" << fileName;
    }
    else
    {
        s_lastError = QString("Failed to write file via SAF: %1/%2").arg(treeUri, fileName);
        qCWarning(fileHelper) << s_lastError;
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
        qCInfo(fileHelper) << "Binary file written via SAF:" << treeUri << "/" << fileName;
    }
    else
    {
        s_lastError = QString("Failed to write binary file via SAF: %1/%2").arg(treeUri, fileName);
        qCWarning(fileHelper) << s_lastError;
    }

    return success;
}
#endif
