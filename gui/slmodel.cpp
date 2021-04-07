#include "slmodelitem.h"
#include "slmodel.h"

#include <QStringList>

SLModel::SLModel(ServiceList *sl, QObject *parent)
    : QAbstractItemModel(parent)
    , slPtr(sl)
{
    rootItem = new SLModelItem();
}


SLModel::~SLModel()
{
    delete rootItem;
}

int SLModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<SLModelItem*>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}


QVariant SLModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->data(index.column(), role);
}

bool SLModel::isService(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->isService();
}

bool SLModel::isEnsemble(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->isEnsemble();
}

uint64_t SLModel::getId(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    SLModelItem *item = static_cast<SLModelItem*>(index.internalPointer());

    return item->getId();
}

Qt::ItemFlags SLModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return 0;
    }

    return QAbstractItemModel::flags(index);
}


QVariant SLModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
//    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
//        return rootItem->data(section);

    return QVariant();
}


QModelIndex SLModel::index(int row, int column, const QModelIndex &parent)
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


QModelIndex SLModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SLModelItem *childItem = static_cast<SLModelItem*>(index.internalPointer());
    SLModelItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}


int SLModel::rowCount(const QModelIndex &parent) const
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

void SLModel::addService(const ServiceListItem * s)
{  // new service in service list
    beginInsertRows(QModelIndex(), rootItem->childCount(), rootItem->childCount());
    rootItem->appendChild(new SLModelItem(s, rootItem));
    endInsertRows();

    sort(0);
}

void SLModel::addEnsemble(const EnsembleListItem * e)
{ // go through items and add ensemble

}

void SLModel::clear()
{

}

void SLModel::sort(int column, Qt::SortOrder order)
{
    //qDebug() << Q_FUNC_INFO;
    Q_UNUSED(column);
    rootItem->sort(order);

    emit dataChanged(QModelIndex(), QModelIndex());
}

