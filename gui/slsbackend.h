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

#ifndef SLSBACKEND_H
#define SLSBACKEND_H

#include <QMouseEvent>
#include <QObject>
#include <QQuickImageProvider>

#include "slideshowapp.h"

class PixmapProvider;
class Settings;

// this implementation allow scaling od SLS with the window
class SLSBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString slidePath READ slidePath NOTIFY slidePathChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY backgroundColorChanged FINAL)
    Q_PROPERTY(QString toolTip READ toolTip NOTIFY toolTipChanged FINAL)
    Q_PROPERTY(Qt::CursorShape cursorShape READ cursorShape NOTIFY cursorShapeChanged FINAL)

public:
    //! @brief constructor (empty)
    SLSBackend(Settings *settings, QObject *parent = nullptr);

    //! @brief Reset view => show default picture, used when service changes
    void reset();

    //! @brief Show announcement picture, used when announcement starts
    void showAnnouncement(DabAnnouncement id);

    //! @brief This slot shall be invoked when new slide is to be shown
    void showSlide(const Slide &slide);

    //! @brief This method changes between light and dark mode
    void setupDarkMode(bool darkModeEna);

    //! @brief This method shows station logo
    void showServiceLogo(const QPixmap &logo, bool force = false);

    //! @brief This method sets slide background color
    void setBgColor(const QColor &color);

    PixmapProvider *provider() const { return m_provider; }

    //! @brief Getter for slide path for QML
    QString slidePath() const;

    //! @brief Current slide bakcground in QML
    QColor backgroundColor() const;

    //! @brief Sets current slide background in QML
    void setBackgroundColor(const QColor &color);

    //! @brief Tooltip getter
    QString toolTip() const;

    //! @brief Tooltip setter
    void setToolTip(const QString &toolTip);

    //! @brief Tooltip getter
    Qt::CursorShape cursorShape() const;

    //! @brief Tooltip setter
    void setCursorShape(Qt::CursorShape cursorShape);

    //! @brief Mouse left click handler (opens link if defined)
    Q_INVOKABLE void handleLeftClick();

    //! @brief Mouse right click handler (returns true if contect menu shoudl be shown)
    Q_INVOKABLE bool handleRightClick();

    //! @brief This methos copies current slide to clipboard
    Q_INVOKABLE void copySlideToClipboard();

    //! @brief This methos save current slide to file
    Q_INVOKABLE void saveSlideToFile();

signals:
    void slidePathChanged();
    void backgroundColorChanged();
    void toolTipChanged();
    void cursorShapeChanged();

private:
    //! @brief Application settings
    Settings *m_settings;

    //! @brief URL of current slide
    QString m_clickThroughURL;

    //! @brief dark mode ena
    bool m_isDarkMode = false;

    //! @brief shows slide
    bool m_isShowingSlide = false;

    //! @brief This is copy of current slide
    Slide m_currentSlide;

    //! @brief This is slide background color
    QColor m_bgColor = Qt::black;

    //! @brief Pixmap provider for QML
    PixmapProvider *m_provider;

    //! @brief returns logo to show when no slide is available
    QPixmap getLogo() const;

    //! @brief Methods displays pixmap
    void displayPixmap(const QPixmap &logo);

    //! @brief Slide path used in QML
    QString m_slidePath;

    //! @brief Current background color of slide
    QColor m_backgroundColor;

    void updatePixmap(const QPixmap &pixmap);

    QString m_toolTip;

    Qt::CursorShape m_cursorShape = Qt::ArrowCursor;
};

class PixmapProvider : public QQuickImageProvider
{
public:
    PixmapProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    void setPixmap(const QPixmap &pixmap) { m_pixmap = pixmap; }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        if (size)
        {
            *size = m_pixmap.size();
        }
        if (requestedSize.isValid())
        {
            return m_pixmap.scaled(requestedSize);
        }
        return m_pixmap;
    }

private:
    QPixmap m_pixmap;
};

#endif  // SLSBACKEND_H
