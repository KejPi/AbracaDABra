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

#include "sltreemodel.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(serviceList)

SLTreeModel::SLTreeModel(const ServiceList *sl, const MetadataManager *mm, QObject *parent)
    : QAbstractItemModel(parent), m_slPtr(sl), m_metadataMgrPtr(mm)
{
    m_rootItem = new SLModelItem(m_slPtr, m_metadataMgrPtr);
}

SLTreeModel::~SLTreeModel()
{
    delete m_rootItem;
}

int SLTreeModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 1;
}

QVariant SLTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    SLModelItem *item = static_cast<SLModelItem *>(index.internalPointer());

    return item->data(index.column(), role);
}

bool SLTreeModel::isService(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return false;
    }

    SLModelItem *item = static_cast<SLModelItem *>(index.internalPointer());

    return item->isService();
}

bool SLTreeModel::isFavoriteService(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return false;
    }

    SLModelItem *item = static_cast<SLModelItem *>(index.internalPointer());

    return item->isFavoriteService();
}

bool SLTreeModel::isEnsemble(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return false;
    }

    SLModelItem *item = static_cast<SLModelItem *>(index.internalPointer());

    return item->isEnsemble();
}

ServiceListId SLTreeModel::id(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return 0;
    }

    SLModelItem *item = static_cast<SLModelItem *>(index.internalPointer());

    return item->id();
}

Qt::ItemFlags SLTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return QFlags<Qt::ItemFlag>();
    }

    return QAbstractItemModel::flags(index);
}

QModelIndex SLTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }

    SLModelItem *parentItem;

    if (!parent.isValid())
    {
        parentItem = m_rootItem;
    }
    else
    {
        parentItem = static_cast<SLModelItem *>(parent.internalPointer());
    }

    SLModelItem *childItem = parentItem->child(row);
    if (childItem)
    {
        return createIndex(row, column, childItem);
    }
    else
    {
        return QModelIndex();
    }
}

QModelIndex SLTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return QModelIndex();
    }

    SLModelItem *childItem = static_cast<SLModelItem *>(index.internalPointer());
    SLModelItem *parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
    {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int SLTreeModel::rowCount(const QModelIndex &parent) const
{
    SLModelItem *parentItem;
    if (parent.column() > 0)
    {
        return 0;
    }

    if (!parent.isValid())
    {
        parentItem = m_rootItem;
    }
    else
    {
        parentItem = static_cast<SLModelItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

void SLTreeModel::addEnsembleService(const ServiceListId &ensId, const ServiceListId &servId)
{  // new service in service list

    SLModelItem *ensChild = m_rootItem->findChildId(ensId);
    if (nullptr == ensChild)
    {  // not found ==> new ensemble
        ensChild = new SLModelItem(m_slPtr, m_metadataMgrPtr, ensId, m_rootItem);
        beginInsertRows(QModelIndex(), m_rootItem->childCount(), m_rootItem->childCount());
        m_rootItem->appendChild(ensChild);
        endInsertRows();
    }

    if (servId.scids() != 0)
    {  // this part is to creates secondary service item as second level service in the tree
        // not tested much - only one stimuli for testing is available
        // it expects that parent item is created first -> if not it will add secondary component as normal service
        ServiceListId id(servId.sid(), uint8_t(0));
        SLModelItem *serviceChild = ensChild->findChildId(id);
        if (nullptr != serviceChild)
        {  // primary service found
            beginInsertRows(index(serviceChild->row(), 0, index(ensChild->row(), 0, QModelIndex())), serviceChild->childCount(),
                            serviceChild->childCount());
            serviceChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, serviceChild));
            endInsertRows();
        }
        else
        {
            qCInfo(serviceList, "Adding %6.6X : %d as primary service [old DAB standard]", servId.sid(), servId.scids());
            serviceChild = ensChild->findChildId(servId);
            if (nullptr == serviceChild)
            {  // new service to be added
                beginInsertRows(index(ensChild->row(), 0, QModelIndex()), ensChild->childCount(), ensChild->childCount());
                ensChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, ensChild));
                endInsertRows();
            }
        }
    }
    else
    {
        SLModelItem *serviceChild = ensChild->findChildId(servId);
        if (nullptr == serviceChild)
        {  // new service to be added
            beginInsertRows(index(ensChild->row(), 0, QModelIndex()), ensChild->childCount(), ensChild->childCount());
            ensChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, ensChild));
            endInsertRows();
        }
    }

    sort(0);
}

void SLTreeModel::updateEnsembleService(const ServiceListId &ensId, const ServiceListId &servId)
{             // service label was updated -> need to sort
    sort(0);  // --> this emits dataChanged()
}

void SLTreeModel::removeEnsembleService(const ServiceListId &ensId, const ServiceListId &servId)
{
    SLModelItem *ensChild = m_rootItem->findChildId(ensId);
    if (nullptr == ensChild)
    {  // not found ==> new ensemble
        return;
    }

    // search for servId recursively (it can be secondary service)
    SLModelItem *serviceChild = ensChild->findChildId(servId, true);
    if (nullptr != serviceChild)
    {  // found
        // beginRemoveRows(index(ensChild->row(), 0, QModelIndex()), serviceChild->row(), serviceChild->row());
        beginRemoveRows(index(serviceChild->parentItem()->row(), 0, QModelIndex()), serviceChild->row(), serviceChild->row());
        serviceChild->parentItem()->removeChildId(servId);
        endRemoveRows();
    }
}

void SLTreeModel::removeEnsemble(const ServiceListId &ensId)
{
    SLModelItem *ensChild = m_rootItem->findChildId(ensId);
    if (nullptr == ensChild)
    {  // not found (it shoud not happen)
        return;
    }

    beginRemoveRows(QModelIndex(), ensChild->row(), ensChild->row());
    m_rootItem->removeChildId(ensId);
    endRemoveRows();
}

void SLTreeModel::clear()
{
    beginResetModel();
    // remove all items
    delete m_rootItem;
    // create new root
    m_rootItem = new SLModelItem(m_slPtr, m_metadataMgrPtr);
    endResetModel();
}

void SLTreeModel::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column)

    beginResetModel();
    m_rootItem->sort(order);
    endResetModel();

    emit dataChanged(QModelIndex(), QModelIndex());
}
