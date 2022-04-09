#include <QStringList>
#include <QFont>
#include <QIcon>

#include "slmodelitem.h"
#include "slmodel.h"
#include "dabtables.h"

SLModelItem::SLModelItem(const ServiceList *slPtr, SLModelItem *parent)
{    
    m_parentItem = parent;
    m_slPtr = slPtr;
}

SLModelItem::SLModelItem(const ServiceList *  slPtr, const ServiceListItem *sPtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_slPtr = slPtr;
    m_serviceId = sPtr->getId();
}

SLModelItem::SLModelItem(const ServiceList *slPtr, const EnsembleListItem *ePtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_slPtr = slPtr;
    m_ensembleId = ePtr->getId();
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
            return QVariant(label());
            break;
        case Qt::ToolTipRole:
        {
            if (0 != m_serviceId)
            {  // service item
                ServiceListConstIterator it = m_slPtr->findService(m_serviceId);
                if (m_slPtr->serviceListEnd() != it)
                {  // found
                    QString tooltip = QString("<b>Short label:</b> %1<br><b>SId:</b> 0x%2").arg(it.value()->shortLabel(),
                                          QString("%1").arg(it.value()->SId().countryServiceRef(), 4, 16, QChar('0')).toUpper() );
                    return QVariant(tooltip);

                }
            }
            if (0 != m_ensembleId)
            {  // ensemble item
                EnsembleListConstIterator it = m_slPtr->findEnsemble(m_ensembleId);
                if (m_slPtr->ensembleListEnd() != it)
                {  // found
                    return QVariant(QString("Channel %1<br>Frequency: %2 MHz")
                                        .arg(DabTables::channelList.value(it.value()->frequency()))
                                        .arg(it.value()->frequency()/1000.0));
                }
            }
        }
            break;
        case Qt::FontRole:
            if (0 != m_ensembleId)
            {
                QFont f;
                f.setBold(true);
                return QVariant(f);
            }
            return QVariant();
         break;
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
    if (0 != m_serviceId)
    {
        return m_serviceId;
    }
    if (0 != m_ensembleId)
    {
        return m_ensembleId;
    }
    return 0;
}

bool SLModelItem::isService() const
{
    return (0 != m_serviceId);
}

bool SLModelItem::isFavoriteService() const
{
    if (0 != m_serviceId)
    {
        ServiceListConstIterator it = m_slPtr->findService(m_serviceId);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->isFavorite();
        }
    }
    return false;
}

bool SLModelItem::isEnsemble() const
{
    return (0 != m_ensembleId);
}

QString SLModelItem::label() const
{
    if (0 != m_serviceId)
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_serviceId);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->label();
        }
    }

    if (0 != m_ensembleId)
    {  // ensemble item
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_ensembleId);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->label().trimmed();
        }
    }
    return QString("--- UNKNOWN ---");
}

QString SLModelItem::shortLabel() const
{
    if (0 != m_serviceId)
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_serviceId);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->shortLabel();
        }
    }

    if (0 != m_ensembleId)
    {  // ensemble item
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_ensembleId);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->shortLabel().trimmed();
        }
    }
    return QString("-UNKN-");
}

DabSId SLModelItem::SId() const
{
    if (0 != m_serviceId)
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_serviceId);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->SId();
        }
    }

    return DabSId();
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
    if (0 != m_ensembleId)
    {
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_ensembleId);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->frequency();
        }

    }
    return 0;
}


