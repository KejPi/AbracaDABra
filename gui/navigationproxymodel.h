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

#ifndef NAVIGATIONPROXYMODEL_H
#define NAVIGATIONPROXYMODEL_H

#include <QObject>
#include <QSortFilterProxyModel>
#include <QtQmlIntegration>

#include "navigationmodel.h"

class NavigationProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int currentRow READ currentRow NOTIFY currentRowChanged FINAL)
    Q_PROPERTY(NavigationModel::NavigationOptions filterFlags READ filterFlags WRITE setFilterFlags NOTIFY filterFlagsChanged FINAL)

public:
    NavigationProxyModel(QObject *parent = nullptr);

    int currentRow() const;
    void setCurrentRow(int row);

    Q_INVOKABLE void setNavigationId(NavigationModel::NavigationId id);
    Q_INVOKABLE QVariant get(int row, const QString &roleName) const;
    Q_INVOKABLE QVariantMap getFunctionalityItemData(NavigationModel::NavigationId id) const;

    void setFilterFlags(const NavigationModel::NavigationOptions &newFilterFlags);
    NavigationModel::NavigationOptions filterFlags() const;

signals:
    void currentRowChanged();
    void filterFlagsChanged();

protected:
    // bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    int m_currentRow = -1;
    NavigationModel::NavigationOptions m_filterFlags = NavigationModel::NavigationOption::NoneOption;

    void onSourceModelChanged();
    void setRowForCurrentNavigationId();
};

#endif  // NAVIGATIONPROXYMODEL_H
