/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "slmodel.h"
#include <QFlags>

SLModel::SLModel(const ServiceList *sl, const MetadataManager *mm, QObject *parent)
    : QAbstractItemModel(parent)
    , m_slPtr(sl)
    , m_metadataMgrPtr(mm)
{

    connect(m_metadataMgrPtr, &MetadataManager::epgModelAvailable, this, &SLModel::epgModelAvailable);

    QPixmap nopic(20,20);
    nopic.fill(Qt::transparent);
    m_noIcon = QIcon(nopic);

    QPixmap pic;
#ifdef Q_OS_LINUX
    // SVG is too big on linux, using PNG instead
    if (pic.load(":/resources/star.png"))
    {
        m_favIcon = QIcon(pic);
    }
    else
    {
        m_favIcon = m_noIcon;
        qDebug() << "Unable to load :/resources/star.png";
    }
#else
    if (pic.load(":/resources/star.svg"))
    {
        m_favIcon = QIcon(pic);
    }
    else
    {
        m_favIcon = m_noIcon;
        qDebug() << "Unable to load :/resources/star.svg";
    }
#endif
}


SLModel::~SLModel()
{
}

int SLModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant SLModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    const SLModelItem * item = static_cast<SLModelItem*>(index.internalPointer());

    if (0 == index.column())
    {
        switch (role)
        {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
        case SLModelRole::IdRole:
        case SLModelRole::SmallLogoTole:
        case SLModelRole::EpgModelRole:
              return item->data(index.column(), role);
        case Qt::DecorationRole:
        {
            if (item->isFavoriteService())
            {
                return QVariant(m_favIcon);
            }
            else
            {
                return QVariant(m_noIcon);
            }
            return QVariant();
        }
        break;
        }
    }
    return QVariant();
}

bool SLModel::isFavoriteService(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return false;
    }

    const SLModelItem * item = static_cast<SLModelItem*>(index.internalPointer());
    return item->isFavoriteService();
}

ServiceListId SLModel::id(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return 0;
    }

    const SLModelItem * item = static_cast<SLModelItem*>(index.internalPointer());

    return item->id();
}

Qt::ItemFlags SLModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return QFlags<Qt::ItemFlag>();
    }

    return QAbstractItemModel::flags(index);
}


QModelIndex SLModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
    {
        return QModelIndex();
    }

    if (parent.isValid())
    {
        return QModelIndex();
    }

    const SLModelItem *childItem = m_serviceItems.at(row);
    if (childItem)
    {
        return createIndex(row, column, (void *)childItem);
    }
    else
    {
        return QModelIndex();
    }
}


QModelIndex SLModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}


int SLModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
    {
        return 0;
    }

    return m_serviceItems.size();
}

void SLModel::addService(const ServiceListId & servId)
{  // new service in service list
    beginInsertRows(QModelIndex(), m_serviceItems.size(), m_serviceItems.size());
    m_serviceItems.append(new SLModelItem(m_slPtr, m_metadataMgrPtr, servId));
    endInsertRows();

    sort(0);
}

void SLModel::updateService(const ServiceListId & servId)
{   // service label was updated -> need to sort
    sort(0);  // --> this emits dataChanged()
}

void SLModel::removeService(const ServiceListId & servId)
{
    // first find service in the list
    for (int row = 0; row < m_serviceItems.size(); ++row)
    {
        if (m_serviceItems.at(row)->id() == servId)
        {   // found
            beginRemoveRows(QModelIndex(), row, row);
            SLModelItem * item = m_serviceItems.at(row);
            m_serviceItems.removeAt(row);
            delete item;
            endRemoveRows();
            return;
        }
    }
}

void SLModel::epgModelAvailable(const ServiceListId &servId)
{
    qDebug() << Q_FUNC_INFO;
    // first find service in the list
    for (int row = 0; row < m_serviceItems.size(); ++row)
    {
        if (m_serviceItems.at(row)->id() == servId)
        {   // found
            qDebug() << Q_FUNC_INFO << "data changed";
            dataChanged(index(row, 0), index(row, 0), {SLModelRole::EpgModelRole});
            return;
        }
    }
}

void SLModel::clear()
{
    beginResetModel();
    // remove all items
    m_serviceItems.clear();
    endResetModel();
}

void SLModel::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);

    beginResetModel();

    if (Qt::AscendingOrder == order)
    {
        std::sort(m_serviceItems.begin(), m_serviceItems.end(), [](const SLModelItem * a, const SLModelItem * b) {
            if ((a->isFavoriteService() && b->isFavoriteService()) || (!a->isFavoriteService() && !b->isFavoriteService()))
            {
                return a->label().toUpper() < b->label().toUpper();
            }
            if (a->isFavoriteService())
            {
                return true;
            }
            return false;
        });
    }
    else
    {
        std::sort(m_serviceItems.begin(), m_serviceItems.end(), [](const SLModelItem * a, const SLModelItem * b) {
            if ((a->isFavoriteService() && b->isFavoriteService()) || (!a->isFavoriteService() && !b->isFavoriteService()))
            {
                return a->label().toUpper() > b->label().toUpper();
            }
            if (b->isFavoriteService())
            {
                return true;
            }
            return false;
        });
    }

    endResetModel();

    emit dataChanged(QModelIndex(), QModelIndex());
}

QHash<int, QByteArray> SLModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

    roles[SLModelRole::IdRole] = "serviceId";
    roles[SLModelRole::SmallLogoTole] = "smallLogo";
    roles[SLModelRole::EpgModelRole] = "epgModelRole";

    return roles;
}

