#include "servicelistitem.h"
#include "ensemblelistitem.h"

ServiceListItem::ServiceListItem(const RadioControlServiceComponent &item, bool fav, int currentEns) : m_id(item)
{
    m_sid = item.SId;
    m_scids = item.SCIdS;
    m_label = item.label;
    m_shortLabel = item.labelShort;
    m_favorite = fav;
    m_currentEnsemble = currentEns;
}

bool ServiceListItem::addEnsemble(EnsembleListItem * ensPtr)
{
    QList<EnsembleListItem *>::iterator it = findEnsemble(ensPtr->id());
    if (m_ensembleList.end() == it)
    {
        m_ensembleList.append(ensPtr);
        return true;
    }
    return false;
}

const EnsembleListItem * ServiceListItem::switchEnsemble(const ServiceListId & id)
{
    if (!id.isValid())
    {
        if (m_ensembleList.size() > 0)
        {
            m_currentEnsemble = (m_currentEnsemble + 1) % m_ensembleList.size();
        }
        return m_ensembleList.at(m_currentEnsemble);
    }
    else
    {
        for (int n = 0; n < m_ensembleList.size(); ++n)
        {
            if (m_ensembleList.at(n)->id() == id)
            {
                m_currentEnsemble = n;
                break;
            }
        }
        return m_ensembleList.at(m_currentEnsemble);
    }
}

bool ServiceListItem::operator==(const ServiceListItem & other) const
{
    return id() == other.id();
}

QList<EnsembleListItem *>::iterator ServiceListItem::findEnsemble(const ServiceListId &  id)
{
    QList<EnsembleListItem *>::iterator it;
    for (it = m_ensembleList.begin(); it < m_ensembleList.end(); ++it)
    {
        if ((*it)->id() == id)
        {
            return it;
        }
    }

    return it;
}

const EnsembleListItem * ServiceListItem::getEnsemble(int num) const
{
    if (num < 0)
    {   // best ensemble
#warning "Best ensemble to be implemented"
        return m_ensembleList.at(m_currentEnsemble);
    }
    else
    {   // wrapping over the end
        return m_ensembleList.at(num % m_ensembleList.size());
    }
    return m_ensembleList.at(0);
}
