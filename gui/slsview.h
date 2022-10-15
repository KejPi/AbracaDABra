#ifndef SLSVIEW_H
#define SLSVIEW_H

#include <QGraphicsView>
#include <QObject>
#include <QGraphicsPixmapItem>

#include "slideshowapp.h"

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

    //! @brief This method changes between light and dark mode
    void setDarkMode(bool darkModeEna);

protected:
    //! @brief Reimplemented to scale image correctly
    void resizeEvent(QResizeEvent *event);

    //! @brief Reimplemented to scale image correctly
    void showEvent(QShowEvent *event);

    //! @brief Reimplemented to catch clicks when slide has defined ClickThroughURL
    void mouseReleaseEvent(QMouseEvent *event);

private:
    //! @brief Used to store current pixmap
    QGraphicsPixmapItem * m_pixmapItem;

    //! @brief URL of current slide
    QString m_clickThroughURL;

    //! @brief dark mode ena
    bool m_isDarkMode = false;

    //! @brief shows slide
    bool m_isShowingSlide = false;
};

#endif // SLSVIEW_H
