/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QMouseEvent>
#include <QDesktopServices>
#include <QUrl>
#include "slsview.h"

SLSView::SLSView(QWidget *parent) : QGraphicsView(parent)
{
    m_announcementText = nullptr;
    reset();
}

void SLSView::reset()
{
    QPixmap pic = getLogo();
    QGraphicsScene * sc = scene();
    if (nullptr == sc)
    {
        sc = new QGraphicsScene(this);
        m_pixmapItem = sc->addPixmap(pic);

        setScene(sc);
    }
    else
    {
        m_pixmapItem->setPixmap(pic);
    }
    sc->setSceneRect(pic.rect());
    if (m_isDarkMode)
    {
        sc->setBackgroundBrush(Qt::black);
    }
    else
    {
        sc->setBackgroundBrush(Qt::white);
    }

    fitInViewTight(pic.rect(), Qt::KeepAspectRatio);

    setToolTip("");
    setCursor(Qt::ArrowCursor);
    m_isShowingSlide = false;
    m_clickThroughURL.clear();
}

void SLSView::showAnnouncement(DabAnnouncement id)
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

    QGraphicsScene * sc = scene();
    if (nullptr == sc)
    {
        sc = new QGraphicsScene(this);
        m_pixmapItem = sc->addPixmap(pic);

        QFont font;
        font.setPixelSize(24);
        font.setBold(true);
        //font.setFamily("Calibri");

        m_announcementText = sc->addText(DabTables::getAnnouncementName(id), font);
        m_announcementText->setDefaultTextColor(QColor(255,255,255));
        setScene(sc);
    }
    else
    {
        m_pixmapItem->setPixmap(pic);

        if (nullptr == m_announcementText)
        {
            QFont font;
            font.setPixelSize(24);
            font.setBold(true);
            //font.setFamily("Calibri");

            m_announcementText = sc->addText(DabTables::getAnnouncementName(id), font);
            m_announcementText->setDefaultTextColor(QColor(255,255,255));
        }
        else
        {
            m_announcementText->setPlainText(DabTables::getAnnouncementName(id));
        }
    }
    QRectF rect = m_announcementText->boundingRect();
    m_announcementText->setPos((320 - rect.width())/2, 185);

    sc->setSceneRect(pic.rect());
    if (DabAnnouncement::Alarm != id)
    {
        sc->setBackgroundBrush(Qt::darkGray);
    }
    else
    {   //d30f0f
        sc->setBackgroundBrush(QBrush(QColor(0xd3, 0x0f, 0x0f)));
    }

    fitInViewTight(pic.rect(), Qt::KeepAspectRatio);

    setToolTip(QString(tr("Ongoing announcement:")+"<br><b>%1</b>").arg(DabTables::getAnnouncementName(id)));
    setCursor(Qt::ArrowCursor);
    m_clickThroughURL.clear();
}

void SLSView::fitInViewTight(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode)
{   // https://bugreports.qt.io/browse/QTBUG-11945
    if (nullptr == scene() || rect.isNull())
        return;
    // Reset the view scale to 1:1.
    //QRectF unity = matrix().mapRect(QRectF(0, 0, 1, 1));
    QRectF unity = transform().mapRect(QRectF(0, 0, 1, 1));
    if (unity.isEmpty())
        return;
    scale(1 / unity.width(), 1 / unity.height());
    // Find the ideal x / y scaling ratio to fit \a rect in the view.
    QRectF viewRect = viewport()->rect();
    if (viewRect.isEmpty())
        return;
    //QRectF sceneRect = matrix().mapRect(rect);
    QRectF sceneRect = transform().mapRect(rect);
    if (sceneRect.isEmpty())
        return;
    qreal xratio = viewRect.width() / sceneRect.width();
    qreal yratio = viewRect.height() / sceneRect.height();
    // Respect the aspect ratio mode.
    switch (aspectRatioMode) {
    case Qt::KeepAspectRatio:
        xratio = yratio = qMin(xratio, yratio);
        break;
    case Qt::KeepAspectRatioByExpanding:
        xratio = yratio = qMax(xratio, yratio);
        break;
    case Qt::IgnoreAspectRatio:
        break;
    }
    // Scale and center on the center of \a rect.
    scale(xratio, yratio);
    centerOn(rect.center());
}

void SLSView::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna != m_isDarkMode)
    {
        m_isDarkMode = darkModeEna;
        QGraphicsScene * sc = scene();
        if (nullptr != sc)
        {
            if (!m_isShowingSlide)
            {
                QPixmap pic = getLogo();
                if (m_isDarkMode)
                {
                    sc->setBackgroundBrush(Qt::black);
                }
                else
                {
                    sc->setBackgroundBrush(Qt::white);
                }

                m_pixmapItem->setPixmap(pic);

                sc->setSceneRect(pic.rect());
                fitInViewTight(pic.rect(), Qt::KeepAspectRatio);
            }
        }
    }
}

void SLSView::setExpertMode(bool expertModeEna)
{
    m_isExpertMode = expertModeEna;
}

void SLSView::showSlide(const Slide & slide)
{
    displayPixmap(slide.getPixmap());

    // update tool tip
    QString toolTip;

    m_clickThroughURL = slide.getClickThroughURL();
    if (!m_clickThroughURL.isEmpty())
    {
        toolTip = QString("%1<br><br>").arg(m_clickThroughURL);

        setCursor(Qt::PointingHandCursor);
    }
    else
    {   // not present
        setCursor(Qt::ArrowCursor);
    }
    if (0 != slide.getCategoryID())
    {
        toolTip += QString(tr("<b>Category:</b> %1")).arg(slide.getCategoryTitle());
    }
    else
    { /* no catSLS */ }

    if (m_isExpertMode)
    {
        if (!toolTip.isEmpty())
        {
            toolTip += "<br>";
        }
        toolTip += QString(tr("<b>Resolution:</b> %1x%2 pixels<br>")).arg(slide.getPixmap().width()).arg(slide.getPixmap().height());
        toolTip += QString(tr("<b>Size:</b> %1 bytes<br>")).arg(slide.getNumBytes());
        toolTip += QString(tr("<b>Format:</b> %1<br>")).arg(slide.getFormat());
        toolTip += QString(tr("<b>Content name:</b> \"%1\"")).arg(slide.getContentName());
    }


    if (toolTip.isEmpty())
    {
        setToolTip(toolTip);
    }
    else
    {    // this disables automatic lines breaks
        setToolTip(QString("<p style='white-space:pre'>") + toolTip);
    }

    m_isShowingSlide = true;
}

void SLSView::showStationLogo(const QPixmap & logo)
{
    if (logo.isNull())
    {
        return;
    }

    displayPixmap(logo);

    setToolTip("");
    m_isShowingSlide = true;
}


void SLSView::resizeEvent(QResizeEvent *event)
{
    QGraphicsScene * sc = scene();
    if (nullptr != sc)
    {
        fitInViewTight(sc->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    QGraphicsView::resizeEvent(event);
}

void SLSView::showEvent(QShowEvent *event)
{
    QGraphicsScene * sc = scene();
    if (nullptr != sc)
    {
        fitInViewTight(sc->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    QGraphicsView::showEvent(event);
}

void SLSView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_clickThroughURL.isEmpty())
    {   // if clickThroughURL is set
        if ((event->position().x() >=0) && (event->position().y() >= 0) &&
                (event->position().x() < width()) && (event->position().y() < height()))
        {   // release was on the slide view
            QDesktopServices::openUrl(QUrl(m_clickThroughURL, QUrl::TolerantMode));
        }
        else
        { /* mouse was released outside the view */ }
    }
    QGraphicsView::mouseReleaseEvent(event);
}

QPixmap SLSView::getLogo() const
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

void SLSView::displayPixmap(const QPixmap &pixmap)
{
    QGraphicsScene * sc = scene();
    if (nullptr == sc)
    {
        sc = new QGraphicsScene(this);
        m_pixmapItem = sc->addPixmap(pixmap);
        setScene(sc);
    }
    else
    {
        if (nullptr != m_announcementText)
        {
            sc->removeItem(m_announcementText);
            delete m_announcementText;
            m_announcementText = nullptr;
        }

        m_pixmapItem->setPixmap(pixmap);
    }

    sc->setSceneRect(pixmap.rect());
    sc->setBackgroundBrush(Qt::black);
    fitInViewTight(pixmap.rect(), Qt::KeepAspectRatio);
}
