/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
    m_isObsolete = false;
}

bool ServiceListItem::update(const RadioControlServiceComponent &item)
{
    if (m_label != item.label)
    {
        m_label = item.label;
        m_shortLabel = item.labelShort;
        return true;
    }
    if (m_shortLabel != item.labelShort)
    {
        m_shortLabel = item.labelShort;
        return true;
    }
    return false;
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

// returns false service is no longer valid ==> no ensembles
bool ServiceListItem::removeEnsemble(EnsembleListItem *ensPtr)
{
    QList<EnsembleListItem *>::const_iterator it = findEnsemble(ensPtr->id());
    if (m_ensembleList.cend() != it)
    {   // found ensemble
        m_ensembleList.erase(it);
        m_isObsolete = false;
        return (0 != m_ensembleList.size());
    }
    return true;
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

bool ServiceListItem::isObsolete() const
{
    return m_isObsolete;
}

void ServiceListItem::setIsObsolete(bool isObsolete)
{
    m_isObsolete = isObsolete;
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
        // #warning "Best ensemble to be implemented"
        return m_ensembleList.at(m_currentEnsemble);
    }
    else
    {   // wrapping over the end
        return m_ensembleList.at(num % m_ensembleList.size());
    }
    return m_ensembleList.at(0);
}
