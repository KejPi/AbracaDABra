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

SLModelItem::SLModelItem(const ServiceList *  slPtr, const ServiceListId & id, SLModelItem *parent)
{
    m_parentItem = parent;
    m_slPtr = slPtr;
    m_id = id;
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
            if (m_id.isService())
            {  // service item
                ServiceListConstIterator it = m_slPtr->findService(m_id);
                if (m_slPtr->serviceListEnd() != it)
                {  // found
                    QString tooltip = QString("<b>Short label:</b> %1<br><b>SId:</b> 0x%2").arg(it.value()->shortLabel(),
                                          QString("%1").arg(it.value()->SId().countryServiceRef(), 4, 16, QChar('0')).toUpper() );
                    return QVariant(tooltip);

                }
            }
            if (m_id.isEnsemble())
            {  // ensemble item
                EnsembleListConstIterator it = m_slPtr->findEnsemble(m_id);
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
            if (m_id.isEnsemble())
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

ServiceListId SLModelItem::id() const
{
    return m_id;
}

bool SLModelItem::isService() const
{
    return m_id.isService();
}

bool SLModelItem::isFavoriteService() const
{
    if (m_id.isService())
    {
        ServiceListConstIterator it = m_slPtr->findService(m_id);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->isFavorite();
        }
    }
    return false;
}

bool SLModelItem::isEnsemble() const
{
    return m_id.isEnsemble();
}

QString SLModelItem::label() const
{
    if (m_id.isService())
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_id);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->label();
        }
    }

    if (m_id.isEnsemble())
    {  // ensemble item
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_id);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->label().trimmed();
        }
    }
    return QString("--- UNKNOWN ---");
}

QString SLModelItem::shortLabel() const
{
    if (m_id.isService())
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_id);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->shortLabel();
        }
    }

    if (m_id.isEnsemble())
    {  // ensemble item
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_id);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->shortLabel().trimmed();
        }
    }
    return QString("-UNKN-");
}

DabSId SLModelItem::SId() const
{
    if (m_id.isService())
    {  // service item
        ServiceListConstIterator it = m_slPtr->findService(m_id);
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

SLModelItem* SLModelItem::findChildId(const ServiceListId & id, bool recursive) const
{
    for (auto item : m_childItems)
    {
        if (item->id() == id)
        {
            return item;
        }
        if (recursive)
        {
            SLModelItem* childPtr = item->findChildId(id);
            if (nullptr != childPtr)
            {
                qDebug() << Q_FUNC_INFO;
                return childPtr;
            }
        }
    }
    return nullptr;
}

bool SLModelItem::removeChildId(const ServiceListId &id)
{
    for (int n = 0; n < m_childItems.size(); ++n)
    {
        if (m_childItems.at(n)->id() == id)
        {   // found
            SLModelItem * item = m_childItems.at(n);
            m_childItems.removeAt(n);
            delete item;
            return true;
        }
    }
    return false;
}

uint32_t SLModelItem::frequency() const
{
    if (m_id.isEnsemble())
    {
        EnsembleListConstIterator it = m_slPtr->findEnsemble(m_id);
        if (m_slPtr->ensembleListEnd() != it)
        {  // found
            return it.value()->frequency();
        }

    }
    return 0;
}


