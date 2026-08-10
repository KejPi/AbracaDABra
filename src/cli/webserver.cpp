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

#include "webserver.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

#include "audiostreamer.h"
#include "config.h"
#include "dabcliapp.h"

Q_LOGGING_CATEGORY(webServer, "WebServer", QtInfoMsg)

WebServer::WebServer(DabCliApp *app, AudioStreamer *streamer, QObject *parent)
    : QObject(parent), m_app(app), m_streamer(streamer)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
    connect(m_streamer, &AudioStreamer::audioChunk, this, &WebServer::onAudioChunk, Qt::QueuedConnection);
}

bool WebServer::listen(const QHostAddress &address, quint16 port)
{
    return m_server->listen(address, port);
}

void WebServer::onNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *socket = m_server->nextPendingConnection();
        m_clients.insert(socket, ClientState());
        connect(socket, &QTcpSocket::readyRead, this, &WebServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebServer::onSocketDisconnected);
    }
}

void WebServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (nullptr == socket)
    {
        return;
    }
    auto it = m_clients.find(socket);
    if (it == m_clients.end())
    {
        return;
    }
    it->buffer.append(socket->readAll());
    tryProcessRequest(socket);
}

void WebServer::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (nullptr == socket)
    {
        return;
    }
    m_streamClients.remove(socket);
    m_clients.remove(socket);
    socket->deleteLater();
}

void WebServer::tryProcessRequest(QTcpSocket *socket)
{
    auto it = m_clients.find(socket);
    if (it == m_clients.end())
    {
        return;
    }
    ClientState &state = it.value();

    int headerEnd = state.buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        if (state.buffer.size() > 64 * 1024)
        {
            sendError(socket, 431, "Request Header Fields Too Large", "Request too large");
        }
        return;
    }

    QByteArray headerPart = state.buffer.left(headerEnd);
    QList<QByteArray> lines = headerPart.split('\n');
    if (lines.isEmpty())
    {
        sendError(socket, 400, "Bad Request", "Malformed request");
        return;
    }

    QList<QByteArray> requestParts = lines.at(0).trimmed().split(' ');
    if (requestParts.size() < 2)
    {
        sendError(socket, 400, "Bad Request", "Malformed request line");
        return;
    }
    QString method = QString::fromLatin1(requestParts.at(0)).toUpper();
    QString path = QString::fromLatin1(requestParts.at(1));

    qint64 contentLength = 0;
    QHash<QByteArray, QByteArray> headers;
    for (int i = 1; i < lines.size(); ++i)
    {
        QByteArray line = lines.at(i).trimmed();
        int colon = line.indexOf(':');
        if (colon <= 0)
        {
            continue;
        }
        QByteArray name = line.left(colon).trimmed().toLower();
        QByteArray value = line.mid(colon + 1).trimmed();
        headers.insert(name, value);
        if ("content-length" == name)
        {
            contentLength = value.toLongLong();
        }
    }

    int bodyStart = headerEnd + 4;
    if (state.buffer.size() - bodyStart < contentLength)
    {
        // wait for the rest of the body to arrive
        return;
    }

    QByteArray body = state.buffer.mid(bodyStart, int(contentLength));
    state.buffer.clear();

    handleRequest(socket, method, path, body, headers);
}

void WebServer::handleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body,
                               const QHash<QByteArray, QByteArray> &headers)
{
    QUrl url(path, QUrl::TolerantMode);
    QString p = url.path();

    if ("GET" == method && ("/" == p || p.isEmpty()))
    {
        sendResponse(socket, 200, "OK", "text/html; charset=utf-8", indexHtml().toUtf8());
        return;
    }

    if ("GET" == method && "/api/status" == p)
    {
        sendJson(socket, QJsonDocument(m_app->statusJson()));
        return;
    }

    if ("GET" == method && "/api/channels" == p)
    {
        QJsonObject o;
        o["channels"] = DabCliApp::channelsJson();
        sendJson(socket, QJsonDocument(o));
        return;
    }

    if ("POST" == method && "/api/tune" == p)
    {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (QJsonParseError::NoError != perr.error || !doc.isObject())
        {
            sendError(socket, 400, "Bad Request", "Invalid JSON body");
            return;
        }
        QJsonObject o = doc.object();
        QString err;
        bool ok;
        if (o.contains("channel"))
        {
            ok = m_app->requestTuneChannel(o.value("channel").toString(), &err);
        }
        else if (o.contains("frequencyKHz"))
        {
            ok = m_app->requestTuneFrequency(uint32_t(o.value("frequencyKHz").toDouble()), &err);
        }
        else
        {
            sendError(socket, 400, "Bad Request", "Expected 'channel' or 'frequencyKHz'");
            return;
        }
        if (!ok)
        {
            sendError(socket, 409, "Conflict", err);
            return;
        }
        QJsonObject res;
        res["ok"] = true;
        sendJson(socket, QJsonDocument(res));
        return;
    }

    if ("POST" == method && "/api/service" == p)
    {
        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (QJsonParseError::NoError != perr.error || !doc.isObject())
        {
            sendError(socket, 400, "Bad Request", "Invalid JSON body");
            return;
        }
        QJsonObject o = doc.object();
        uint32_t sid = 0;
        if (o.value("sid").isString())
        {
            sid = o.value("sid").toString().toUInt(nullptr, 0);  // accepts "0x..." or decimal
        }
        else
        {
            sid = uint32_t(o.value("sid").toDouble());
        }
        uint8_t scids = uint8_t(o.value("scids").toInt(0));

        QString err;
        if (!m_app->requestService(sid, scids, &err))
        {
            sendError(socket, 409, "Conflict", err);
            return;
        }
        QJsonObject res;
        res["ok"] = true;
        sendJson(socket, QJsonDocument(res));
        return;
    }

    if ("GET" == method && "/stream/audio" == p)
    {
        const bool icyRequested = ("1" == headers.value("icy-metadata"));
        startStreamClient(socket, icyRequested);
        return;
    }

    if ("GET" == method && "/api/slideshow" == p)
    {
        QByteArray data;
        QString contentType;
        if (!m_app->currentSlide(&data, &contentType))
        {
            sendError(socket, 404, "Not Found", "No slide available yet");
            return;
        }
        sendResponse(socket, 200, "OK", contentType.toLatin1(), data);
        return;
    }

    sendError(socket, 404, "Not Found", "Unknown endpoint: " + path);
}

void WebServer::sendResponse(QTcpSocket *socket, int status, const QString &statusText, const QByteArray &contentType, const QByteArray &body,
                              bool closeAfter)
{
    QByteArray header;
    header += "HTTP/1.1 " + QByteArray::number(status) + " " + statusText.toLatin1() + "\r\n";
    header += "Content-Type: " + contentType + "\r\n";
    header += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    header += "Access-Control-Allow-Origin: *\r\n";
    header += closeAfter ? "Connection: close\r\n" : "Connection: keep-alive\r\n";
    header += "\r\n";

    socket->write(header);
    socket->write(body);
    if (closeAfter)
    {
        socket->disconnectFromHost();
    }
}

void WebServer::sendJson(QTcpSocket *socket, const QJsonDocument &doc, int status)
{
    sendResponse(socket, status, status == 200 ? "OK" : "Error", "application/json", doc.toJson(QJsonDocument::Compact));
}

void WebServer::sendError(QTcpSocket *socket, int status, const QString &statusText, const QString &message)
{
    QJsonObject o;
    o["error"] = message;
    sendJson(socket, QJsonDocument(o), status);
}

void WebServer::startStreamClient(QTcpSocket *socket, bool icyMetadataRequested)
{
    int sampleRate = 48000;
    int numChannels = 2;
    m_app->currentAudioFormat(&sampleRate, &numChannels);

    QByteArray header;
    header += "HTTP/1.1 200 OK\r\n";
#if HAVE_MP3LAME
    // Real MP3 elementary stream, so it can be played back with a standard <audio> element
    // (and by any Icecast/SHOUTcast-aware client) instead of being manually decoded in JS.
    header += "Content-Type: audio/mpeg\r\n";
#else
    // Raw interleaved 16-bit LE PCM, not wrapped in a WAV container: browsers don't reliably
    // play an <audio> WAV stream of unknown/unbounded length, so the client instead reads this
    // via fetch()+ReadableStream and plays it through the Web Audio API.
    header += "Content-Type: application/octet-stream\r\n";
    header += "X-Audio-Sample-Rate: " + QByteArray::number(sampleRate) + "\r\n";
    header += "X-Audio-Channels: " + QByteArray::number(numChannels) + "\r\n";
#endif
    header += "Cache-Control: no-cache, no-store\r\n";
    header += "Access-Control-Allow-Origin: *\r\n";
    header += "icy-name: AbracaDABra-cli\r\n";

    ClientState &state = m_clients[socket];
    state.isStreamClient = true;
    // ICY (SHOUTcast/Icecast-style) inline metadata, so external players (VLC, foobar2000, ...)
    // can show the current DLS/DL Plus title/artist, similar to an Icecast source stream.
    if (icyMetadataRequested)
    {
        state.icyMetadata = true;
        state.icyMetaInt = 16000;
        state.bytesSinceMeta = 0;
        header += "icy-metaint: " + QByteArray::number(state.icyMetaInt) + "\r\n";
    }
    header += "Connection: close\r\n";
    header += "\r\n";
    socket->write(header);

    m_streamClients.insert(socket);
    qCInfo(webServer) << "Audio stream client connected, format" << sampleRate << "Hz" << numChannels << "ch, icy metadata"
                       << icyMetadataRequested;
}

QByteArray WebServer::buildIcyMetaBlock(ClientState &state)
{
    const QString title = m_app->currentStreamTitle();
    if (title == state.lastIcyTitle)
    {
        return QByteArray(1, char(0));  // unchanged since last interval -> zero-length block
    }
    state.lastIcyTitle = title;

    QString escaped = title;
    escaped.replace('\'', QStringLiteral("\\'"));
    QByteArray text = "StreamTitle='" + escaped.toUtf8() + "';";

    int numBlocks = std::min<int>((text.size() + 15) / 16, 255);
    QByteArray padded = text.leftJustified(numBlocks * 16, '\0', true);

    QByteArray out;
    out.append(char(numBlocks));
    out.append(padded);
    return out;
}

void WebServer::writeStreamData(QTcpSocket *socket, ClientState &state, const QByteArray &pcm)
{
    if (!state.icyMetadata)
    {
        socket->write(pcm);
        return;
    }

    int offset = 0;
    while (offset < pcm.size())
    {
        int chunkLen = std::min<int>(state.icyMetaInt - state.bytesSinceMeta, pcm.size() - offset);
        socket->write(pcm.constData() + offset, chunkLen);
        offset += chunkLen;
        state.bytesSinceMeta += chunkLen;
        if (state.bytesSinceMeta >= state.icyMetaInt)
        {
            socket->write(buildIcyMetaBlock(state));
            state.bytesSinceMeta = 0;
        }
    }
}

void WebServer::onAudioChunk(const QByteArray &pcm, int sampleRate, int numChannels)
{
    if (m_streamClients.isEmpty())
    {
        return;
    }

#if HAVE_MP3LAME
    if (!m_mp3Encoder.init(sampleRate, numChannels))
    {
        qCCritical(webServer) << "Failed to initialize MP3 encoder for" << sampleRate << "Hz" << numChannels << "ch";
        return;
    }
    const QByteArray data = m_mp3Encoder.encode(pcm);
    if (data.isEmpty())
    {
        return;  // LAME is still buffering internally; nothing to send yet
    }
#else
    const QByteArray &data = pcm;
#endif

    QList<QTcpSocket *> toRemove;
    for (QTcpSocket *socket : std::as_const(m_streamClients))
    {
        auto it = m_clients.find(socket);
        if (QAbstractSocket::ConnectedState != socket->state() || it == m_clients.end())
        {
            toRemove.append(socket);
            continue;
        }
        writeStreamData(socket, it.value(), data);
    }
    for (QTcpSocket *socket : std::as_const(toRemove))
    {
        m_streamClients.remove(socket);
    }
}

QString WebServer::indexHtml()
{
    QString html = QStringLiteral(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AbracaDABra CLI Web-UI</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: system-ui, sans-serif; background:#111; color:#eee; margin:0; padding:1.5rem; }
  h1 { font-size:1.3rem; margin:0 0 1rem 0; }
  .card { background:#1b1b1b; border:1px solid #333; border-radius:8px; padding:1rem; margin-bottom:1rem; }
  .grid { display:grid; grid-template-columns: repeat(auto-fit, minmax(160px,1fr)); gap:.75rem; }
  .stat label { display:block; font-size:.75rem; color:#999; text-transform:uppercase; letter-spacing:.05em; }
  .stat span { font-size:1.1rem; }
  table { width:100%; border-collapse: collapse; }
  th, td { text-align:left; padding:.4rem .5rem; border-bottom:1px solid #333; font-size:.9rem; }
  button, select { background:#2a2a2a; color:#eee; border:1px solid #444; border-radius:6px; padding:.4rem .7rem; cursor:pointer; }
  button:hover { background:#3a3a3a; }
  button.playing { background:#2e7d32; border-color:#2e7d32; }
  #player { width:100%; margin-top:.5rem; }
  .row { display:flex; gap:.5rem; align-items:center; flex-wrap:wrap; }
  .statusRow { display:flex; gap:1rem; align-items:flex-start; flex-wrap:wrap; }
  #slide { width:160px; height:160px; object-fit:contain; border-radius:6px; background:#222; border:1px solid #333; display:none; }
  #slide.visible { display:block; }
  details.card summary { cursor:pointer; font-weight:600; }
  details.card[open] summary { margin-bottom:.75rem; }
</style>
</head>
<body>
<h1>AbracaDABra &mdash; headless DAB/DAB+ receiver</h1>

<div class="card">
  <div class="row">
    <select id="channelSelect"></select>
    <button onclick="tune()">Tune</button>
  </div>
</div>

<div class="card statusRow">
  <img id="slide" alt="Station image">
  <div class="grid" style="flex:1">
    <div class="stat"><label>Ensemble</label><span id="ensemble">-</span></div>
    <div class="stat"><label>Channel</label><span id="channel">-</span></div>
    <div class="stat"><label>Frequency</label><span id="freq">-</span></div>
    <div class="stat"><label>Sync</label><span id="sync">-</span></div>
    <div class="stat"><label>SNR</label><span id="snr">-</span></div>
    <div class="stat"><label>Now playing</label><span id="nowplaying">-</span></div>
    <div class="stat"><label>Title</label><span id="title">-</span></div>
    <div class="stat"><label>DL Plus info</label><span id="dlPlusTags">-</span></div>
  </div>
</div>

<details class="card">
  <summary>Advanced / technical info</summary>
  <div class="grid">
    <div class="stat"><label>Freq. offset</label><span id="freqOffset">-</span></div>
    <div class="stat"><label>RF level</label><span id="rfLevel">-</span></div>
    <div class="stat"><label>Gain</label><span id="gain">-</span></div>
    <div class="stat"><label>Sub-ch bitrate</label><span id="advBitRate">-</span></div>
    <div class="stat"><label>FIB errors</label><span id="fibErrors">-</span></div>
    <div class="stat"><label>MSC CRC errors</label><span id="mscCrcErrors">-</span></div>
    <div class="stat"><label>RS bit errors</label><span id="rsBitErrors">-</span></div>
    <div class="stat"><label>RS uncorrectable</label><span id="rsUncorrectable">-</span></div>
  </div>
</details>

<div class="card">
  <table>
    <thead><tr><th>Service</th><th>Sub-ch</th><th>Bitrate</th><th></th></tr></thead>
    <tbody id="services"></tbody>
  </table>
  <div class="row">
    <span id="playerStatus">Not playing</span>
    <button onclick="stopPlayback()">Stop</button>
    <label for="volume">Volume</label>
    <input type="range" id="volume" min="0" max="100" value="100" oninput="setVolume(this.value)">
  </div>
  <audio id="player" style="display:none"></audio>
</div>

<script>
async function loadChannels(){
  const r = await fetch('/api/channels');
  const j = await r.json();
  const sel = document.getElementById('channelSelect');
  sel.innerHTML = '';
  j.channels.forEach(c => {
    const opt = document.createElement('option');
    opt.value = c.channel;
    opt.textContent = c.channel + ' (' + (c.frequencyKHz/1000).toFixed(3) + ' MHz)';
    sel.appendChild(opt);
  });
}
async function tune(){
  const ch = document.getElementById('channelSelect').value;
  await fetch('/api/tune', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({channel: ch})});
}

let lastSlideVersion = 0;
)HTML");

#if HAVE_MP3LAME
    // The stream is a real MP3 elementary stream, so it is played back through a standard
    // <audio> element instead of being manually decoded. This is what makes browsers treat the
    // page as genuine media playback
    html += QStringLiteral(R"HTML(
const player = document.getElementById('player');
let lastSid = null;
let lastScids = 0;

if ('mediaSession' in navigator) {
  navigator.mediaSession.setActionHandler('play', () => { if (null !== lastSid) playService(lastSid, lastScids); });
  navigator.mediaSession.setActionHandler('pause', stopPlayback);
  navigator.mediaSession.setActionHandler('stop', stopPlayback);
}

function setVolume(percent){
  player.volume = percent / 100;
}

function stopPlayback(){
  player.pause();
  player.removeAttribute('src');
  player.load();
  document.getElementById('playerStatus').textContent = 'Not playing';
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'paused';
    navigator.mediaSession.setPositionState();
  }
}

async function playService(sid, scids){
  lastSid = sid;
  lastScids = scids;
  stopPlayback();
  await fetch('/api/service', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({sid: sid, scids: scids})});

  document.getElementById('playerStatus').textContent = 'Playing...';
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'playing';
    // Tells MPRIS/OS media-control widgets this is a genuine live broadcast (rather than an
    // unknown/invalid session), which some of them require before they display real metadata
    // instead of falling back to a generic "<app> is playing media" placeholder.
    try {
      navigator.mediaSession.setPositionState({duration: Infinity, playbackRate: 1, position: 0});
    } catch (e) { /* not fatal, some browsers reject Infinity */ }
  }
  player.src = '/stream/audio?_=' + Date.now();
  player.volume = document.getElementById('volume').value / 100;
  try {
    await player.play();
  } catch (e) {
    console.error(e);
    document.getElementById('playerStatus').textContent = 'Not playing';
  }
}
player.onerror = stopPlayback;
)HTML");
#else
    // Raw PCM streamed over HTTP and played via the Web Audio API: browsers don't reliably play
    // an <audio> element pointed at a WAV stream of unbounded length, so we decode it ourselves.
    // (Fallback used when the CLI was built without libmp3lame; see mp3encoder.h.)
    html += QStringLiteral(R"HTML(
let audioCtx = null;
let gainNode = null;
let nextStartTime = 0;
let streamXhr = null;
let leftoverBytes = new Uint8Array(0);

if ('mediaSession' in navigator) {
  navigator.mediaSession.setActionHandler('pause', stopPlayback);
  navigator.mediaSession.setActionHandler('stop', stopPlayback);
}

function setVolume(percent){
  if (gainNode) {
    gainNode.gain.value = percent / 100;
  }
}

function stopPlayback(){
  if (streamXhr) {
    const x = streamXhr;
    streamXhr = null;
    x.abort();
  }
  leftoverBytes = new Uint8Array(0);
  document.getElementById('playerStatus').textContent = 'Not playing';
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'paused';
  }
}

function feedPcm(chunk, numChannels, sampleRate){
  let combined = chunk;
  if (leftoverBytes.length) {
    combined = new Uint8Array(leftoverBytes.length + chunk.length);
    combined.set(leftoverBytes, 0);
    combined.set(chunk, leftoverBytes.length);
  }
  const frameBytes = 2 * numChannels;
  const usableLen = combined.length - (combined.length % frameBytes);
  leftoverBytes = combined.slice(usableLen);
  if (0 === usableLen) {
    return;
  }

  const view = new DataView(combined.buffer, combined.byteOffset, usableLen);
  const numFrames = usableLen / frameBytes;
  const buffer = audioCtx.createBuffer(numChannels, numFrames, sampleRate);
  for (let ch = 0; ch < numChannels; ch++) {
    const channelData = buffer.getChannelData(ch);
    for (let i = 0; i < numFrames; i++) {
      channelData[i] = view.getInt16(i * frameBytes + ch * 2, true) / 32768;
    }
  }

  const src = audioCtx.createBufferSource();
  src.buffer = buffer;
  src.connect(gainNode);
  const now = audioCtx.currentTime;
  if (nextStartTime < now) {
    nextStartTime = now + 0.2;  // buffer a bit before (re)starting to avoid glitches
  }
  src.start(nextStartTime);
  nextStartTime += buffer.duration;
}

async function playService(sid, scids){
  // Must happen synchronously, before any await: iOS Safari only allows an AudioContext to be
  // created/unlocked while still inside the original user-gesture call stack (the click handler).
  if (!audioCtx) {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    gainNode = audioCtx.createGain();
    gainNode.connect(audioCtx.destination);
    setVolume(document.getElementById('volume').value);
  }
  if ('suspended' === audioCtx.state) {
    audioCtx.resume();
  }
  // iOS Safari additionally requires an actual buffer to be started (not just resume()) within
  // the gesture to fully unlock audio output; a 1-sample silent buffer is the standard trick.
  const unlockBuffer = audioCtx.createBuffer(1, 1, 22050);
  const unlockSrc = audioCtx.createBufferSource();
  unlockSrc.buffer = unlockBuffer;
  unlockSrc.connect(audioCtx.destination);
  unlockSrc.start(0);

  stopPlayback();
  await fetch('/api/service', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({sid: sid, scids: scids})});

  nextStartTime = 0;
  document.getElementById('playerStatus').textContent = 'Playing...';
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'playing';
  }

  const xhr = new XMLHttpRequest();
  xhr.open('GET', '/stream/audio?_=' + Date.now(), true);
  xhr.overrideMimeType('text/plain; charset=x-user-defined');
  streamXhr = xhr;

  let sampleRate = 48000;
  let numChannels = 2;
  let headersParsed = false;
  let lastLength = 0;

  xhr.onprogress = function(){
    if (streamXhr !== xhr) {
      return;
    }
    if (!headersParsed) {
      sampleRate = parseInt(xhr.getResponseHeader('X-Audio-Sample-Rate') || '48000', 10);
      numChannels = parseInt(xhr.getResponseHeader('X-Audio-Channels') || '2', 10);
      headersParsed = true;
    }
    const text = xhr.responseText;
    const newText = text.substring(lastLength);
    lastLength = text.length;
    if (newText.length) {
      const bytes = new Uint8Array(newText.length);
      for (let i = 0; i < newText.length; i++) {
        bytes[i] = newText.charCodeAt(i) & 0xff;
      }
      feedPcm(bytes, numChannels, sampleRate);
    }
  };
  xhr.onloadend = function(){
    if (streamXhr === xhr) {
      streamXhr = null;
      document.getElementById('playerStatus').textContent = 'Not playing';
    }
  };
  xhr.send();
}
)HTML");
#endif

    html += QStringLiteral(R"HTML(
function fmtSync(level){
  return ({0:'No sync', 1:'Null sync', 2:'Full sync'})[level] ?? level;
}
async function refresh(){
  try {
    const r = await fetch('/api/status');
    const s = await r.json();
    document.getElementById('ensemble').textContent = s.ensemble && s.ensemble.valid ? (s.ensemble.label || '(unnamed)') : '-';
    document.getElementById('channel').textContent = s.channel || '-';
    document.getElementById('freq').textContent = s.frequencyKHz ? (s.frequencyKHz/1000).toFixed(3) + ' MHz' : '-';
    document.getElementById('sync').textContent = fmtSync(s.syncLevel);
    document.getElementById('snr').textContent = (s.snr ?? 0).toFixed(1) + ' dB';
    document.getElementById('nowplaying').textContent = (s.current && s.current.playing) ? s.current.label : '(none)';
    // prefer DL Plus title/artist over the raw DLS text, same precedence as the CLI's own
    // display (DabCliApp::buildCurrentSubtitleLocked()), so the page shows the same info
    const cur = s.current || {};
    let titleText = '-';
    if (cur.dlPlusTitle) {
      titleText = cur.dlPlusArtist ? (cur.dlPlusArtist + ' - ' + cur.dlPlusTitle) : cur.dlPlusTitle;
    } else if (cur.dls) {
      titleText = cur.dls;
    }
    document.getElementById('title').textContent = titleText;
    const tags = cur.dlPlusTags || [];
    document.getElementById('dlPlusTags').textContent = tags.length ? tags.map(t => t.label + ': ' + t.text).join(' | ') : '-';

    const adv = s.advanced || {};
    document.getElementById('freqOffset').textContent = (adv.freqOffsetHz != null) ? (adv.freqOffsetHz/1000).toFixed(2) + ' kHz' : '-';
    document.getElementById('rfLevel').textContent = (adv.rfLevel != null) ? adv.rfLevel.toFixed(1) + ' dB' : '-';
    document.getElementById('gain').textContent = (adv.gain != null) ? adv.gain.toFixed(1) + ' dB' : '-';
    document.getElementById('advBitRate').textContent = adv.bitRate ? adv.bitRate + ' kbps' : '-';
    const stats = adv.decodingStats || {};
    document.getElementById('fibErrors').textContent = (stats.fibErrorCntr != null) ? (stats.fibErrorCntr + ' / ' + stats.fibCntr) : '-';
    document.getElementById('mscCrcErrors').textContent = (stats.mscCrcErrorCntr != null) ? stats.mscCrcErrorCntr : '-';
    document.getElementById('rsBitErrors').textContent = (stats.rsBitErrorCntr != null) ? stats.rsBitErrorCntr : '-';
    document.getElementById('rsUncorrectable').textContent = (stats.rsUncorrectableCntr != null) ? stats.rsUncorrectableCntr : '-';

    const slideImg = document.getElementById('slide');
    const slideVersion = (s.current && s.current.slideVersion) || 0;
    if (slideVersion && slideVersion !== lastSlideVersion) {
      lastSlideVersion = slideVersion;
      slideImg.src = '/api/slideshow?v=' + slideVersion;
      slideImg.classList.add('visible');
    } else if (!slideVersion) {
      lastSlideVersion = 0;
      slideImg.classList.remove('visible');
      slideImg.removeAttribute('src');
    }

    if ('mediaSession' in navigator && s.current) {
      const artist = s.current.dlPlusArtist || '';
      const title = s.current.dlPlusTitle || s.current.dls || s.current.label || '';
      const artwork = slideVersion ? [{src: '/api/slideshow?v=' + slideVersion}] : [];
      navigator.mediaSession.metadata = new MediaMetadata({
        title: title,
        artist: artist,
        album: (s.ensemble && s.ensemble.label) || '',
        artwork: artwork
      });
    }

    const tbody = document.getElementById('services');
    tbody.innerHTML = '';
    (s.services || []).forEach(svc => {
      const isPlaying = s.current && s.current.playing && s.current.sid === svc.sid && s.current.scids === svc.scids;
      const tr = document.createElement('tr');
      tr.innerHTML = '<td>' + svc.label + '</td><td>' + svc.subChId + '</td><td>' + svc.bitRate + ' kbps</td>' +
        '<td><button class="' + (isPlaying?'playing':'') + '" onclick="playService(\'' + svc.sid + '\',' + svc.scids + ')">' +
        (isPlaying ? 'Playing' : 'Play') + '</button></td>';
      tbody.appendChild(tr);
    });
  } catch (e) { console.error(e); }
}
loadChannels();
refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)HTML");

    return html;
}
