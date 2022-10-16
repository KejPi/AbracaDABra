#include <QMouseEvent>
#include <QDesktopServices>
#include <QUrl>
#include "slsview.h"

SLSView::SLSView(QWidget *parent) : QGraphicsView(parent)
{
    reset();
}

void SLSView::reset()
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

void SLSView::setDarkMode(bool darkModeEna)
{
    if (darkModeEna != m_isDarkMode)
    {
        m_isDarkMode = darkModeEna;
        QGraphicsScene * sc = scene();
        if (nullptr != sc)
        {
            if (!m_isShowingSlide)
            {
                QPixmap pic;
                if (m_isDarkMode)
                {
                    pic.load(":/resources/sls_logo_dark.png");
                    sc->setBackgroundBrush(Qt::black);
                }
                else
                {
                    pic.load(":/resources/sls_logo.png");
                    sc->setBackgroundBrush(Qt::white);
                }

                m_pixmapItem->setPixmap(pic);

                sc->setSceneRect(pic.rect());
                fitInViewTight(pic.rect(), Qt::KeepAspectRatio);
            }
        }
    }
}

void SLSView::showSlide(const Slide & slide)
{
    QGraphicsScene * sc = scene();
    if (nullptr == sc)
    {
        sc = new QGraphicsScene(this);
        m_pixmapItem = sc->addPixmap(slide.getPixmap());
        setScene(sc);
    }
    else
    {
        m_pixmapItem->setPixmap(slide.getPixmap());
    }

    sc->setSceneRect(slide.getPixmap().rect());
    sc->setBackgroundBrush(Qt::black);
    fitInViewTight(slide.getPixmap().rect(), Qt::KeepAspectRatio);

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
        toolTip += QString("<b>Category:</b> %1").arg(slide.getCategoryTitle());
    }
    else
    { /* no catSLS */ }

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
        { /* mouse was released outside teh view */ }
    }
    QGraphicsView::mouseReleaseEvent(event);
}
