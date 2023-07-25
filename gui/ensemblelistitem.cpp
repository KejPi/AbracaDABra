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

void EnsembleListItem::beginUpdate()
{
    for (auto & s : m_serviceList)
    {
        s->setIsObsolete(true);
    }
}

void EnsembleListItem::endUpdate()
{
    // remove obsolete services from list
    QList<ServiceListItem *>::iterator it = m_serviceList.begin();
    while (it != m_serviceList.end())
    {
        if ((*it)->isObsolete())
        {
            it = m_serviceList.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
