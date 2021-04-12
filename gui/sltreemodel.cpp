#include "sltreemodel.h"

SLTreeModel::SLTreeModel(ServiceList *sl, QObject *parent)
    : QAbstractItemModel(parent)
    , slPtr(sl)
{
    rootItem = new SLModelItem();
}


SLTreeModel::~SLTreeModel()
{
    delete rootItem;
}

int SLTreeModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 1;
}


QVariant SLTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->data(index.column(), role);
}

bool SLTreeModel::isService(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->isService();
}

bool SLTreeModel::isFavoriteService(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->isFavoriteService();
}

bool SLTreeModel::isEnsemble(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->isEnsemble();
}

uint64_t SLTreeModel::getId(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->getId();
}

Qt::ItemFlags SLTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return QFlags<Qt::ItemFlag>();
    }

    return QAbstractItemModel::flags(index);
}


//QVariant SLTreeModel::headerData(int section, Qt::Orientation orientation,
//                               int role) const
//{
////    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
////        return rootItem->data(section);

//    return QVariant();
//}


QModelIndex SLTreeModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SLModelItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<SLModelItem*>(parent.internalPointer());

    SLModelItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}


QModelIndex SLTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SLModelItem *childItem = static_cast<SLModelItem*>(index.internalPointer());
    SLModelItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}


int SLTreeModel::rowCount(const QModelIndex &parent) const
{
    SLModelItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<SLModelItem*>(parent.internalPointer());

    return parentItem->childCount();
}

void SLTreeModel::addEnsembleService(const EnsembleListItem *e, const ServiceListItem *s)
{  // new service in service list

    SLModelItem * ensChild = rootItem->findChildId(e->getId());
    if (nullptr == ensChild)
    {   // not found ==> new ensemble
        ensChild = new SLModelItem(e, rootItem);
        beginInsertRows(QModelIndex(), rootItem->childCount(), rootItem->childCount());
        rootItem->appendChild(ensChild);
        endInsertRows();
    }
    SLModelItem * serviceChild = ensChild->findChildId(s->getId());
    if (nullptr == serviceChild)
    {  // new service to be added
        beginInsertRows(index(ensChild->row(), 0, QModelIndex()), ensChild->childCount(), ensChild->childCount());
        ensChild->appendChild(new SLModelItem(s, ensChild));
        endInsertRows();
    }

    sort(0);
}

void SLTreeModel::clear()
{
    beginResetModel();
    // remove all items
    delete rootItem;
    // create new root
    rootItem = new SLModelItem();
    endResetModel();
}

void SLTreeModel::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);
    rootItem->sort(order);

    emit dataChanged(QModelIndex(), QModelIndex());
}

