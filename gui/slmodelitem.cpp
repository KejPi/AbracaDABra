/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "slmodelitem.h"

#include <QFont>
#include <QIcon>
#include <QStringList>

#include "dabtables.h"
#include "slmodel.h"

SLModelItem::SLModelItem(const ServiceList *slPtr, const MetadataManager *mmPtr, SLModelItem *parent)
{
    m_parentItem = parent;
    m_slPtr = slPtr;
    m_metadataMgrPtr = mmPtr;
}

SLModelItem::SLModelItem(const ServiceList *slPtr, const MetadataManager *mmPtr, const ServiceListId &id, SLModelItem *parent)
{
    m_parentItem = parent;
    m_slPtr = slPtr;
    m_metadataMgrPtr = mmPtr;
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
                        QString logo;
                        if (!m_metadataMgrPtr->data(m_id, MetadataManager::SmallLogo).value<QPixmap>().isNull())
                        {
                            QBuffer buffer;
                            buffer.open(QIODevice::WriteOnly);
                            m_metadataMgrPtr->data(m_id, MetadataManager::SmallLogo).value<QPixmap>().save(&buffer, "PNG");
                            logo = QString("<img src='data:image/png;base64, %1'/>").arg(QString(buffer.data().toBase64()));
                        }
                        QString tooltip = QString(
                                              "<table border='0' cellspacing='2' cellpadding='2'>"
                                              "<tr>"
                                              "<td valign='middle'>%4</td>"
                                              "<td valign='middle'>"
                                              "<p style='margin: 0;'><b>%1</b> %2</p>"
                                              "<p style='margin: 0;'><b>SId:</b> %3</p>"
                                              "</td>"
                                              "</tr>"
                                              "</table>")
                                              .arg(QObject::tr("Short label:"), it.value()->shortLabel().toHtmlEscaped(),
                                                   QString("%1").arg(it.value()->SId().countryServiceRef(), 4, 16, QChar('0')).toUpper(), logo);
                        return QVariant(tooltip);
                    }
                }
                if (m_id.isEnsemble())
                {  // ensemble item
                    EnsembleListConstIterator it = m_slPtr->findEnsemble(m_id);
                    if (m_slPtr->ensembleListEnd() != it)
                    {  // found
                        return QVariant(QString(QObject::tr("Channel %1<br>Frequency: %2 MHz"))
                                            .arg(DabTables::channelList.value(it.value()->frequency()))
                                            .arg(it.value()->frequency() / 1000.0, 3, 'f', 3, QChar('0')));
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
            case SLModelRole::IdRole:
                return QVariant::fromValue(m_id.value());
            case SLModelRole::SmallLogoRole:
                return m_metadataMgrPtr->data(m_id, MetadataManager::SmallLogo);
            case SLModelRole::SmallLogoIdRole:
            {
                if (m_metadataMgrPtr->data(m_id, MetadataManager::SmallLogo).value<QPixmap>().isNull())
                {
                    return QVariant(0);
                }
                return QVariant::fromValue(m_id.value());
            }
            case SLModelRole::EpgModelRole:
                return QVariant::fromValue(m_metadataMgrPtr->epgModel(m_id));
            case SLModelRole::EnsembleListRole:
            {
                ServiceListConstIterator it = m_slPtr->findService(m_id);
                if (m_slPtr->serviceListEnd() != it)
                {  // found
                    auto service = it.value();
                    QList<int> ensList;
                    for (int e = 0; e < service->numEnsembles(); ++e)
                    {
                        ensList.append(service->getEnsemble(e)->ueid());
                    }
                    return QVariant::fromValue(ensList);
                }
            }
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
        return m_parentItem->m_childItems.indexOf(const_cast<SLModelItem *>(this));
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
#if 0
    if (m_id.isService())
    {
        ServiceListConstIterator it = m_slPtr->findService(m_id);
        if (m_slPtr->serviceListEnd() != it)
        {  // found
            return it.value()->isFavorite();
        }
    }
    return false;
#else
    return m_slPtr->isServiceFavorite(m_id);
#endif
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
        std::sort(m_childItems.begin(), m_childItems.end(),
                  [](SLModelItem *a, SLModelItem *b)
                  {
                      if (a->isEnsemble())
                      {
                          return a->frequency() < b->frequency();
                      }
                      else
                      {
                          return a->label().toUpper() < b->label().toUpper();
                      }
                  });
    }
    else
    {
        std::sort(m_childItems.begin(), m_childItems.end(),
                  [](SLModelItem *a, SLModelItem *b)
                  {
                      if (a->isEnsemble())
                      {
                          return a->frequency() > b->frequency();
                      }
                      else
                      {
                          return a->label().toUpper() > b->label().toUpper();
                      }
                  });
    }

    for (auto child : m_childItems)
    {
        child->sort(order);
    }
}

SLModelItem *SLModelItem::findChildId(const ServiceListId &id, bool recursive) const
{
    for (auto item : m_childItems)
    {
        if (item->id() == id)
        {
            return item;
        }
        if (recursive)
        {
            SLModelItem *childPtr = item->findChildId(id);
            if (nullptr != childPtr)
            {
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
        {  // found
            SLModelItem *item = m_childItems.at(n);
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
