/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "epgbackend.h"

#include <QQmlContext>

#include "epgtime.h"

EPGBackend::EPGBackend(SLModel *serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager *metadataManager, Settings *settings,
                       QObject *parent)
    : UIControlProvider(parent), m_metadataManager(metadataManager), m_settings(settings), m_currentUEID(0), m_slSelectionModel(slSelectionModel)
{
    m_slProxyModel = new SLProxyModel(this);
    m_slProxyModel->setSourceModel(serviceListModel);
    connect(this, &EPGBackend::filterEmptyEpgChanged, [this]() { m_slProxyModel->setEmptyEpgFilter(m_settings->epg.filterEmptyEpg); });
    connect(this, &EPGBackend::filterEnsembleChanged,
            [this]() { m_slProxyModel->setUeidFilter(m_settings->epg.filterEnsemble ? m_currentUEID : 0); });
    // set initial state
    m_slProxyModel->setEmptyEpgFilter(m_settings->epg.filterEmptyEpg);
    m_slProxyModel->setUeidFilter(m_settings->epg.filterEnsemble ? m_currentUEID : 0);
}

EPGBackend::~EPGBackend()
{}

QPersistentModelIndex EPGBackend::selectedEpgItem() const
{
    return m_settings->epg.selectedItem;
}

void EPGBackend::setSelectedEpgItem(const QPersistentModelIndex &newSelectedEpgItem)
{
    if (m_settings->epg.selectedItem == newSelectedEpgItem)
    {
        return;
    }
    m_settings->epg.selectedItem = newSelectedEpgItem;
    emit selectedEpgItemChanged();
}

bool EPGBackend::filterEmptyEpg() const
{
    return m_settings->epg.filterEmptyEpg;
}

void EPGBackend::setFilterEmptyEpg(bool newFilterEmptyEpg)
{
    if (m_settings->epg.filterEmptyEpg == newFilterEmptyEpg)
    {
        return;
    }
    m_settings->epg.filterEmptyEpg = newFilterEmptyEpg;
    emit filterEmptyEpgChanged();
}

void EPGBackend::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_currentUEID = ens.ueid;
    if (m_settings->epg.filterEnsemble)
    {
        m_slProxyModel->setUeidFilter(m_currentUEID);
    }
}

bool EPGBackend::filterEnsemble() const
{
    return m_settings->epg.filterEnsemble;
}

void EPGBackend::setFilterEnsemble(bool newFilterEnsemble)
{
    if (m_settings->epg.filterEnsemble == newFilterEnsemble)
    {
        return;
    }
    m_settings->epg.filterEnsemble = newFilterEnsemble;
    emit filterEnsembleChanged();
}

void EPGBackend::scheduleRecording()
{
    if (m_settings->epg.selectedItem.isValid())
    {
        const EPGModel *model = dynamic_cast<const EPGModel *>(m_settings->epg.selectedItem.model());
        AudioRecScheduleItem item;
        item.setName(model->data(m_settings->epg.selectedItem, EPGModelRoles::NameRole).toString());
        item.setStartTime(model->data(m_settings->epg.selectedItem, EPGModelRoles::StartTimeRole).value<QDateTime>());
        item.setDurationSec(model->data(m_settings->epg.selectedItem, EPGModelRoles::DurationSecRole).toInt());
        item.setServiceId(model->serviceId());

        emit scheduleAudioRecording(item);
    }
}
