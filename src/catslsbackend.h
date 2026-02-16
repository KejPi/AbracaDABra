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

#ifndef CATSLSBACKEND_H
#define CATSLSBACKEND_H

#include <QtQml/qqmlcontext.h>

#include <QQmlApplicationEngine>

#include "slideshowapp.h"
#include "slsbackend.h"
#include "uicontrolprovider.h"

class QItemSelectionModel;
class Settings;
class CategoriesModel;

class CatSLSBackend : public UIControlProvider
{
    Q_OBJECT
    UI_PROPERTY(QString, slideCount)
    UI_PROPERTY_DEFAULT(bool, isFwdEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isBackEnabled, false);
    Q_PROPERTY(CategoriesModel *categoriesModel READ categoriesModel CONSTANT FINAL)
    Q_PROPERTY(SLSBackend *slsBackend READ slsBackend CONSTANT FINAL)

public:
    using UIControlProvider::UIControlProvider;

    //! @ Constructor
    explicit CatSLSBackend(Settings *settings, QObject *parent = nullptr);

    //! @brief Destructor
    ~CatSLSBackend();

    //! @brief Reset UI (on service change)
    void reset();

    //! @brief Slot for new category, removal of category (title is empty) or update of the category name
    void onCategoryUpdate(int catId, const QString &title);

    //! @brief Slot for new slide to come
    void onCatSlide(const Slide &slide, int catId, int slideIdx, int numSlides);

    //! @brief This method sets slide background color
    void setSlsBgColor(const QColor &color);

    //! @brief Slot when slide back button is clicked
    Q_INVOKABLE void onBackButtonClicked();

    //! @brief Slot when slide forward button is clicked
    Q_INVOKABLE void onFwdButtonClicked();

    //! @brief Slot when soem category is clicked in categoryView
    Q_INVOKABLE void onCategoryViewClicked(int row);

    //! @brief Getter for categories model
    CategoriesModel *categoriesModel() const;

    //! @brief Getter for SLS backend
    SLSBackend *slsBackend() const;

signals:
    //! @brief Signal to get current slide in category (when user changes category or when category is updated)
    void getCurrentCatSlide(int catId);

    //! @brief Signal to get next slide in category (when user presses back/fwd button)
    void getNextCatSlide(int catId, bool forward);

private:
    //! @brief Categories model
    CategoriesModel *m_categoriesModel = nullptr;

    //! @brief Slide provider
    SLSBackend *m_slsBackend = nullptr;
};

class CategoriesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged FINAL)
public:
    enum Roles
    {
        CatNameRole = Qt::UserRole,
        CatIdRole,
    };

    explicit CategoriesModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    ~CategoriesModel() { qDeleteAll(m_modelData); }
    void setData(int id, const QString &title);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_modelData.count(); }
    void clear();

    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int currentIndex);
    int currentCategoryId() const;

signals:
    void currentIndexChanged();

private:
    struct Category
    {
        QString title;
        int id;
    };
    QList<Category *> m_modelData;
    int m_currentIndex = -1;
};

#endif  // CATSLSBACKEND_H
