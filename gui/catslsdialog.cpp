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

#include <QDebug>
#include "slsview.h"
#include "catslsdialog.h"
#include "ui_catslsdialog.h"

CatSLSDialog::CatSLSDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CatSLSDialog)
{
    ui->setupUi(this);

    connect(ui->fwdButton, &QPushButton::clicked, this, &CatSLSDialog::onFwdButtonClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &CatSLSDialog::onBackButtonClicked);
    connect(ui->categoryView, &QListView::clicked, this, &CatSLSDialog::onCategoryViewClicked);

    ui->slideCountLabel->setMinimumWidth(ui->slideCountLabel->fontMetrics().boundingRect("88 / 88").width());
    ui->slsView->setMinimumSize(QSize(322, 242));
    ui->categoryView->setModel(new QStandardItemModel());

    reset();

    resize(minimumSizeHint());
}

CatSLSDialog::~CatSLSDialog()
{
    delete ui;
}

void CatSLSDialog::reset()
{
    hide();

    ui->slideCountLabel->setText("");
    ui->fwdButton->setEnabled(false);
    ui->backButton->setEnabled(false);
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    model->clear();

    ui->slsView->reset();
}

void CatSLSDialog::onCategoryUpdate(int catId, const QString & title)
{
    // find category
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    QModelIndexList list = model->match(model->index(0, 0), Qt::UserRole, catId);

    if (title.isEmpty())
    {   // remove category
        if (!list.empty())
        {  // found
            model->removeRow(list.first().row());
        }
        else
        { /* category not found */ }
    }
    else
    {  // add category
        if (list.empty())
        {   // not found - add new category
            QStandardItem * item = new QStandardItem(title);
            item->setData(catId, Qt::UserRole);
            model->appendRow(item);
            model->sort(0);
        }
        else
        {   // found - rename category
            model->itemFromIndex(list.first())->setText(title);
        }
    }

    if (!ui->categoryView->currentIndex().isValid())
    {   // // if index is not valid - select first item
        ui->categoryView->selectionModel()->setCurrentIndex(model->index(0,0), QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);

        // this triggers request for slide
        onCategoryViewClicked(model->index(0,0));
    }
    else
    { // current index is valid - ask for current slide
        emit getCurrentCatSlide(catId);
    }
}

void CatSLSDialog::onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides)
{
    if (ui->categoryView->currentIndex().data(Qt::UserRole).toUInt() != catId)
    {   // do nothing, category is not selected
        return;
    }

    ui->slideCountLabel->setText(QString("%1 / %2").arg(slideIdx+1).arg(numSlides));

    if (numSlides > 1)
    {
        ui->fwdButton->setEnabled(true);
        ui->backButton->setEnabled(true);
    }
    else
    {
        ui->fwdButton->setEnabled(false);
        ui->backButton->setEnabled(false);
    }

    ui->slsView->showSlide(slide);
}

void CatSLSDialog::setExpertMode(bool expertModeEna)
{
    ui->slsView->setExpertMode(expertModeEna);
}

void CatSLSDialog::onBackButtonClicked()
{
    emit getNextCatSlide(ui->categoryView->currentIndex().data(Qt::UserRole).toUInt(), false);
}


void CatSLSDialog::onFwdButtonClicked()
{
    emit getNextCatSlide(ui->categoryView->currentIndex().data(Qt::UserRole).toUInt(), true);
}


void CatSLSDialog::onCategoryViewClicked(const QModelIndex &index)
{
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    QStandardItem *item = model->itemFromIndex(index);

    emit getCurrentCatSlide(item->data(Qt::UserRole).toUInt());
}

