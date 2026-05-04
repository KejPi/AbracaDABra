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

#ifndef SLPROXYMODEL_H
#define SLPROXYMODEL_H

#include <QObject>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include "servicelistid.h"

class SLProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool emptyEpgFilter READ emptyEpgFilter WRITE setEmptyEpgFilter NOTIFY emptyEpgFilterChanged FINAL)
    Q_PROPERTY(int ueidFilter READ ueidFilter WRITE setUeidFilter NOTIFY ueidFilterChanged FINAL)
    Q_PROPERTY(QString serviceNameFilter READ serviceNameFilter WRITE setServiceNameFilter NOTIFY serviceNameFilterChanged FINAL)
public:
    explicit SLProxyModel(QObject *parent = nullptr);

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    bool emptyEpgFilter() const;
    void setEmptyEpgFilter(bool newEmptyEpgFilter);
    int ueidFilter() const;
    void setUeidFilter(int newUeidFilter);

    QString serviceNameFilter() const;
    void setServiceNameFilter(const QString &newServiceNameFilter);

    ServiceListId id(const QModelIndex &index) const;
    bool isFavoriteService(const QModelIndex &index) const;

signals:
    void emptyEpgFilterChanged();
    void ueidFilterChanged();

    void serviceNameFilterChanged();

private:
    bool m_emptyEpgFilter;
    int m_ueidFilter;
    QString m_serviceNameFilter;
};

#endif  // SLPROXYMODEL_H
