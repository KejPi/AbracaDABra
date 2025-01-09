/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "slproxymodel.h"

#include "slmodel.h"

SLProxyModel::SLProxyModel(QObject *parent) : QSortFilterProxyModel{parent}, m_emptyEpgFilter(false), m_ueidFilter(0)
{}

bool SLProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    bool ret = true;
    if (m_emptyEpgFilter)
    {
        ret = ret && (sourceModel()->data(index, SLModelRole::EpgModelRole).value<EPGModel *>() != nullptr);
    }
    if (m_ueidFilter > 0)
    {
        QList<int> ensList = sourceModel()->data(index, SLModelRole::EnsembleListRole).value<QList<int>>();
        ret = ret && ensList.contains(m_ueidFilter);
    }

    return ret;
}

bool SLProxyModel::emptyEpgFilter() const
{
    return m_emptyEpgFilter;
}

void SLProxyModel::setEmptyEpgFilter(bool newEmptyEpgFilter)
{
    if (m_emptyEpgFilter == newEmptyEpgFilter)
    {
        return;
    }
    m_emptyEpgFilter = newEmptyEpgFilter;
    invalidateFilter();
    emit emptyEpgFilterChanged();
}

int SLProxyModel::ueidFilter() const
{
    return m_ueidFilter;
}

void SLProxyModel::setUeidFilter(int newUeidFilter)
{
    if (m_ueidFilter == newUeidFilter)
    {
        return;
    }
    m_ueidFilter = newUeidFilter;
    invalidateFilter();
    emit ueidFilterChanged();
}
