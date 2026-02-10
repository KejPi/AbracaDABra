/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "catslsbackend.h"

#include <QDebug>
#include <QItemSelectionModel>
#include <QQmlContext>
#include <QStandardItemModel>

#include "slsbackend.h"

CatSLSBackend::CatSLSBackend(Settings *settings, QObject *parent) : UIControlProvider(parent)
{
    m_categoriesModel = new CategoriesModel(this);
    m_slsBackend = new SLSBackend(settings, this);
    reset();
}

CatSLSBackend::~CatSLSBackend()
{
    delete m_slsBackend;
    delete m_categoriesModel;
}

void CatSLSBackend::reset()
{
    slideCount("");
    isFwdEnabled(false);
    isBackEnabled(false);

    m_categoriesModel->clear();
    m_slsBackend->reset();
}

void CatSLSBackend::onCategoryUpdate(int catId, const QString &title)
{
    m_categoriesModel->setData(catId, title);

    if (m_categoriesModel->currentIndex() < 0 && m_categoriesModel->rowCount() > 0)
    {  // if index is not valid - select first item
        // update current index and trigger request for slide
        onCategoryViewClicked(0);
    }
    else
    {  // current index is valid - ask for current slide
        emit getCurrentCatSlide(catId);
    }
}

void CatSLSBackend::onCatSlide(const Slide &slide, int catId, int slideIdx, int numSlides)
{
    if (m_categoriesModel->currentCategoryId() != catId)
    {  // do nothing, category is not selected
        return;
    }

    slideCount(QString("%1 / %2").arg(slideIdx + 1).arg(numSlides));

    isFwdEnabled(numSlides > 1);
    isBackEnabled(numSlides > 1);
    m_slsBackend->showSlide(slide);
}

void CatSLSBackend::setSlsBgColor(const QColor &color)
{
    m_slsBackend->setBgColor(color);
}

void CatSLSBackend::onBackButtonClicked()
{
    emit getNextCatSlide(m_categoriesModel->currentCategoryId(), false);
}

void CatSLSBackend::onFwdButtonClicked()
{
    emit getNextCatSlide(m_categoriesModel->currentCategoryId(), true);
}

void CatSLSBackend::onCategoryViewClicked(int row)
{
    m_categoriesModel->setCurrentIndex(row);
    emit getCurrentCatSlide(m_categoriesModel->currentCategoryId());
}

void CategoriesModel::setData(int id, const QString &title)
{
    // find category
    int catIdIdx = -1;
    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id == id)
        {  // found
            catIdIdx = row;
            break;
        }
    }

    if (title.isEmpty())
    {  // remove category
        if (catIdIdx >= 0)
        {  // found
            beginRemoveRows(QModelIndex(), catIdIdx, catIdIdx);
            delete m_modelData.at(catIdIdx);
            m_modelData.removeAt(catIdIdx);
            endRemoveRows();
            if (catIdIdx == currentIndex())
            {  // invalidate current index
                setCurrentIndex(-1);
            }
        }
        else
        { /* category not found */
        }
    }
    else
    {  // add category
        if (catIdIdx < 0)
        {  // not found - add new category
            // find position
            catIdIdx = 0;
            while (catIdIdx < m_modelData.size())
            {
                if (m_modelData.at(catIdIdx)->title > title)
                {
                    break;
                }
                catIdIdx += 1;
            }
            auto item = new Category;
            item->id = id;
            item->title = title;
            beginInsertRows(QModelIndex(), catIdIdx, catIdIdx);
            m_modelData.insert(catIdIdx, item);
            endInsertRows();
            if (catIdIdx <= currentIndex())
            {  // inserted before current => move current index
                setCurrentIndex(currentIndex() + 1);
            }
        }
        else
        {  // found - rename category
            m_modelData.at(catIdIdx)->title = title;
            emit dataChanged(index(catIdIdx, 0), index(catIdIdx, 0), {Roles::CatNameRole});
        }
    }
}

QVariant CategoriesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
    {
        return QVariant();
    }

    switch (role)
    {
        case Roles::CatNameRole:
            return QVariant(m_modelData.at(index.row())->title);
        case Roles::CatIdRole:
            return QVariant(m_modelData.at(index.row())->id);
        default:
            break;
    }
    return QVariant();
}

QHash<int, QByteArray> CategoriesModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Roles::CatNameRole] = "catName";
    return roles;
}

void CategoriesModel::clear()
{
    beginResetModel();
    qDeleteAll(m_modelData);
    m_modelData.clear();
    endResetModel();
    setCurrentIndex(-1);
}

void CategoriesModel::setCurrentIndex(int currentIndex)
{
    if (m_currentIndex == currentIndex)
    {
        return;
    }
    m_currentIndex = currentIndex;
    emit currentIndexChanged();
}

int CategoriesModel::currentCategoryId() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_modelData.size())
    {
        return -1;
    }
    return m_modelData.at(m_currentIndex)->id;
}

CategoriesModel *CatSLSBackend::categoriesModel() const
{
    return m_categoriesModel;
}

SLSBackend *CatSLSBackend::slsBackend() const
{
    return m_slsBackend;
}
