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

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QObject>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>

#include "config.h"

#if HAVE_MP3LAME
#include "mp3encoder.h"
#endif

class DabCliApp;
class AudioStreamer;

// Minimal single-purpose HTTP/1.1 server (no keep-alive, no chunked requests) that exposes
// dashboard: ensemble/service status as JSON, tune/service-selection
// control endpoints, and a live audio stream of the currently selected service.
class WebServer : public QObject
{
    Q_OBJECT
public:
    explicit WebServer(DabCliApp *app, AudioStreamer *streamer, QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onAudioChunk(const QByteArray &pcm, int sampleRate, int numChannels);

private:
    struct ClientState
    {
        QByteArray buffer;
        bool isStreamClient = false;

        // ICY (SHOUTcast/Icecast-style) inline metadata, requested via the "Icy-MetaData: 1"
        // request header, so external players (VLC, foobar2000, ...) can show title/artist.
        bool icyMetadata = false;
        int icyMetaInt = 0;
        int bytesSinceMeta = 0;
        QString lastIcyTitle;
    };

    QTcpServer *m_server;
    DabCliApp *m_app;
    AudioStreamer *m_streamer;
    QHash<QTcpSocket *, ClientState> m_clients;
    QSet<QTcpSocket *> m_streamClients;

#if HAVE_MP3LAME
    // Encoding happens once per incoming PCM chunk and the resulting MP3 bytes are broadcast to
    // every connected stream client, rather than encoding separately per client.
    Mp3Encoder m_mp3Encoder;
#endif

    void tryProcessRequest(QTcpSocket *socket);
    void handleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body,
                        const QHash<QByteArray, QByteArray> &headers);

    void sendResponse(QTcpSocket *socket, int status, const QString &statusText, const QByteArray &contentType, const QByteArray &body,
                       bool closeAfter = true);
    void sendJson(QTcpSocket *socket, const QJsonDocument &doc, int status = 200);
    void sendError(QTcpSocket *socket, int status, const QString &statusText, const QString &message);
    void startStreamClient(QTcpSocket *socket, bool icyMetadataRequested);
    void writeStreamData(QTcpSocket *socket, ClientState &state, const QByteArray &pcm);
    QByteArray buildIcyMetaBlock(ClientState &state);
    static QString indexHtml();
};

#endif  // WEBSERVER_H
