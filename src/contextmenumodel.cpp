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

#include "contextmenumodel.h"

#include <QDebug>

ContextMenuModel::ContextMenuModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(this, &QAbstractListModel::rowsInserted, this, &ContextMenuModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &ContextMenuModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &ContextMenuModel::rowCountChanged);
}

int ContextMenuModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return m_items.count();
}

QVariant ContextMenuModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.count())
    {
        return QVariant();
    }

    const MenuItem &item = m_items[index.row()];

    switch (role)
    {
        case TextRole:
            return item.text;
        case EnabledRole:
            return item.enabled;
        case ActionIdRole:
            return item.actionId;
        case CheckableRole:
            return item.checkable;
        case CheckedRole:
            return item.checked;
        case DataRole:
            return item.data;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ContextMenuModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TextRole] = "text";
    roles[EnabledRole] = "enabled";
    roles[ActionIdRole] = "actionId";
    roles[CheckableRole] = "checkable";
    roles[CheckedRole] = "checked";
    return roles;
}

void ContextMenuModel::addMenuItem(const QString &text, int actionId, const QVariant &data, bool enabled, bool checkable, bool checked)
{
    // Check if actionId already exists
    for (int i = 0; i < m_items.count(); ++i)
    {
        if (m_items[i].actionId == actionId)
        {
            // Update existing item
            bool changed = false;

            if (m_items[i].text != text)
            {
                m_items[i].text = text;
                changed = true;
            }
            if (m_items[i].enabled != enabled)
            {
                m_items[i].enabled = enabled;
                changed = true;
            }
            if (m_items[i].checkable != checkable)
            {
                m_items[i].checkable = checkable;
                changed = true;
            }
            if (m_items[i].checked != checked)
            {
                m_items[i].checked = checked;
                changed = true;
            }
            if (m_items[i].data != data)
            {
                m_items[i].data = data;
                changed = true;
            }

            if (changed)
            {
                QModelIndex modelIndex = index(i);
                emit dataChanged(modelIndex, modelIndex);
            }
            return;
        }
    }

    // ActionId doesn't exist, add new item
    beginInsertRows(QModelIndex(), m_items.count(), m_items.count());
    m_items.append({text, actionId, enabled, checkable, checked, data});
    endInsertRows();
}

void ContextMenuModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}

void ContextMenuModel::triggerAction(int index)
{
    if (index >= 0 && index < m_items.count())
    {
        const MenuItem &item = m_items[index];

        // If it's checkable, toggle the checked state
        if (item.checkable)
        {
            setChecked(index, !item.checked);
        }

        emit actionTriggered(item.actionId, item.data);
    }
}

void ContextMenuModel::setChecked(int index, bool checked)
{
    if (index >= 0 && index < m_items.count())
    {
        if (m_items[index].checkable && m_items[index].checked != checked)
        {
            m_items[index].checked = checked;

            QModelIndex modelIndex = this->index(index);
            emit dataChanged(modelIndex, modelIndex, {CheckedRole});
            emit checkStateChanged(m_items[index].actionId, checked, m_items[index].data);
        }
    }
}
