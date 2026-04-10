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

#ifndef ANDROIDFILEHELPER_H
#define ANDROIDFILEHELPER_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <cstdio>

/**
 * @brief Helper class for Android Storage Access Framework (SAF) file operations.
 *
 * On Android, when users select external storage locations (like Documents folder)
 * through the system folder picker, the application receives content:// URIs instead
 * of regular file paths. These URIs require special handling through the SAF APIs.
 *
 * This class provides a unified interface for file operations that works both on
 * desktop platforms (using regular file paths) and on Android (using SAF for
 * content:// URIs and regular file operations for app-private storage).
 */
class AndroidFileHelper
{
public:
    /**
     * @brief Check if a path is an Android content URI that requires SAF handling.
     * @param path The path to check
     * @return true if the path is a content:// URI, false otherwise
     */
    static bool isContentUri(const QString &path);

    /**
     * @brief Take persistable URI permission for a tree URI.
     *
     * On Android, when a folder is selected via the folder picker, we need to
     * explicitly take persistable permission with both read and write flags.
     * This should be called immediately after folder selection.
     *
     * @param treeUri The tree URI from the folder picker
     * @return true if permission was successfully taken, false otherwise
     */
    static bool takePersistablePermission(const QString &treeUri);

    /**
     * @brief Check if the application has write permission to the given URI.
     *
     * This method is only relevant for Android content:// URIs. For regular
     * file paths, it always returns true (actual permission check happens at write time).
     *
     * @param treeUri The tree URI to check (typically from folder picker)
     * @return true if write permission is available, false otherwise
     */
    static bool hasWritePermission(const QString &treeUri);

    /**
     * @brief Create nested directories under a base path/URI.
     *
     * For regular file paths this mirrors QDir::mkpath(). For Android SAF
     * content:// URIs it will ensure the tree is writable and create the
     * subdirectory structure using SAF APIs.
     *
     * @param basePath Base directory or tree URI (e.g. dataStoragePath)
     * @param relativePath Relative subdirectory path to create (e.g. "ensemble" or "a/b/c")
     * @return true if the directory exists or was created, false otherwise
     */
    static bool mkpath(const QString &basePath, const QString &relativePath = QString());

    /**     * @brief Build a subdirectory path by appending a subdirectory to a base path.
     *
     * On Android with content:// URIs, this properly handles percent-encoding of the
     * subdirectory and URI separator encoding. For regular file paths, uses standard
     * path concatenation with forward slashes.
     *
     * @param basePath The base path/URI (may be content:// URI or file system path)
     * @param subdir The subdirectory name to append (without leading/trailing slashes)
     * @return The combined path with proper encoding for content URIs or standard path separator for files
     */
    static QString buildSubdirPath(const QString &basePath, const QString &subdir);

    /**     * @brief Write text content to a file.
     *
     * On Android with content:// URIs, this uses SAF APIs. On desktop or with
     * regular Android file paths, it uses standard Qt file operations.
     *
     * @param basePath The base path/URI where the file should be created
     * @param fileName The name of the file to create
     * @param content The text content to write
     * @param mimeType The MIME type of the file (e.g., "text/csv", "text/plain")
     * @param overwriteExisting If true, overwrites existing files; if false, fails when file exists
     * @return true if the file was written successfully, false otherwise
     */
    static bool writeTextFile(const QString &basePath, const QString &fileName, const QString &content, const QString &mimeType = "text/plain",
                              bool overwriteExisting = true);

    /**
     * @brief Write binary content to a file.
     *
     * On Android with content:// URIs, this uses SAF APIs. On desktop or with
     * regular Android file paths, it uses standard Qt file operations.
     *
     * @param basePath The base path/URI where the file should be created
     * @param fileName The name of the file to create
     * @param data The binary data to write
     * @param mimeType The MIME type of the file (e.g., "image/jpeg", "application/octet-stream")
     * @param overwriteExisting If true, overwrites existing files; if false, fails when file exists
     * @return true if the file was written successfully, false otherwise
     */
    static bool writeBinaryFile(const QString &basePath, const QString &fileName, const QByteArray &data,
                                const QString &mimeType = "application/octet-stream", bool overwriteExisting = true);

    /**
     * @brief Open a file for continuous writing.
     *
     * On Android with content:// URIs, this uses SAF APIs to create the file
     * and returns a QFile object that wraps the native file descriptor.
     * On desktop or with regular Android file paths, it uses standard Qt file operations.
     *
     * The caller takes ownership of the returned QFile and is responsible for
     * closing and deleting it when done.
     *
     * @param basePath The base path/URI where the file should be created
     * @param fileName The name of the file to create
     * @param mimeType The MIME type of the file (e.g., "text/csv")
     * @return QFile pointer opened for writing, or nullptr on failure
     */
    static QFile *openFileForWriting(const QString &basePath, const QString &fileName, const QString &mimeType = "text/plain");

    /**
     * @brief Open a file for continuous binary writing using FILE* (C stdio).
     *
     * This method is optimized for high-throughput binary writing where the overhead
     * of QFile's virtual calls may be undesirable (e.g., recording raw IQ samples).
     *
     * On Android with content:// URIs, this uses SAF APIs to create the file
     * and returns a FILE* via fdopen() on the native file descriptor.
     * On desktop or with regular Android file paths, it uses standard fopen().
     *
     * The caller takes ownership of the returned FILE* and is responsible for
     * calling fclose() when done.
     *
     * @param basePath The base path/URI where the file should be created
     * @param fileName The name of the file to create
     * @param mimeType The MIME type of the file (e.g., "application/octet-stream")
     * @return FILE* opened for binary writing ("wb"), or nullptr on failure
     */
    static FILE *openFileForWritingRaw(const QString &basePath, const QString &fileName, const QString &mimeType = "application/octet-stream");

    /**
     * @brief Get a human-readable error message for the last failed operation.
     * @return Error message string, or empty string if no error occurred
     */
    static QString lastError();

private:
    static QString s_lastError;

#ifdef Q_OS_ANDROID
    /**
     * @brief Write content using Android SAF APIs (JNI call to Java helper).
     */
    static bool writeUsingSAF(const QString &treeUri, const QString &fileName, const QString &content, const QString &mimeType,
                              bool overwriteExisting);

    /**
     * @brief Write binary content using Android SAF APIs (JNI call to Java helper).
     */
    static bool writeBinaryUsingSAF(const QString &treeUri, const QString &fileName, const QByteArray &data, const QString &mimeType,
                                    bool overwriteExisting);
#endif
};

#endif  // ANDROIDFILEHELPER_H
