#include <QStringList>
#include <QFont>
#include <QIcon>

#include "slmodelitem.h"
#include "slmodel.h"
#include "dabtables.h"

SLModelItem::SLModelItem(SLModelItem *parent)
{
    m_parentItem = parent;
}

SLModelItem::SLModelItem(const ServiceListItem *sPtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_servicePtr = sPtr;
}

SLModelItem::SLModelItem(const EnsembleListItem *ePtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_ensemblePtr = ePtr;
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
                return QVariant(m_ensemblePtr->label().trimmed());
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
                return QVariant(QString("Channel %1<br>Frequency: %2 MHz")
                                .arg(DabTables::channelList.value(m_ensemblePtr->frequency()))
                                .arg(m_ensemblePtr->frequency()/1000.0));
            }
        case Qt::FontRole:
            if (nullptr != m_ensemblePtr)
            {
                QFont f;
                f.setBold(true);
                return QVariant(f);
            }
            return QVariant();
         break;
//        case Qt::DecorationRole:
//        {
//            if (nullptr != m_ensemblePtr)
//            {
//                QPixmap pic;
//                if (pic.load(":/resources/broadcast.svg"))
//                {
//                    return QVariant(QIcon(pic));
//                }
//            }
//            return QVariant();
//        }
//            break;
        }        
    }
    return QVariant();
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
            if (a->isEnsemble())
            {
                return a->frequency() < b->frequency();
            }
            else
            {
                return a->label() < b->label();
            }
        });
    }
    else
    {
        std::sort(m_childItems.begin(), m_childItems.end(), [](SLModelItem* a, SLModelItem* b) {
            if (a->isEnsemble())
            {
                return a->frequency() > b->frequency();
            }
            else
            {
                return a->label() > b->label();
            }
        });
    }

    for (auto child : m_childItems)
    {
        child->sort(order);
    }
}

SLModelItem* SLModelItem::findChildId(uint64_t id) const
{
    for (auto item : m_childItems)
    {
        if (item->getId() == id)
        {
            return item;
        }
    }
    return nullptr;
}

uint32_t SLModelItem::frequency() const
{
    if (nullptr != m_ensemblePtr)
    {
        return m_ensemblePtr->frequency();
    }
    return 0;
}


