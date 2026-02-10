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

#include "itemmodel.h"

ItemModel::ItemModel(QObject *parent) : QAbstractListModel{parent}
{
    connect(this, &QAbstractListModel::rowsInserted, this, &ItemModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &ItemModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &ItemModel::rowCountChanged);
}

QVariant ItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_modelData.size())
    {
        return QVariant();
    }

    const Item &item = m_modelData.at(index.row());

    switch (role)
    {
        case ItemModelRoles::NameRole:
            return item.name;
        case ItemModelRoles::DataRole:
            return item.data;
        default:
            return QVariant();
    }
}

bool ItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_modelData.size())
    {
        return false;
    }

    Item &item = m_modelData[index.row()];

    switch (role)
    {
        case ItemModelRoles::NameRole:
            item.name = value.toString();
            break;
        case ItemModelRoles::DataRole:
            item.data = value;
            break;
        default:
            return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

void ItemModel::addItem(const QString &name, const QVariant &data)
{
    beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
    m_modelData.append({name, data});
    endInsertRows();
}

int ItemModel::findItem(const QVariant &data) const
{
    for (int i = 0; i < m_modelData.size(); ++i)
    {
        if (m_modelData.at(i).data == data)
        {
            return i;
        }
    }
    return -1;
}

QHash<int, QByteArray> ItemModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemModelRoles::NameRole] = "itemName";
    roles[ItemModelRoles::DataRole] = "itemData";
    return roles;
}

void ItemModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();

    setCurrentIndex(-1);
}

int ItemModel::currentIndex() const
{
    return m_currentIndex;
}

void ItemModel::setCurrentIndex(int currentIdx)
{
    if (m_currentIndex == currentIdx)
    {
        return;
    }
    m_currentIndex = currentIdx;
    emit currentIndexChanged();
}

QVariant ItemModel::currentData() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_modelData.size())
    {
        return QVariant();
    }
    return m_modelData.at(m_currentIndex).data;
}

bool ItemModel::setCurrentData(const QVariant &data)
{
    int idx = findItem(data);
    if (idx >= 0)
    {
        setCurrentIndex(idx);
        return true;
    }
    return false;
}
