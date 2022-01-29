#ifndef SLSVIEW_H
#define SLSVIEW_H

#include <QGraphicsView>
#include <QObject>
#include <QGraphicsPixmapItem>

#include "userapplication.h"

// this implementation allow scaling od SLS with the window
class SLSView : public QGraphicsView
{
    Q_OBJECT
public:
    //! @brief constructor (empty)
    SLSView(QWidget *parent = nullptr);

    //! @brief Reset view => show default picture, used when service changes
    void reset();

    //! @brief This slot shall be invoked when new slide is to be shown
    void showSlide(const Slide & slide);

    //! @brief This method implements fintInViw without borders
    void fitInViewTight(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode);

private:
    //! @brief Used to store current pixmap
    QGraphicsPixmapItem * pixmapItem;
};

#endif // SLSVIEW_H
