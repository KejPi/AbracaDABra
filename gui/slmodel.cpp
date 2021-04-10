#include "slmodel.h"
#include <QFlags>

SLModel::SLModel(ServiceList *sl, QObject *parent)
    : QAbstractItemModel(parent)
    , slPtr(sl)
{
    QPixmap nopic(20,20);
    nopic.fill(Qt::transparent);
    noIcon = QIcon(nopic);

    QPixmap pic;
    if (pic.load(":/resources/star.svg"))
    {
        favIcon = QIcon(pic);
    }
    else
    {
        favIcon = noIcon;
        qDebug() << "Unable to load :/resources/star.svg";
    }
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
        return QVariant();

    const ServiceListItem * item = static_cast<ServiceListItem*>(index.internalPointer());

    if (0 == index.column())
    {
        switch (role)
        {
        case Qt::DisplayRole:
            return QVariant(item->label());
            break;
        case Qt::ToolTipRole:
        {
            QString tooltip = QString("<b>Short label:</b> %1<br><b>SId:</b> 0x%2")
                    .arg(item->shortLabel())
                    .arg( QString("%1").arg(item->SId().prog.countryServiceRef, 4, 16, QChar('0')).toUpper() );
            return QVariant(tooltip);
        }
            break;
        case Qt::DecorationRole:
        {
            if (item->isFavorite())
            {
                return QVariant(favIcon);
            }
            else
            {
                return QVariant(noIcon);
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
        return false;

    const ServiceListItem * item = static_cast<ServiceListItem*>(index.internalPointer());
    return item->isFavorite();
}

uint64_t SLModel::getId(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    const ServiceListItem * item = static_cast<ServiceListItem*>(index.internalPointer());

    return item->getId();
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
        return QModelIndex();

    if (parent.isValid())
    {
        return QModelIndex();
    }

    const ServiceListItem *childItem = m_serviceItems.at(row);
    if (childItem)
        return createIndex(row, column, (void *)childItem);
    else
        return QModelIndex();
}


QModelIndex SLModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}


int SLModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    return m_serviceItems.size();
}

void SLModel::addService(const ServiceListItem *s)
{  // new service in service list
    beginInsertRows(QModelIndex(), m_serviceItems.size(), m_serviceItems.size());
    m_serviceItems.append(s);
    endInsertRows();

    sort(0);
}

void SLModel::clear()
{
    qDebug() << Q_FUNC_INFO;
    beginResetModel();
    // remove all items
    m_serviceItems.clear();
    endResetModel();
}

void SLModel::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);
    if (Qt::AscendingOrder == order)
    {
        std::sort(m_serviceItems.begin(), m_serviceItems.end(), [](const ServiceListItem * a, const ServiceListItem * b) {
            if ((a->isFavorite() && b->isFavorite()) || (!a->isFavorite() && !b->isFavorite()))
                return a->label() < b->label();
            if (a->isFavorite())
                return true;
            return false;
        });
    }
    else
    {
        std::sort(m_serviceItems.begin(), m_serviceItems.end(), [](const ServiceListItem * a, const ServiceListItem * b) {
            if ((a->isFavorite() && b->isFavorite()) || (!a->isFavorite() && !b->isFavorite()))
                return a->label() > b->label();
            if (b->isFavorite())
                return true;
            return false;
        });
    }

    emit dataChanged(QModelIndex(), QModelIndex());
}

