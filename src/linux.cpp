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

#include "linux.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>
#include <QMetaObject>
#include <QObject>

static QObject *s_app = nullptr;

// Forward declaration
class MediaPlayer2PlayerAdaptor;
static MediaPlayer2PlayerAdaptor *s_playerAdaptor = nullptr;
static QObject *s_mprisRoot = nullptr;

// ── org.mpris.MediaPlayer2 ────────────────────────────────────────────────
class MediaPlayer2Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool        CanQuit              READ canQuit)
    Q_PROPERTY(bool        CanRaise             READ canRaise)
    Q_PROPERTY(bool        HasTrackList         READ hasTrackList)
    Q_PROPERTY(QString     Identity             READ identity)
    Q_PROPERTY(QStringList SupportedUriSchemes  READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes   READ supportedMimeTypes)
public:
    explicit MediaPlayer2Adaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    bool        canQuit()             const { return false; }
    bool        canRaise()            const { return false; }
    bool        hasTrackList()        const { return false; }
    QString     identity()            const { return QStringLiteral("AbracaDABra"); }
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes()  const { return {}; }
public slots:
    void Raise() {}
    void Quit()  {}
};

// ── org.mpris.MediaPlayer2.Player ────────────────────────────────────────
class MediaPlayer2PlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString     PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double      Rate           READ rate)
    Q_PROPERTY(QVariantMap Metadata       READ metadata)
    Q_PROPERTY(double      Volume         READ volume)
    Q_PROPERTY(qlonglong   Position       READ position)
    Q_PROPERTY(double      MinimumRate    READ minimumRate)
    Q_PROPERTY(double      MaximumRate    READ maximumRate)
    Q_PROPERTY(bool        CanGoNext      READ canGoNext)
    Q_PROPERTY(bool        CanGoPrevious  READ canGoPrevious)
    Q_PROPERTY(bool        CanPlay        READ canPlay)
    Q_PROPERTY(bool        CanPause       READ canPause)
    Q_PROPERTY(bool        CanSeek        READ canSeek)
    Q_PROPERTY(bool        CanControl     READ canControl)
public:
    explicit MediaPlayer2PlayerAdaptor(QObject *parent)
        : QDBusAbstractAdaptor(parent), m_isPlaying(true)
    {}

    QString     playbackStatus() const { return m_isPlaying ? QStringLiteral("Playing")
                                                             : QStringLiteral("Paused"); }
    double      rate()           const { return 1.0; }
    QVariantMap metadata()       const { return m_metadata; }
    double      volume()         const { return 1.0; }
    qlonglong   position()       const { return 0; }
    double      minimumRate()    const { return 1.0; }
    double      maximumRate()    const { return 1.0; }
    bool        canGoNext()      const { return true; }
    bool        canGoPrevious()  const { return true; }
    bool        canPlay()        const { return true; }
    bool        canPause()       const { return true; }
    bool        canSeek()        const { return false; }
    bool        canControl()     const { return true; }

    void setPlaying(bool playing)
    {
        m_isPlaying = playing;
        QVariantMap changed;
        changed[QStringLiteral("PlaybackStatus")] = playbackStatus();
        emitPropertiesChanged(changed);
    }

    void setTitle(const QString &title)
    {
        m_metadata[QStringLiteral("xesam:title")]   = title;
        m_metadata[QStringLiteral("mpris:trackid")] =
            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/Track/1")));
        QVariantMap changed;
        changed[QStringLiteral("Metadata")] = QVariant::fromValue(m_metadata);
        emitPropertiesChanged(changed);
    }

    void setSubtitle(const QString &dl)
    {
        // xesam:artist is a QStringList in MPRIS2
        if (dl.isEmpty())
            m_metadata.remove(QStringLiteral("xesam:artist"));
        else
            m_metadata[QStringLiteral("xesam:artist")] = QStringList{dl};
        QVariantMap changed;
        changed[QStringLiteral("Metadata")] = QVariant::fromValue(m_metadata);
        emitPropertiesChanged(changed);
    }

signals:
    void Seeked(qlonglong Position);

public slots:
    void PlayPause() { if (s_app) QMetaObject::invokeMethod(s_app, "toggleMute",               Qt::QueuedConnection); }
    void Play()      { if (s_app) QMetaObject::invokeMethod(s_app, "toggleMute",               Qt::QueuedConnection); }
    void Pause()     { if (s_app) QMetaObject::invokeMethod(s_app, "toggleMute",               Qt::QueuedConnection); }
    void Next()      { if (s_app) QMetaObject::invokeMethod(s_app, "onNextFavoriteService",    Qt::QueuedConnection); }
    void Previous()  { if (s_app) QMetaObject::invokeMethod(s_app, "onPreviousFavoriteService",Qt::QueuedConnection); }
    void Stop()                          {}
    void Seek(qlonglong)                 {}
    void SetPosition(QDBusObjectPath, qlonglong) {}
    void OpenUri(QString)                {}

private:
    bool        m_isPlaying;
    QVariantMap m_metadata;

    void emitPropertiesChanged(const QVariantMap &changed)
    {
        QDBusMessage sig = QDBusMessage::createSignal(
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        sig << QStringLiteral("org.mpris.MediaPlayer2.Player")
            << changed
            << QStringList();
        QDBusConnection::sessionBus().send(sig);
    }
};

// ── Public API ───────────────────────────────────────────────────────────

void linuxSetupMediaRemoteCommands(QObject *app)
{
    s_app = app;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    if (!bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.AbracaDABra")))
        return;

    s_mprisRoot     = new QObject();
    new MediaPlayer2Adaptor(s_mprisRoot);
    s_playerAdaptor = new MediaPlayer2PlayerAdaptor(s_mprisRoot);

    bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), s_mprisRoot,
                       QDBusConnection::ExportAdaptors);
}

void linuxTeardownMediaRemoteCommands()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QStringLiteral("/org/mpris/MediaPlayer2"));
    bus.unregisterService(QStringLiteral("org.mpris.MediaPlayer2.AbracaDABra"));
    delete s_mprisRoot;
    s_mprisRoot     = nullptr;
    s_playerAdaptor = nullptr;
    s_app           = nullptr;
}

void linuxUpdateNowPlayingInfo(const QString &stationName)
{
    if (s_playerAdaptor)
        s_playerAdaptor->setTitle(stationName);
}

void linuxUpdateNowPlayingSubtitle(const QString &dl)
{
    if (s_playerAdaptor)
        s_playerAdaptor->setSubtitle(dl);
}

void linuxUpdateNowPlayingPlaybackState(bool isPlaying)
{
    if (s_playerAdaptor)
        s_playerAdaptor->setPlaying(isPlaying);
}

#include "linux.moc"
