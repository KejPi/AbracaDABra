#include "ensemblelistitem.h"
#include "servicelistitem.h"

EnsembleListItem::EnsembleListItem(const RadioControlService & item)
{
    m_frequency = item.ensemble.frequency;
    m_ueid = item.ensemble.ueid;
    m_label = item.ensemble.label;
    m_shortLabel = item.ensemble.labelShort;
}

bool EnsembleListItem::operator==(const EnsembleListItem & other)
{
    return getId() == other.getId();
}

bool EnsembleListItem::addService(ServiceListItem * servPtr)
{
    QList<ServiceListItem *>::iterator it = findService(servPtr->getId());
    if (m_serviceList.end() == it)
    {
        m_serviceList.append(servPtr);
        return true;
    }
    return false;
}

QList<ServiceListItem *>::iterator EnsembleListItem::findService(uint64_t id)
{
    QList<ServiceListItem *>::iterator it;
    for (it = m_serviceList.begin(); it < m_serviceList.end(); ++it)
    {
        if ((*it)->getId() == id)
        {
            return it;
        }
    }

    return it;
}

const ServiceListItem * EnsembleListItem::getService(int num) const
{
    if (num < m_serviceList.size())
    {
        return m_serviceList.at(num);
    }
    return nullptr;
}
