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

#ifndef CATSLSDIALOG_H
#define CATSLSDIALOG_H

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QShowEvent>

#include "slideshowapp.h"

namespace Ui {
class CatSLSDialog;
}

class CatSLSDialog : public QDialog
{
    Q_OBJECT

public:
    //! @ Constructor
    explicit CatSLSDialog(QWidget *parent = nullptr);

    //! @brief Destructor
    ~CatSLSDialog();

    //! @brief Reset UI (on service change)
    void reset();

    //! @brief Slot for new category, removal of category (title is empty) or update of the category name
    void onCategoryUpdate(int catId, const QString & title);

    //! @brief Slot for new slide to come
    void onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides);

signals:
    //! @brief Signal to get current slide in category (when user changes category or when category is updated)
    void getCurrentCatSlide(int catId);

    //! @brief Signal to get next slide in category (when user presses back/fwd button)
    void getNextCatSlide(int catId, bool forward);

private:
    //! @brief User interface
    Ui::CatSLSDialog *ui;

private slots:
    //! @brief Slot when slide back button is clicked
    void onBackButtonClicked();

    //! @brief Slot when slide forward button is clicked
    void onFwdButtonClicked();

    //! @brief Slot when soem category is clicked in categoryView
    void onCategoryViewClicked(const QModelIndex &index);
};

#endif // CATSLSDIALOG_H
