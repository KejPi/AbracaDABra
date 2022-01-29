#include "slsview.h"

SLSView::SLSView(QWidget *parent) : QGraphicsView(parent)
{
    reset();
}

void SLSView::reset()
{
    QPixmap pic;
    if (pic.load(":/resources/sls_logo.png"))
    {
        QGraphicsScene * sc = scene();
        if (nullptr == sc)
        {
            //qDebug() << Q_FUNC_INFO << "New graphisc scene";
            sc = new QGraphicsScene(this);
            pixmapItem = sc->addPixmap(pic);

            setScene(sc);
        }
        else
        {
            pixmapItem->setPixmap(pic);
        }
        sc->setSceneRect(pic.rect());
        sc->setBackgroundBrush(Qt::white);
        fitInViewTight(pic.rect(), Qt::KeepAspectRatio);
    }
    else
    {
        qDebug() << "Unable to load :/resources/sls_logo.png";
    }

    setToolTip("");
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

void SLSView::showSlide(const Slide & slide)
{
    QGraphicsScene * sc = scene();
    if (nullptr == sc)
    {
        //qDebug() << Q_FUNC_INFO << "New graphisc scene";
        sc = new QGraphicsScene(this);
        pixmapItem = sc->addPixmap(slide.getPixmap());
        setScene(sc);
    }
    else
    {
        pixmapItem->setPixmap(slide.getPixmap());
    }

    sc->setSceneRect(slide.getPixmap().rect());
    sc->setBackgroundBrush(Qt::black);
    fitInViewTight(slide.getPixmap().rect(), Qt::KeepAspectRatio);

    // update tool tip
    QString toolTip = QString("<b>ContentName:</b> %1").arg(slide.getContentName());
    if (0 != slide.getCategoryID())
    {
        toolTip += QString("<br><b>Category:</b> %1 (%2)").arg(slide.getCategoryTitle())
                                                          .arg(slide.getCategoryID());
    }
    else
    { /* no catSLS */ }
    if (!slide.getClickThroughURL().isEmpty())
    {
        toolTip += QString("<br><b>ClickThroughURL:</b> %1").arg(slide.getClickThroughURL());
    }
    else
    { /* not present */ }

    setToolTip(toolTip);
}
