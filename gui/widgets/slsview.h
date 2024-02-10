/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

    //! @brief Show announcement picture, used when announcement starts
    void showAnnouncement(DabAnnouncement id);

    //! @brief This slot shall be invoked when new slide is to be shown
    void showSlide(const Slide & slide);

    //! @brief This method implements fintInViw without borders
    void fitInViewTight(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode);

    //! @brief This method changes between light and dark mode
    void setupDarkMode(bool darkModeEna);

    //! @brief This method enables expert mode features
    void setExpertMode(bool expertModeEna);

    //! @brief This method shows station logo
    void showServiceLogo(const QPixmap & logo);

    //! @brief This method returns default slide save path
    QString savePath() const;

    //! @brief This methis sets default slide save path
    void setSavePath(const QString &newSavePath);

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

    //! @brief Used to store current announcement text
    QGraphicsTextItem * m_announcementText;

    //! @brief URL of current slide
    QString m_clickThroughURL;

    //! @brief dark mode ena
    bool m_isDarkMode = false;

    //! @brief shows slide
    bool m_isShowingSlide = false;

    //! @brief expert mode
    bool m_isExpertMode = false;

    //! @brief returns logo to show when no slide is available
    QPixmap getLogo() const;

    //! @brief Methods displays pixmap
    void displayPixmap(const QPixmap & logo);

    //! @brief This is copy of current slide
    Slide m_currentSlide;

    //! @brief This is default file path for saving slides
    QString m_savePath;
};

#endif // SLSVIEW_H
