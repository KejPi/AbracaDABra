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

#include "dabchannellistmodel.h"

DABChannelListModel::DABChannelListModel(QObject *parent) : QAbstractListModel{parent}
{}

QVariant DABChannelListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    if (index.row() >= rowCount())
    {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        auto key = DabTables::channelList.keys().at(index.row());
        return QVariant(DabTables::channelList.value(key));
    }
    if (role == Roles::FrequencyRole)
    {
        return DabTables::channelList.keys().at(index.row());
    }

    return QVariant();
}

DABChannelListFilteredModel::DABChannelListFilteredModel(QObject *parent) : QSortFilterProxyModel(parent), m_frequencyFilter(0)
{}

void DABChannelListFilteredModel::setChannelFilter(int freq)
{
    if (DabTables::channelList.contains(freq))
    {  // this is protection from setting invalid DAB frequency
        m_frequencyFilter = freq;
    }
    else
    {
        m_frequencyFilter = 0;
    }
    invalidateFilter();
}

bool DABChannelListFilteredModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_frequencyFilter == 0)
    {
        return true;
    }
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return sourceModel()->data(index, DABChannelListModel::Roles::FrequencyRole).toInt() == m_frequencyFilter;
}
