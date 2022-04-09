#include "ensemblelistitem.h"
#include "servicelistitem.h"

EnsembleListItem::EnsembleListItem(const RadioControlEnsemble & ens) : m_id(ens)
{    
    m_frequency = ens.frequency;
    m_ueid = ens.ueid;
    m_label = ens.label;
    m_shortLabel = ens.labelShort;
}

bool EnsembleListItem::operator==(const EnsembleListItem & other) const
{
    return id() == other.id();
}

bool EnsembleListItem::addService(ServiceListItem * servPtr)
{
    QList<ServiceListItem *>::iterator it = findService(servPtr->id());
    if (m_serviceList.end() == it)
    {
        m_serviceList.append(servPtr);
        return true;
    }
    return false;
}

QList<ServiceListItem *>::iterator EnsembleListItem::findService(const ServiceListId & id)
{
    QList<ServiceListItem *>::iterator it;
    for (it = m_serviceList.begin(); it < m_serviceList.end(); ++it)
    {
        if ((*it)->id() == id)
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
