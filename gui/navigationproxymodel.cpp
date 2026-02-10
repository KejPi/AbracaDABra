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

#include "navigationproxymodel.h"

#include <QTimer>

NavigationProxyModel::NavigationProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    connect(this, &QSortFilterProxyModel::sourceModelChanged, this, &NavigationProxyModel::onSourceModelChanged);
}

// bool NavigationProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
// {}

bool NavigationProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex idx = sourceModel()->index(source_row, 0);
    const auto *item = sourceModel()->data(idx, NavigationModel::ItemRole).value<const NavigationModelItem *>();
    if (item != nullptr)
    {
        return (item->options() & m_filterFlags) != 0;
    }
    return false;
}

int NavigationProxyModel::currentRow() const
{
    return m_currentRow;
}

void NavigationProxyModel::setCurrentRow(int row)
{
    if (m_currentRow == row)
    {
        return;
    }
    m_currentRow = row;
    emit currentRowChanged();
}

void NavigationProxyModel::setNavigationId(NavigationModel::NavigationId id)
{
    dynamic_cast<NavigationModel *>(sourceModel())->setCurrentId(id);
}

void NavigationProxyModel::setRowForCurrentNavigationId()
{
    auto srcModel = dynamic_cast<NavigationModel *>(sourceModel());
    Q_ASSERT(srcModel != nullptr);

    NavigationModel::NavigationId id = srcModel->currentId();
    for (int row = 0; row < rowCount(); ++row)
    {
        QModelIndex idx = index(row, 0);  // 0 = first column
        if (idx.data(NavigationModel::NavigationModelRoles::FunctionalityIdRole).toInt() == id)
        {
            setCurrentRow(row);
            return;
        }
    }
    setCurrentRow(-1);  // not found
}

void NavigationProxyModel::onSourceModelChanged()
{
    auto srcModel = dynamic_cast<NavigationModel *>(sourceModel());
    Q_ASSERT(srcModel != nullptr);

    connect(srcModel, &NavigationModel::currentIdChanged, this, &NavigationProxyModel::setRowForCurrentNavigationId);
}

QVariant NavigationProxyModel::get(int row, const QString &roleName) const
{
    int role = roleNames().key(roleName.toUtf8(), -1);
    if (role == -1)
    {
        return QVariant();
    }

    QModelIndex idx = index(row, 0);
    return data(idx, role);
}

QVariantMap NavigationProxyModel::getFunctionalityItemData(NavigationModel::NavigationId id) const
{
    auto srcModel = dynamic_cast<NavigationModel *>(sourceModel());
    Q_ASSERT(srcModel != nullptr);

    return srcModel->getFunctionalityItemData(id);
}

void NavigationProxyModel::setFilterFlags(const NavigationModel::NavigationOptions &newFilterFlags)
{
    if (m_filterFlags == newFilterFlags)
    {
        return;
    }
    m_filterFlags = newFilterFlags;
    emit filterFlagsChanged();

    invalidateFilter();
    QTimer::singleShot(50, this, [this]() { setRowForCurrentNavigationId(); });
}

NavigationModel::NavigationOptions NavigationProxyModel::filterFlags() const
{
    return m_filterFlags;
}
