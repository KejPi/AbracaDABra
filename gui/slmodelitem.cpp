#include <QStringList>
#include <QFont>
#include <QIcon>

#include "slmodelitem.h"
#include "slmodel.h"

SLModelItem::SLModelItem(SLModelItem *parent)
{
    m_parentItem = parent;
}

SLModelItem::SLModelItem(const ServiceListItem *sPtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_servicePtr = sPtr;
    loadIcon();
}

SLModelItem::SLModelItem(const EnsembleListItem *ePtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_ensemblePtr = ePtr;
    loadIcon();
}


SLModelItem::~SLModelItem()
{
    qDeleteAll(m_childItems);
}

void SLModelItem::appendChild(SLModelItem *item)
{
    m_childItems.append(item);
}


SLModelItem *SLModelItem::child(int row)
{
    return m_childItems.value(row);
}

int SLModelItem::childCount() const
{
    return m_childItems.count();
}

int SLModelItem::columnCount() const
{
    return 1;
}

QVariant SLModelItem::data(int column, int role) const
{
    if (0 == column)
    {
        switch (role)
        {
        case Qt::DisplayRole:
            if (nullptr != m_servicePtr)
            {  // service item
                return QVariant(m_servicePtr->label());
            }

            if (nullptr != m_ensemblePtr)
            {  // ensemble item
                return QVariant(m_ensemblePtr->label());
            }
            break;
        case Qt::ToolTipRole:
            if (nullptr != m_servicePtr)
            {  // service item
                QString tooltip = QString("<b>Short label:</b> %1<br><b>SId:</b> 0x%2")
                        .arg(m_servicePtr->shortLabel())
                        .arg( QString("%1").arg(m_servicePtr->SId().prog.countryServiceRef, 4, 16, QChar('0')).toUpper() );
                return QVariant(tooltip);
            }

            if (nullptr != m_ensemblePtr)
            {  // ensemble item
                return QVariant(QString("Frequency: %1").arg(m_ensemblePtr->frequency()));
            }

            break;
        case Qt::DecorationRole:
        {
            if ((nullptr != m_servicePtr) && (m_servicePtr->isFavorite()))
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

void SLModelItem::loadIcon()
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

SLModelItem *SLModelItem::parentItem()
{
    return m_parentItem;
}

int SLModelItem::row() const
{
    if (m_parentItem)
    {
        return m_parentItem->m_childItems.indexOf(const_cast<SLModelItem*>(this));
    }

    return 0;
}

uint64_t SLModelItem::getId() const
{
    if (nullptr != m_servicePtr)
    {
        return m_servicePtr->getId();
    }
    if (nullptr != m_ensemblePtr)
    {
        return m_ensemblePtr->getId();
    }
    return 0;
}

bool SLModelItem::isService() const
{
    return (nullptr != m_servicePtr);
}

bool SLModelItem::isFavoriteService() const
{
    return ((nullptr != m_servicePtr) && m_servicePtr->isFavorite());
}

bool SLModelItem::isEnsemble() const
{
    return (nullptr != m_ensemblePtr);
}

QString SLModelItem::label()
{
    if (nullptr != m_servicePtr)
    {
        return m_servicePtr->label();
    }
    if (nullptr != m_ensemblePtr)
    {
        return m_ensemblePtr->label();
    }
    return QString();
}

void SLModelItem::sort(Qt::SortOrder order)
{
    if (Qt::AscendingOrder == order)
    {
        std::sort(m_childItems.begin(), m_childItems.end(), [](SLModelItem* a, SLModelItem* b) {
            if ((a->isFavoriteService() && b->isFavoriteService()) || (!a->isFavoriteService() && !b->isFavoriteService()))
                return a->label() < b->label();
            if (a->isFavoriteService())
                return true;           
            return false;
        });
    }
    else
    {
        std::sort(m_childItems.begin(), m_childItems.end(), [](SLModelItem* a, SLModelItem* b) {
            if ((a->isFavoriteService() && b->isFavoriteService()) || (!a->isFavoriteService() && !b->isFavoriteService()))
                return a->label() > b->label();
            if (b->isFavoriteService())
                return true;
            return false;
        });
    }
}



