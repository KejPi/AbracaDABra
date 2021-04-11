#include "servicelistitem.h"
#include "ensemblelistitem.h"

ServiceListItem::ServiceListItem(const RadioControlServiceListEntry & item, bool fav, int currentEns)
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
    QList<EnsembleListItem *>::iterator it = findEnsemble(ensPtr->getId());
    if (m_ensembleList.end() == it)
    {
        m_ensembleList.append(ensPtr);
        return true;
    }
    return false;
}

const EnsembleListItem * ServiceListItem::switchEnsemble(uint64_t id)
{
    if (0 == id)
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
            if (m_ensembleList.at(n)->getId() == id)
            {
                m_currentEnsemble = n;
                break;
            }
        }
        return m_ensembleList.at(m_currentEnsemble);
    }
}

bool ServiceListItem::operator==(const ServiceListItem & other)
{
    return getId() == other.getId();
}

QList<EnsembleListItem *>::iterator ServiceListItem::findEnsemble(uint64_t id)
{
    QList<EnsembleListItem *>::iterator it;
    for (it = m_ensembleList.begin(); it < m_ensembleList.end(); ++it)
    {
        if ((*it)->getId() == id)
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
