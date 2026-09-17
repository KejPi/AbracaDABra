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

#include "sltreemodel.h"

#include <QLoggingCategory>

#include "slmodel.h"

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
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case SLModelRole::IdRole:
        case SLModelRole::SmallLogoRole:
        case SLModelRole::SmallLogoIdRole:
        case SLModelRole::EpgModelRole:
        case SLModelRole::EnsembleListRole:
        case SLModelRole::IsFavoriteRole:
        case SLModelRole::SIdHexRole:
        case SLModelRole::ChannelRole:
            return item->data(index.column(), role);
    }
    return QVariant();
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

    if (!parent.isValid())
    {  // ensemble level -> row must be resolved against visible ensembles when filtering
        SLModelItem *childItem = nullptr;
        if (!m_filterCurrentEnsemble)
        {
            childItem = m_rootItem->child(row);
        }
        else
        {
            int visibleRow = -1;
            for (int i = 0; i < m_rootItem->childCount(); ++i)
            {
                SLModelItem *candidate = m_rootItem->child(i);
                if (isEnsembleVisible(candidate) && (++visibleRow == row))
                {
                    childItem = candidate;
                    break;
                }
            }
        }

        return childItem ? createIndex(row, column, childItem) : QModelIndex();
    }

    SLModelItem *parentItem = static_cast<SLModelItem *>(parent.internalPointer());
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

    int row = (m_filterCurrentEnsemble && parentItem->parentItem() == m_rootItem) ? visibleRootRow(parentItem) : parentItem->row();

    return createIndex(row, 0, parentItem);
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

    if (parentItem == m_rootItem && m_filterCurrentEnsemble)
    {
        int count = 0;
        for (int i = 0; i < m_rootItem->childCount(); ++i)
        {
            if (isEnsembleVisible(m_rootItem->child(i)))
            {
                ++count;
            }
        }
        return count;
    }

    return parentItem->childCount();
}

bool SLTreeModel::isEnsembleVisible(SLModelItem *ensItem) const
{
    return !m_filterCurrentEnsemble || (ensItem->id() == m_currentEnsembleId);
}

int SLTreeModel::visibleRootRow(SLModelItem *item) const
{
    int row = -1;
    for (int i = 0; i < m_rootItem->childCount(); ++i)
    {
        SLModelItem *candidate = m_rootItem->child(i);
        if (isEnsembleVisible(candidate))
        {
            ++row;
        }
        if (candidate == item)
        {
            return isEnsembleVisible(candidate) ? row : -1;
        }
    }
    return -1;
}

void SLTreeModel::addEnsembleService(const ServiceListId &ensId, const ServiceListId &servId)
{  // new service in service list

    if (m_filterCurrentEnsemble)
    {  // filtered row numbers do not match real child indices -> reset instead of fine-grained signalling
        beginResetModel();

        SLModelItem *ensChild = m_rootItem->findChildId(ensId);
        if (nullptr == ensChild)
        {
            ensChild = new SLModelItem(m_slPtr, m_metadataMgrPtr, ensId, m_rootItem);
            m_rootItem->appendChild(ensChild);
        }

        if (servId.scids() != 0)
        {
            ServiceListId id(servId.sid(), uint8_t(0));
            SLModelItem *serviceChild = ensChild->findChildId(id);
            if (nullptr != serviceChild)
            {
                serviceChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, serviceChild));
            }
            else
            {
                qCInfo(serviceList, "Adding %6.6X : %d as primary service [old DAB standard]", servId.sid(), servId.scids());
                serviceChild = ensChild->findChildId(servId);
                if (nullptr == serviceChild)
                {
                    ensChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, ensChild));
                }
            }
        }
        else
        {
            SLModelItem *serviceChild = ensChild->findChildId(servId);
            if (nullptr == serviceChild)
            {
                ensChild->appendChild(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId, ensChild));
            }
        }

        endResetModel();
        sort(0);
        return;
    }

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
    if (nullptr == serviceChild)
    {
        return;
    }

    if (m_filterCurrentEnsemble)
    {  // filtered row numbers do not match real child indices -> reset instead of fine-grained signalling
        beginResetModel();
        serviceChild->parentItem()->removeChildId(servId);
        endResetModel();
        return;
    }

    // beginRemoveRows(index(ensChild->row(), 0, QModelIndex()), serviceChild->row(), serviceChild->row());
    beginRemoveRows(index(serviceChild->parentItem()->row(), 0, QModelIndex()), serviceChild->row(), serviceChild->row());
    serviceChild->parentItem()->removeChildId(servId);
    endRemoveRows();
}

void SLTreeModel::removeEnsemble(const ServiceListId &ensId)
{
    SLModelItem *ensChild = m_rootItem->findChildId(ensId);
    if (nullptr == ensChild)
    {  // not found (it shoud not happen)
        return;
    }

    if (m_filterCurrentEnsemble)
    {  // filtered row numbers do not match real child indices -> reset instead of fine-grained signalling
        beginResetModel();
        m_rootItem->removeChildId(ensId);
        endResetModel();
        return;
    }

    beginRemoveRows(QModelIndex(), ensChild->row(), ensChild->row());
    m_rootItem->removeChildId(ensId);
    endRemoveRows();
}

void SLTreeModel::setFilterCurrentEnsembleOnly(bool enabled)
{
    if (m_filterCurrentEnsemble == enabled)
    {
        return;
    }

    beginResetModel();
    m_filterCurrentEnsemble = enabled;
    endResetModel();
}

void SLTreeModel::setCurrentEnsembleId(const ServiceListId &ensId)
{
    if (m_currentEnsembleId == ensId)
    {
        return;
    }

    if (m_filterCurrentEnsemble)
    {
        beginResetModel();
        m_currentEnsembleId = ensId;
        endResetModel();
    }
    else
    {
        m_currentEnsembleId = ensId;
    }
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

QHash<int, QByteArray> SLTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

    roles[Qt::DisplayRole] = "serviceName";
    roles[SLModelRole::IsFavoriteRole] = "isFavorite";
    roles[SLModelRole::IdRole] = "serviceId";
    roles[SLModelRole::SIdHexRole] = "sidHex";
    roles[SLModelRole::SmallLogoRole] = "smallLogo";
    roles[SLModelRole::SmallLogoIdRole] = "smallLogoId";
    roles[SLModelRole::EpgModelRole] = "epgModelRole";
    roles[SLModelRole::EnsembleListRole] = "ueidList";
    roles[SLModelRole::ChannelRole] = "channel";

    return roles;
}
