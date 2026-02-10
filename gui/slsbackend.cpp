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

#include "slsbackend.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include "androidfilehelper.h"
#include "settings.h"

Q_DECLARE_LOGGING_CATEGORY(application)

SLSBackend::SLSBackend(Settings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    m_provider = new PixmapProvider;
    reset();
}

void SLSBackend::reset()
{
    QPixmap pic = getLogo();
    if (m_isDarkMode)
    {
        setBackgroundColor(Qt::black);
    }
    else
    {
        setBackgroundColor(Qt::white);
    }

    updatePixmap(pic);

    setToolTip("");
    setCursorShape(Qt::ArrowCursor);
    m_currentSlide.setContentName("");  // this blocks slide saving (Issue #131)
    m_isShowingSlide = false;
    m_clickThroughURL.clear();
}

void SLSBackend::showAnnouncement(DabAnnouncement id)
{
    QPixmap pic;
    if (DabAnnouncement::Undefined == id)
    {
        pic = getLogo();
        m_isShowingSlide = false;
    }
    else
    {
        pic.load(QString(":/resources/announcement%1.png").arg(static_cast<int>(id), 2, 10, QChar('0')));
        m_isShowingSlide = true;
    }

    if (DabAnnouncement::Alarm != id)
    {
        setBackgroundColor(Qt::darkGray);
    }
    else
    {  // d30f0f
        setBackgroundColor(QColor(0xd3, 0x0f, 0x0f));
    }

    QPainter painter(&pic);
    QFont font;
    font.setPixelSize(24);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255));
    const QRect rectangle = QRect(0, 185, 320, 240 - 185);
    painter.drawText(rectangle, Qt::AlignHCenter | Qt::AlignTop, DabTables::getAnnouncementName(id));
    updatePixmap(pic);

    setToolTip(QString(tr("Ongoing announcement:") + "<br><b>%1</b>").arg(DabTables::getAnnouncementName(id)));
    setCursorShape(Qt::ArrowCursor);
    m_clickThroughURL.clear();
}

void SLSBackend::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna != m_isDarkMode)
    {
        m_isDarkMode = darkModeEna;

        if (!m_isShowingSlide)
        {
            updatePixmap(getLogo());
            if (m_isDarkMode)
            {
                setBackgroundColor(Qt::black);
            }
            else
            {
                setBackgroundColor(Qt::white);
            }
        }
    }
}

void SLSBackend::showSlide(const Slide &slide)
{
    displayPixmap(slide.getPixmap());

    // update tool tip
    QString toolTip;

    m_clickThroughURL = slide.getClickThroughURL();
    if (!m_clickThroughURL.isEmpty())
    {
        toolTip = QString("%1<br><br>").arg(m_clickThroughURL);

        setCursorShape(Qt::PointingHandCursor);
    }
    else
    {  // not present
        setCursorShape(Qt::ArrowCursor);
    }
    if (0 != slide.getCategoryID())
    {
        toolTip += QString(tr("<b>Category:</b> %1")).arg(slide.getCategoryTitle());
    }
    else
    { /* no catSLS */
    }

    if (!toolTip.isEmpty())
    {
        toolTip += "<br>";
    }
    toolTip += QString(tr("<b>Resolution:</b> %1x%2 pixels<br>")).arg(slide.getPixmap().width()).arg(slide.getPixmap().height());
    toolTip += QString(tr("<b>Size:</b> %1 bytes<br>")).arg(slide.getNumBytes());
    toolTip += QString(tr("<b>Format:</b> %1<br>")).arg(slide.getFormat());
    toolTip += QString(tr("<b>Content name:</b> \"%1\"")).arg(slide.getContentName());

    if (toolTip.isEmpty())
    {
        setToolTip(toolTip);
    }
    else
    {  // this disables automatic lines breaks
        setToolTip(QString("<p style='white-space:pre'>") + toolTip);
    }

    m_currentSlide = slide;
    m_isShowingSlide = true;
}

void SLSBackend::showServiceLogo(const QPixmap &logo, bool force)
{
    if (logo.isNull())
    {
        if (force)
        {
            reset();
        }
        return;
    }

    if (!m_isShowingSlide || force)
    {
        displayPixmap(logo);

        // setToolTip(tr("Service logo"));
        m_currentSlide.setPixmap(logo);
        m_isShowingSlide = true;
    }
}

QPixmap SLSBackend::getLogo() const
{
    QPixmap pic;
    if (m_isDarkMode)
    {
        pic.load(":/resources/sls_logo_dark.png");
    }
    else
    {
        pic.load(":/resources/sls_logo.png");
    }
    return pic;
}

void SLSBackend::displayPixmap(const QPixmap &pixmap)
{
    updatePixmap(pixmap);

    setBackgroundColor(m_bgColor);
}

void SLSBackend::updatePixmap(const QPixmap &pixmap)
{
    m_provider->setPixmap(pixmap);
    // append random query param to force refresh
    // m_slidePath = QString("image://sls/%1").arg(QDateTime::currentMSecsSinceEpoch());
    m_slidePath = QString("%1").arg(QDateTime::currentMSecsSinceEpoch());
    emit slidePathChanged();
}

void SLSBackend::setBgColor(const QColor &color)
{
    if (color != m_bgColor)
    {
        m_bgColor = color;
        if (m_isShowingSlide)
        {
            displayPixmap(m_currentSlide.getPixmap());
        }
    }
}

QString SLSBackend::slidePath() const
{
    return m_slidePath;
}

QColor SLSBackend::backgroundColor() const
{
    return m_backgroundColor;
}

void SLSBackend::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color)
    {
        m_backgroundColor = color;
        emit backgroundColorChanged();
    }
}

QString SLSBackend::toolTip() const
{
    return m_toolTip;
}

void SLSBackend::setToolTip(const QString &toolTip)
{
    if (m_toolTip == toolTip)
    {
        return;
    }
    m_toolTip = toolTip;
    emit toolTipChanged();
}

Qt::CursorShape SLSBackend::cursorShape() const
{
    return m_cursorShape;
}

void SLSBackend::setCursorShape(Qt::CursorShape cursorShape)
{
    if (m_cursorShape == cursorShape)
    {
        return;
    }
    m_cursorShape = cursorShape;
    emit cursorShapeChanged();
}

void SLSBackend::handleLeftClick()
{
    if (!m_clickThroughURL.isEmpty())
    {  // if clickThroughURL is set
        QDesktopServices::openUrl(QUrl(m_clickThroughURL, QUrl::TolerantMode));
    }
}

bool SLSBackend::handleRightClick()
{
    return (m_isShowingSlide && !m_currentSlide.getContentName().isEmpty());
}

void SLSBackend::copySlideToClipboard()
{
    QImage image;
    if (image.loadFromData(m_currentSlide.getRawData(), m_currentSlide.getFormat().toLatin1()))
    {
        QGuiApplication::clipboard()->setImage(image, QClipboard::Clipboard);
        qCInfo(application) << "Slide copied to clipboard";
    }
    else
    {
        qCWarning(application) << "Failed to copy slide to clipboard";
    }
}

void SLSBackend::saveSlideToFile()
{
    QString filename = m_currentSlide.getContentName();

    // need to copy data as soon as possible before new slide comes
    QByteArray data = m_currentSlide.getRawData();

    // remove problematic characters
    static const QRegularExpression regexp("[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    filename.replace(regexp, "_");
    if (QFileInfo(filename).suffix().isEmpty())
    {
        filename.append(QString(".%1").arg(m_currentSlide.getFormat().toLower()));
    }

    if (!filename.isEmpty())
    {
        const QString slidePath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, SLIDES_DIR_NAME);

        // Ensure directory exists and is writable
        if (!AndroidFileHelper::mkpath(m_settings->dataStoragePath, SLIDES_DIR_NAME))
        {
            qCWarning(application) << "Failed to create slide export directory:" << AndroidFileHelper::lastError();
            return;
        }

        if (!AndroidFileHelper::hasWritePermission(slidePath))
        {
            qCWarning(application) << "No permission to write to:" << slidePath;
            qCWarning(application) << "Please select a new data storage folder in settings.";
            return;
        }

        filename = QString("%1_%2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), filename);
        QString mime = "image/jpeg";
        if (m_currentSlide.getFormat() == "PNG")
        {
            mime = "image/png";
        }

        if (AndroidFileHelper::writeBinaryFile(slidePath, filename, data, mime))
        {
            qCInfo(application) << "Slide saved to file:" << QString("%1/%2").arg(slidePath, filename);
        }
        else
        {
            qCWarning(application) << "Failed to save slide to file:" << AndroidFileHelper::lastError();
        }
    }
}
