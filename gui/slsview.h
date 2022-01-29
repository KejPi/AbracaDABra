#ifndef SLSVIEW_H
#define SLSVIEW_H

#include <QGraphicsView>
#include <QObject>

// this implementation allow scaling od SLS with the window
class SLSView : public QGraphicsView
{
    Q_OBJECT
public:
    //! @brief constructor (empty)
    SLSView(QWidget *parent = nullptr);

    //! @brief This method implements fintInViw without borders
    void fitInViewTight(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode);
};

#endif // SLSVIEW_H
