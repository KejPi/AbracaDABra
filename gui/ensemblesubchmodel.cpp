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

#include "ensemblesubchmodel.h"

EnsembleSubchModel::EnsembleSubchModel(QObject *parent)
    : QAbstractListModel{parent},
      m_palette{
          QColor(0x42, 0x85, 0xF4),  // Blue
          QColor(0xEA, 0x43, 0x35),  // Red
          QColor(0xFB, 0xBC, 0x05),  // Yellow
          QColor(0x34, 0xA8, 0x53),  // Green
          QColor(0x9C, 0x27, 0xB0),  // Purple
          QColor(0xFF, 0x98, 0x00),  // Orange
          QColor(0x00, 0xBC, 0xD4),  // Cyan
          QColor(0x79, 0x55, 0x48)   // Brown
      }
{
    connect(this, &QAbstractListModel::rowsInserted, this, &EnsembleSubchModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &EnsembleSubchModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &EnsembleSubchModel::rowCountChanged);
}

QVariant EnsembleSubchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
    {
        return QVariant();
    }

    const auto &subCh = m_subchannels.at(index.row());

    switch (static_cast<Roles>(role))
    {
        case IdRole:
            return QString::number(subCh.id);
        case StartCuRole:
            return subCh.startCU;
        case NumCuRole:
            return subCh.numCU;
        case OccupiedCuRole:
            return QString(tr("%1 CU [%2..%3]").arg(subCh.numCU).arg(subCh.startCU).arg(subCh.startCU + subCh.numCU - 1));
        case ColorRole:
            return subCh.color;
        case IsSelectedRole:
            return subCh.isSelected;
        case IsAudioRole:
            return subCh.isAudio;
        case ProtectionRole:
        {
            QString protection;
            if (subCh.protection.isEEP())
            {  // EEP
                QString label;
                if (subCh.protection.level < DabProtectionLevel::EEP_1B)
                {  // EEP x-A
                    label = QString("EEP %1-%2").arg(int(subCh.protection.level) - int(DabProtectionLevel::EEP_1A) + 1).arg("A");
                }
                else
                {  // EEP x+B
                    label = QString("EEP %1-%2").arg(int(subCh.protection.level) - int(DabProtectionLevel::EEP_1B) + 1).arg("B");
                }
                protection = QString(tr("%1 (coderate: %2/%3)")).arg(label).arg(subCh.protection.codeRateUpper).arg(subCh.protection.codeRateLower);
            }
            else
            {  // UEP
                QString label;
                label = QString("UEP #%1").arg(subCh.protection.uepIndex);
                protection = QString(tr("%1 (level: %2)")).arg(label).arg(int(subCh.protection.level));
            }
            return protection;
        }
        case BitrateRole:
            return QString(tr("%1 kbps")).arg(subCh.bitrate);
        case ContentRole:
        {
            switch (subCh.content)
            {
                case Subchannel::AAC:
                    return QString(tr("Audio AAC"));
                case Subchannel::MP2:
                    return QString(tr("Audio MP2"));
                case Subchannel::Data:
                    return QString(tr("Data"));
            }
        }
        break;
        case ServicesRole:
            return subCh.services;
        case ServicesShortRole:
            if (subCh.services.size() > 1)
            {
                return QString(tr("%1 (+%2)")).arg(subCh.services.first()).arg(subCh.services.size() - 1);
            }
            return subCh.services.isEmpty() ? "" : subCh.services.first();
        case RelativeWidthRole:
            return subCh.numCU * (1.0 / TotalNumCU);
        case IsEmptyRole:
            return subCh.isEmpty;
    }
    return QVariant();
}

QHash<int, QByteArray> EnsembleSubchModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "subchId";
    roles[StartCuRole] = "startCU";
    roles[NumCuRole] = "numCU";
    roles[ColorRole] = "subchColor";
    roles[IsSelectedRole] = "isSelected";
    roles[IsAudioRole] = "isAudio";
    roles[IsEmptyRole] = "isEmpty";
    roles[ProtectionRole] = "protection";
    roles[BitrateRole] = "bitrate";
    roles[ContentRole] = "content";
    roles[ServicesRole] = "services";
    roles[OccupiedCuRole] = "occupiedCU";
    roles[RelativeWidthRole] = "relativeWidth";
    roles[ServicesShortRole] = "servicesShort";
    return roles;
}

void EnsembleSubchModel::init(const QList<RadioControlServiceComponent> &scList)
{
    beginResetModel();
    m_subchannels.clear();

    QHash<int, Subchannel> subchannelMap;

    for (const auto &sc : scList)
    {
        if (subchannelMap.contains(sc.SubChId))
        {  // subchannel is reused for more services
            if (!sc.label.isEmpty())
            {
                subchannelMap[sc.SubChId].services.append(sc.label);
            }
        }
        else
        {
            Subchannel subchannel;
            subchannel.id = sc.SubChId;
            subchannel.startCU = sc.SubChAddr;
            subchannel.numCU = sc.SubChSize;
            subchannel.isAudio = sc.isAudioService();
            subchannel.protection = sc.protection;
            subchannel.isEmpty = false;

            if (sc.isDataPacketService())
            {  // packet data
                switch (sc.protection.level)
                {
                    case DabProtectionLevel::EEP_1A:
                        subchannel.bitrate = sc.SubChSize / 12 * 8;
                        break;
                    case DabProtectionLevel::EEP_2A:
                        subchannel.bitrate = sc.SubChSize / 8 * 8;
                        break;
                    case DabProtectionLevel::EEP_3A:
                        subchannel.bitrate = sc.SubChSize / 6 * 8;
                        break;
                    case DabProtectionLevel::EEP_4A:
                        subchannel.bitrate = sc.SubChSize / 4 * 8;
                        break;
                    case DabProtectionLevel::EEP_1B:
                        subchannel.bitrate = sc.SubChSize / 27 * 32;
                        break;
                    case DabProtectionLevel::EEP_2B:
                        subchannel.bitrate = sc.SubChSize / 21 * 32;
                        break;
                    case DabProtectionLevel::EEP_3B:
                        subchannel.bitrate = sc.SubChSize / 18 * 32;
                        break;
                    case DabProtectionLevel::EEP_4B:
                        subchannel.bitrate = sc.SubChSize / 25 * 32;
                        break;
                    default:
                        subchannel.bitrate = 0;
                        break;
                }
                subchannel.content = Subchannel::Data;
            }
            else
            {  // stream data
                subchannel.bitrate = sc.streamAudioData.bitRate;
                if (sc.isAudioService())
                {
                    if (sc.streamAudioData.scType == DabAudioDataSCty::DABPLUS_AUDIO)
                    {
                        subchannel.content = Subchannel::AAC;
                    }
                    else
                    {
                        subchannel.content = Subchannel::MP2;
                    }
                }
                else
                {
                    subchannel.content = Subchannel::Data;
                }
            }
            if (!sc.label.isEmpty())
            {
                subchannel.services = {sc.label};
            }

            subchannelMap[sc.SubChId] = subchannel;
        }
    }

    for (const auto &subch : subchannelMap.values())
    {
        if (subch.startCU < (subch.startCU + subch.numCU) && subch.startCU >= 0 && (subch.startCU + subch.numCU) <= TotalNumCU)
        {
            m_subchannels.append(subch);
        }
    }

    // Sort by start
    std::sort(m_subchannels.begin(), m_subchannels.end(), [](const Subchannel &a, const Subchannel &b) { return a.startCU < b.startCU; });

    // Assign colors to any segments that is not gap
    assignAutoColors();

    // Fill gaps between segments
    fillGaps();

    endResetModel();

    setCurrentRow(-1);
}

void EnsembleSubchModel::clear()
{
    beginResetModel();
    m_subchannels.clear();
    endResetModel();

    setCurrentRow(-1);
}

void EnsembleSubchModel::assignAutoColors()
{
    int colorIdx = 0;
    for (auto &subch : m_subchannels)
    {
        if (!subch.isEmpty)  // && subch.color == Qt::transparent)
        {
            subch.color = m_palette[colorIdx % m_palette.size()];
            colorIdx++;
        }
    }
}

void EnsembleSubchModel::fillGaps()
{
    QVector<Subchannel> subchannelsWithGaps;

    // Add first gap if needed (from 0 to first segment start)
    if (m_subchannels.isEmpty())
    {
        // No segments, add a gap for the entire range
        Subchannel gap;  //(0, TotalNumCU, Qt::transparent, "Empty: 0-" + QString::number(TotalNumCU));
        gap.startCU = 0;
        gap.numCU = TotalNumCU;
        gap.color = Qt::transparent;
        gap.isEmpty = true;
        subchannelsWithGaps.append(gap);
    }
    else
    {
        if (m_subchannels.first().startCU > 0)
        {
            Subchannel gap;
            gap.startCU = 0;
            gap.numCU = m_subchannels.first().startCU;
            gap.color = Qt::transparent;
            gap.isEmpty = true;
            subchannelsWithGaps.append(gap);
        }

        // Add all segments and gaps between them
        for (int i = 0; i < m_subchannels.size(); i++)
        {
            // Add the current segment
            subchannelsWithGaps.append(m_subchannels[i]);

            // Add gap to next segment if there is one
            if (i < m_subchannels.size() - 1 && (m_subchannels[i].startCU + m_subchannels[i].numCU) < m_subchannels[i + 1].startCU)
            {
                int gapSize = m_subchannels[i + 1].startCU - (m_subchannels[i].startCU + m_subchannels[i].numCU);
                Subchannel gap;
                gap.startCU = m_subchannels[i].startCU + m_subchannels[i].numCU;
                gap.numCU = gapSize;
                gap.color = Qt::transparent;
                gap.isEmpty = true;
                subchannelsWithGaps.append(gap);
            }
        }

        // Add final gap if needed (from last segment end to total parts)
        if (m_subchannels.last().startCU + m_subchannels.last().numCU < TotalNumCU)
        {
            int gapSize = TotalNumCU - (m_subchannels.last().startCU + m_subchannels.last().numCU);
            Subchannel gap;
            gap.startCU = m_subchannels.last().startCU + m_subchannels.last().numCU;
            gap.numCU = gapSize;
            gap.color = Qt::transparent;
            gap.isEmpty = true;
            subchannelsWithGaps.append(gap);
        }
    }

    m_subchannels = subchannelsWithGaps;
}

void EnsembleSubchModel::setCurrentRow(int currentRow)
{
    if (m_currentRow == currentRow || currentRow > m_subchannels.size())
    {
        return;
    }
    m_currentRow = currentRow;

    for (int n = 0; n < m_subchannels.size(); ++n)
    {
        if (m_subchannels.at(n).isSelected)
        {
            m_subchannels[n].isSelected = false;
            emit dataChanged(index(n, 0), index(n, 0), {Roles::IsSelectedRole});
        }
    }
    if (m_currentRow >= 0)
    {
        m_subchannels[m_currentRow].isSelected = true;
        emit dataChanged(index(m_currentRow, 0), index(m_currentRow, 0), {Roles::IsSelectedRole});
    }
    emit currentRowChanged();
}

void EnsembleSubchModel::setCurrentSubCh(int subchId)
{
    for (int n = 0; n < m_subchannels.size(); ++n)
    {
        if (m_subchannels.at(n).id == subchId)
        {
            setCurrentRow(n);
            return;
        }
    }
    // not found
    setCurrentRow(-1);
}

EnsembleNonEmptySubchModel::EnsembleNonEmptySubchModel(QObject *parent) : QSortFilterProxyModel(parent)
{}

void EnsembleNonEmptySubchModel::selectSubCh(int row)
{
    auto sourceRow = mapToSource(index(row, 0)).row();
    auto model = dynamic_cast<EnsembleSubchModel *>(sourceModel());
    if (model)
    {
        model->setCurrentRow(sourceRow);
    }
}

bool EnsembleNonEmptySubchModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
    auto model = dynamic_cast<EnsembleSubchModel *>(sourceModel());
    if (model)
    {
        QModelIndex idx = sourceModel()->index(source_row, 0);
        return (model->data(idx, EnsembleSubchModel::Roles::IsEmptyRole).toBool() == false);
    }
    return false;
}
