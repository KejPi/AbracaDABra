#include "slsview.h"

SLSView::SLSView(QWidget *parent) : QGraphicsView(parent)
{
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
