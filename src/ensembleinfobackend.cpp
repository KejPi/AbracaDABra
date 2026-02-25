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

#include "ensembleinfobackend.h"

#include <QtQml/qqml.h>

#include <QLoggingCategory>
#include <QUrl>

#include "androidfilehelper.h"
#include "dabtables.h"
#include "settings.h"

Q_DECLARE_LOGGING_CATEGORY(application)

EnsembleInfoBackend::EnsembleInfoBackend(QObject *parent) : QAbstractListModel{parent}
{
    // create list
    m_modelData.resize(LabelId::NumLabels);
    // clang-format off
    int group = 0;
    m_modelData[LabelId::Frequency] = new EnsembleInfoModelItem(group, tr("Frequency"), tr("Tuned frequency"));
    m_modelData[LabelId::Channel] = new EnsembleInfoModelItem(group, tr("Channel"), tr("Tuned DAB channel"));
    m_modelData[LabelId::Snr] = new EnsembleInfoModelItem(group, tr("SNR"), tr("Estimated SNR"));
    m_modelData[LabelId::FreqOffset] = new EnsembleInfoModelItem(group, tr("Frequency offset"), tr("Estimated frequency offset"));
    m_modelData[LabelId::AgcGain] = new EnsembleInfoModelItem(group, tr("AGC gain"), tr("Current AGC gain<br>(only in software mode)"));
    m_modelData[LabelId::RfLevel] = new EnsembleInfoModelItem(group, tr("RF level"), tr("Estimated RF level<br>(only on supported devices)"));

    group += 1;  // group 1
    m_modelData[LabelId::Service] = new EnsembleInfoModelItem(group, tr("Service"), tr("Current service name"));
    m_modelData[LabelId::SId] = new EnsembleInfoModelItem(group, tr("Service ID"), tr("Current Service Identifier"));
    m_modelData[LabelId::SCSId] = new EnsembleInfoModelItem(group, tr("SCIdS"), tr("Service Component Identifier within the Service"));
    m_modelData[LabelId::Subchannel] = new EnsembleInfoModelItem(group, tr("SubChannel"), tr("Service Component Identifier within the Service"));
    m_modelData[LabelId::StartCU] = new EnsembleInfoModelItem(group, tr("Start CU"), tr("First capacity unit used by sub-channel"));
    m_modelData[LabelId::NumCU] = new EnsembleInfoModelItem(group, tr("Number of CU"), tr("Number of capacity units used by sub-channel"));

    group += 1;  // group 2
    m_modelData[LabelId::ServiceBitrate] = new EnsembleInfoModelItem(group, tr("Service bitrate"), tr("Gross bitrate"));
    m_modelData[LabelId::UsefulBitrate] = new EnsembleInfoModelItem(group, tr("Useful bitrate"), tr("Net bitrate"));
    m_modelData[LabelId::AudioBitrate] = new EnsembleInfoModelItem(group, tr("Audio bitrate"), tr("Net audio bitrate without PAD"));
    m_modelData[LabelId::PadBitrate] = new EnsembleInfoModelItem(group, tr("PAD bitrate"), tr("Net PAD bitrate"));
    m_modelData[LabelId::AudioRatio] = new EnsembleInfoModelItem(group, tr("Audio ratio"), tr("Percentage of audio in useful bitrate"));
    m_modelData[LabelId::PadRatio] = new EnsembleInfoModelItem(group, tr("PAD ratio"), tr("Percentage of PAD in useful bitrate"));

    group += 1;  // group 3
    m_modelData[LabelId::FibCrcErr] = new EnsembleInfoModelItem(group, tr("FIB CRC errors"), tr("Total number of FIB's with CRC error"));
    m_modelData[LabelId::FibErrRate] = new EnsembleInfoModelItem(group, tr("FIB error rate"), tr("FIB error rate"));
    m_modelData[LabelId::RsUncorr] = new EnsembleInfoModelItem(group, tr("RS uncorrectable"), tr("Total number of uncorrectable Reed-Solomon code words (DAB+ only)"));
    m_modelData[LabelId::RsBer] = new EnsembleInfoModelItem(group, tr("RS BER"), tr("BER before Reed-Solomon decoder (DAB+ only)"));
    m_modelData[LabelId::AudioCrcErr] = new EnsembleInfoModelItem(group, tr("Audio CRC errors"), tr("Total number of audio frames with CRC error (AU for DAB+)"));
    m_modelData[LabelId::AudioCrcErrRate] = new EnsembleInfoModelItem(group, tr("Audio error rate"), tr("Audio frame (AU for DAB+) error rate"));

    group += 1;  // group 4
    m_modelData[LabelId::AllocatedCU] = new EnsembleInfoModelItem(group, tr("Allocated"), tr("Used capacity units"));
    m_modelData[LabelId::FreeCU] = new EnsembleInfoModelItem(group, tr("Free"), tr("Unused capacity units"));
    m_modelData[LabelId::AudioCU] = new EnsembleInfoModelItem(group, tr("Audio"), tr("Capacity units for allocated for audio services"));
    m_modelData[LabelId::DataCU] = new EnsembleInfoModelItem(group, tr("Data"), tr("Capacity units for allocated for data services"));
    group += 1;  // group 5
    m_modelData[LabelId::SelectedSubchannel] = new EnsembleInfoModelItem(group, tr("SubChannel"), tr("Sub-channel Identifier"));
    m_modelData[LabelId::SelectedCU] = new EnsembleInfoModelItem(group, tr("Capacity units"), tr("Number of capacity units allocated to sub-channel"));
    m_modelData[LabelId::SelectedProtection] = new EnsembleInfoModelItem(group, tr("Error protection"), tr("Sub-channel error protection"));
    m_modelData[LabelId::SelectedBitrate] = new EnsembleInfoModelItem(group, tr("Bitrate"), tr("Gross sub-channel bitrate"));
    group += 1;  // group 6
    m_modelData[LabelId::SelectedContent] = new EnsembleInfoModelItem(group, tr("Content"), tr("Sub-channel content"));
    m_modelData[LabelId::SelectedServices0] = new EnsembleInfoModelItem(group, tr("Services"), tr("List of services transmitted in sub-channel"));
    m_modelData[LabelId::SelectedServices1] = new EnsembleInfoModelItem(group, tr(""), tr("List of services transmitted in sub-channel"));
    m_modelData[LabelId::SelectedServices2] = new EnsembleInfoModelItem(group, tr(""), tr("List of services transmitted in sub-channel"));
    // clang-format on

    m_fibStats = new uint16_t[StatsHistorySize * 2];
    m_mscStats = new uint16_t[StatsHistorySize * 2];

    m_ensembleSubchModel = new EnsembleSubchModel(this);
    connect(m_ensembleSubchModel, &EnsembleSubchModel::currentRowChanged, this, &EnsembleInfoBackend::onSubchannelSelected);

    m_ensembleInfoUploaded = false;

    clearFreqInfo();
    clearSignalInfo();
    clearServiceInfo();
    clearSubchInfo();
    resetFibStat();
    resetMscStat();

    enableRecording(false);
}

EnsembleInfoBackend::~EnsembleInfoBackend()
{
    delete[] m_mscStats;
    delete[] m_fibStats;
    delete m_ensembleSubchModel;
    qDeleteAll(m_modelData);
}

void EnsembleInfoBackend::loadSettings(Settings *settings)
{
    m_settings = settings;
    recordingTimeout(m_settings->ensembleInfo.recordingTimeoutSec);
    isTimeoutEnabled(m_settings->ensembleInfo.recordingTimeoutEna);
    // if (!m_settings->ensembleInfo.geometry.isEmpty())
    // {
    //     restoreGeometry(m_settings->ensembleInfo.geometry);
    // }
}

void EnsembleInfoBackend::saveSettings()
{
    if (isRecordingOngoing())
    {
        emit recordingStop();
    }

    m_settings->ensembleInfo.recordingTimeoutSec = recordingTimeout();
    m_settings->ensembleInfo.recordingTimeoutEna = isTimeoutEnabled();
}

void EnsembleInfoBackend::refreshEnsembleConfiguration(const QString &txt)
{
    // ui->ensStructureTextEdit->setHtml(txt);
    setEnsembleConfigurationText(txt);
    if (txt.isEmpty())
    {  // empty ensemble configuration means tuning to new frequency
        clearServiceInfo();

        resetFibStat();
        resetMscStat();

        isCsvExportEnabled(false);
        isCsvUploadEnabled(false);

        // ui->ensStructureTextEdit->setDocumentTitle("");
    }
    else
    {
        // ui->ensStructureTextEdit->setDocumentTitle(tr("Ensemble information"));
        isCsvExportEnabled(true);
    }
}

void EnsembleInfoBackend::onServiceComponentsList(const QList<RadioControlServiceComponent> &scList)
{
    m_ensembleSubchModel->init(scList);

    int allocatedCU = 0;
    int audioCU = 0;
    QSet<int> subchMap;
    for (const auto &sc : scList)
    {
        if (!subchMap.contains(sc.SubChId))
        {
            subchMap.insert(sc.SubChId);
            if (sc.isAudioService())
            {
                audioCU += sc.SubChSize;
            }
            allocatedCU += sc.SubChSize;
        }
    }

    m_modelData.at(LabelId::AllocatedCU)->setInfo(QString(tr("%1 CU (%2%)")).arg(allocatedCU).arg(allocatedCU * 100.0 / 864, 1, 'f', 1));
    m_modelData.at(LabelId::FreeCU)->setInfo(QString(tr("%1 CU (%2%)")).arg(864 - allocatedCU).arg((864 - allocatedCU) * 100.0 / 864, 1, 'f', 1));
    m_modelData.at(LabelId::AudioCU)->setInfo(QString(tr("%1 CU (%2%)")).arg(audioCU).arg(audioCU * 100.0 / 864, 1, 'f', 1));
    m_modelData.at(LabelId::DataCU)
        ->setInfo(QString(tr("%1 CU (%2%)")).arg(allocatedCU - audioCU).arg((allocatedCU - audioCU) * 100.0 / 864, 1, 'f', 1));
    emit dataChanged(index(LabelId::AllocatedCU, 0), index(LabelId::DataCU, 0), {Roles::InfoRole});
}

QVariant EnsembleInfoBackend::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
    {
        return QVariant();
    }

    const auto &item = m_modelData.at(index.row());

    switch (static_cast<Roles>(role))
    {
        case LabelRole:
            return item->label();
        case InfoRole:
            return item->info();
        case IsVisibleRole:
            return item->visible();
        case GroupRole:
            return item->group();
        case LabelToolTipRole:
            return item->labelToolTip();
        default:
            break;
    }
    return QVariant();
}

QHash<int, QByteArray> EnsembleInfoBackend::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Roles::LabelRole] = "label";
    roles[Roles::LabelToolTipRole] = "tooltip";
    roles[Roles::InfoRole] = "info";
    roles[Roles::IsVisibleRole] = "isVisible";
    return roles;
}

void EnsembleInfoBackend::newFrequency(quint32 f)
{
    if (f != m_frequency)
    {
        m_frequency = f;

        clearFreqInfo();
        clearSignalInfo();
        clearServiceInfo();
        clearSubchInfo();
        resetMscStat();
        resetFibStat();
        m_ensembleName.clear();
        m_ensembleInfoUploaded = false;
        if (m_frequency)
        {
            m_modelData.at(LabelId::Frequency)->setInfo(QString::number(m_frequency) + " kHz");
            m_modelData.at(LabelId::Channel)->setInfo(DabTables::channelList.value(m_frequency));
            emit dataChanged(index(LabelId::Frequency, 0), index(LabelId::Channel, 0), {Roles::InfoRole});
        }
    }
}

void EnsembleInfoBackend::onSubchannelSelected()
{
    int idx = m_ensembleSubchModel->currentRow();
    if (idx < 0)
    {
        m_modelData.at(LabelId::SelectedSubchannel)->setInfo(tr("No subchannel selected"));
        for (int n = LabelId::SelectedCU; n <= LabelId::SelectedServices2; ++n)
        {
            m_modelData.at(n)->setInfo("");
        }

        emit dataChanged(index(LabelId::SelectedSubchannel, 0), index(LabelId::SelectedServices2, 0), {Roles::InfoRole});
        return;
    }

    QModelIndex sIdx = m_ensembleSubchModel->index(idx, 0);
    m_modelData.at(LabelId::SelectedSubchannel)->setInfo(m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::IdRole).toString());
    m_modelData.at(LabelId::SelectedCU)->setInfo(m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::OccupiedCuRole).toString());
    m_modelData.at(LabelId::SelectedProtection)->setInfo(m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::ProtectionRole).toString());
    m_modelData.at(LabelId::SelectedBitrate)->setInfo(m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::BitrateRole).toString());
    m_modelData.at(LabelId::SelectedContent)->setInfo(m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::ContentRole).toString());
    m_modelData.at(LabelId::SelectedServices0)->setInfo("");
    m_modelData.at(LabelId::SelectedServices1)->setInfo("");
    m_modelData.at(LabelId::SelectedServices2)->setInfo("");

    QStringList services = m_ensembleSubchModel->data(sIdx, EnsembleSubchModel::ServicesRole).toStringList();
    QString s[3];

    s[0] = services.first();
    for (int n = 1; n < services.count(); ++n)
    {
        auto &label = s[(n - 1) % 2 + 1];
        if (label.isEmpty())
        {
            label = services.at(n);
        }
        else
        {
            label.append(", " + services.at(n));
        }
    }
    m_modelData.at(LabelId::SelectedServices0)->setInfo(s[0]);
    m_modelData.at(LabelId::SelectedServices1)->setInfo(s[1]);
    m_modelData.at(LabelId::SelectedServices2)->setInfo(s[2]);

    emit dataChanged(index(LabelId::SelectedSubchannel, 0), index(LabelId::SelectedServices2, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::serviceChanged(const RadioControlServiceComponent &s)
{
    setServiceInformation(s);
    resetMscStat();
}

void EnsembleInfoBackend::setServiceInformation(const RadioControlServiceComponent &s)
{
    clearServiceInfo();
    if (!s.SId.isValid())
    {  // service component not valid -> can happen during reconfig
        return;
    }

    m_modelData.at(LabelId::Service)->setInfo(QString("%1").arg(s.label, 16));
    m_modelData.at(LabelId::SId)->setInfo(QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper());
    m_modelData.at(LabelId::SCSId)->setInfo(QString::number(s.SCIdS));
    m_modelData.at(LabelId::Subchannel)->setInfo(QString::number(s.SubChId));
    m_modelData.at(LabelId::StartCU)->setInfo(QString::number(s.SubChAddr));
    m_modelData.at(LabelId::NumCU)->setInfo(QString::number(s.SubChSize));
    m_serviceBitrate = s.streamAudioData.bitRate;
    m_modelData.at(LabelId::ServiceBitrate)->setInfo(QString(tr("%1 kbps").arg(m_serviceBitrate)));

    emit dataChanged(index(LabelId::Service, 0), index(LabelId::ServiceBitrate, 0), {Roles::InfoRole});
    m_ensembleSubchModel->setCurrentSubCh(s.SubChId);
}

void EnsembleInfoBackend::setAudioParameters(const AudioParameters &params)
{
    if (params.coding == AudioCoding::MP2)
    {
        m_serviceBitrateNet = m_serviceBitrate;
    }
    else if (params.coding == AudioCoding::None)
    {
        m_modelData.at(LabelId::UsefulBitrate)->setInfo("");
        m_modelData.at(LabelId::AudioBitrate)->setInfo("");
        m_modelData.at(LabelId::PadBitrate)->setInfo("");
        m_modelData.at(LabelId::AudioRatio)->setInfo("");
        m_modelData.at(LabelId::PadRatio)->setInfo("");
        emit dataChanged(index(LabelId::UsefulBitrate, 0), index(LabelId::PadRatio, 0), {Roles::InfoRole});

        m_serviceBitrateNet = 0;
        return;
    }
    else
    {
        int numAU = 3;
        if (params.sampleRateKHz == 32)
        {
            numAU = 2;
        }
        if (!params.sbr)
        {
            numAU = 2 * numAU;
        }
        m_serviceBitrateNet = qRound(((m_serviceBitrate / 8 * 110) - 2 * numAU - 2 - qRound(1.5 * (numAU - 1)) - 1) / 120.0 * 8 * 10) * 0.1;
    }
    m_modelData.at(LabelId::UsefulBitrate)->setInfo(QString(tr("%1 kbps").arg(m_serviceBitrateNet, 1, 'f', 1)));
    emit dataChanged(index(LabelId::UsefulBitrate, 0), index(LabelId::UsefulBitrate, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::updateSnr(uint8_t, float snr)
{
    m_modelData.at(LabelId::Snr)->setInfo(QString("%1 dB").arg(snr, 0, 'f', 1));
    emit dataChanged(index(LabelId::Snr, 0), index(LabelId::Snr, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::updateFreqOffset(float offset)
{
    m_modelData.at(LabelId::FreqOffset)->setInfo(QString("%1 Hz").arg(offset, 0, 'f', 1));
    emit dataChanged(index(LabelId::FreqOffset, 0), index(LabelId::FreqOffset, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::updateAgcGain(float gain)
{
    if (std::isnan(gain) || m_frequency == 0)
    {  // gain is not available (input device in HW mode)
        m_modelData.at(LabelId::AgcGain)->setInfo(tr("N/A"));
        emit dataChanged(index(LabelId::AgcGain, 0), index(LabelId::AgcGain, 0), {Roles::InfoRole});
        return;
    }
    m_modelData.at(LabelId::AgcGain)->setInfo(QString::number(double(gain), 'f', 1) + " dB");
    emit dataChanged(index(LabelId::AgcGain, 0), index(LabelId::AgcGain, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::updateRfLevel(float rfLevel, float)
{
    if (std::isnan(rfLevel) || m_frequency == 0)
    {  // level is not available (input device in HW mode or not RTL-SDR)
        m_modelData.at(LabelId::RfLevel)->setInfo(tr("N/A"));
        emit dataChanged(index(LabelId::RfLevel, 0), index(LabelId::RfLevel, 0), {Roles::InfoRole});
        return;
    }
    m_modelData.at(LabelId::RfLevel)->setInfo(QString::number(double(rfLevel), 'f', 1) + " dBm");
    emit dataChanged(index(LabelId::RfLevel, 0), index(LabelId::RfLevel, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::updatedDecodingStats(const RadioControlDecodingStats &stats)
{
    m_fibErrorCounter += stats.fibErrorCntr;
    m_fibStatsSum += stats.fibCntr - m_fibStats[m_fibStatsIdx];
    m_fibStats[m_fibStatsIdx++] = stats.fibCntr;
    m_fibStatsErrSum += stats.fibErrorCntr - m_fibStats[m_fibStatsIdx];
    m_fibStats[m_fibStatsIdx] = stats.fibErrorCntr;
    m_modelData.at(LabelId::FibCrcErr)->setInfo(QString::number(m_fibErrorCounter));
    m_fibStatsIdx = (m_fibStatsIdx + 1) & StatsIdxMask;
    if (m_fibStatsSum > 0)
    {
        m_modelData.at(LabelId::FibErrRate)->setInfo(QString("%1").arg(double(m_fibStatsErrSum) / m_fibStatsSum, 0, 'e', 2));
    }
    else
    {
        m_modelData.at(LabelId::FibErrRate)->setInfo("");
    }

    m_crcErrorCounter += stats.mscCrcErrorCntr;
    m_mscStatsSum += (stats.mscCrcOkCntr + stats.mscCrcErrorCntr) - m_mscStats[m_mscStatsIdx];
    m_mscStats[m_mscStatsIdx++] = (stats.mscCrcOkCntr + stats.mscCrcErrorCntr);
    m_mscStatsErrSum += stats.mscCrcErrorCntr - m_mscStats[m_mscStatsIdx];
    m_mscStats[m_mscStatsIdx] = stats.mscCrcErrorCntr;
    m_mscStatsIdx = (m_mscStatsIdx + 1) & StatsIdxMask;
    m_modelData.at(LabelId::AudioCrcErr)->setInfo(QString::number(m_crcErrorCounter));
    if (m_mscStatsSum > 0)
    {
        m_modelData.at(LabelId::AudioCrcErrRate)->setInfo(QString("%1").arg(double(m_mscStatsErrSum) / m_mscStatsSum, 0, 'e', 2));
    }
    else
    {
        m_modelData.at(LabelId::AudioCrcErrRate)->setInfo("");
        if (m_crcErrorCounter == 0)
        {
            m_modelData.at(LabelId::AudioCrcErr)->setInfo("");
        }
    }
    m_rsUncorrCounter += stats.rsUncorrectableCntr;
    m_modelData.at(LabelId::RsUncorr)->setInfo(QString::number(m_rsUncorrCounter));
    if (stats.rsBytes > 0)
    {
        m_modelData.at(LabelId::RsBer)->setInfo(QString("%1").arg(stats.rsBitErrorCntr * 0.125 / stats.rsBytes, 0, 'e', 2));
    }
    else
    {
        m_modelData.at(LabelId::RsBer)->setInfo("");
        if (stats.rsBitErrorCntr == 0)
        {
            m_modelData.at(LabelId::RsUncorr)->setInfo("");
        }
    }

    emit dataChanged(index(LabelId::FibCrcErr, 0), index(LabelId::AudioCrcErrRate, 0), {Roles::InfoRole});

    if ((stats.audioServiceBytes > 0) && (m_serviceBitrateNet > 0))
    {
        float padRatio = qRound(stats.padBytes * 1000.0 / stats.audioServiceBytes) * 0.1;
        float padBitrate = qRound(m_serviceBitrateNet * padRatio * 0.1) * 0.1;
        m_modelData.at(LabelId::PadBitrate)->setInfo(QString(tr("%1 kbps")).arg(padBitrate, 1, 'f', 1));
        m_modelData.at(LabelId::PadRatio)->setInfo(QString("%1 %").arg(padRatio, 1, 'f', 1));
        float audioRatio = 100.0 - padRatio;
        float audioBitrate = qRound(m_serviceBitrateNet * audioRatio * 0.1) * 0.1;
        m_modelData.at(LabelId::AudioBitrate)->setInfo(QString(tr("%1 kbps")).arg(audioBitrate, 1, 'f', 1));
        m_modelData.at(LabelId::AudioRatio)->setInfo(QString("%1 %").arg(audioRatio, 1, 'f', 1));
        emit dataChanged(index(LabelId::AudioBitrate, 0), index(LabelId::AudioCrcErrRate, 0), {Roles::InfoRole});
    }
}

void EnsembleInfoBackend::clearServiceInfo()
{
    for (int n = LabelId::Service; n <= LabelId::PadRatio; ++n)
    {
        m_modelData.at(n)->setInfo("");
    }
    m_serviceBitrate = 0;
    m_serviceBitrateNet = 0;
    emit dataChanged(index(LabelId::Service, 0), index(LabelId::PadRatio, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::clearSignalInfo()
{
    for (int n = LabelId::Snr; n <= LabelId::RfLevel; ++n)
    {
        m_modelData.at(n)->setInfo(tr("N/A"));
    }
    emit dataChanged(index(LabelId::Snr, 0), index(LabelId::RfLevel, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::clearFreqInfo()
{
    m_modelData.at(LabelId::Frequency)->setInfo("");
    m_modelData.at(LabelId::Channel)->setInfo("");
    emit dataChanged(index(LabelId::Frequency, 0), index(LabelId::Channel, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::clearSubchInfo()
{
    m_ensembleSubchModel->clear();
    for (int n = LabelId::AllocatedCU; n <= LabelId::DataCU; ++n)
    {
        m_modelData.at(n)->setInfo("");
    }
    emit dataChanged(index(LabelId::AllocatedCU, 0), index(LabelId::DataCU, 0), {Roles::InfoRole});

    m_ensembleSubchModel->setCurrentRow(-1);
    onSubchannelSelected();
}

void EnsembleInfoBackend::resetFibStat()
{
    m_fibStatsErrSum = 0;
    m_fibStatsSum = 0;
    m_fibErrorCounter = 0;
    m_modelData.at(LabelId::FibCrcErr)->setInfo("");
    m_modelData.at(LabelId::FibErrRate)->setInfo("");
    memset(m_fibStats, 0, sizeof(uint16_t) * StatsHistorySize * 2);
    emit dataChanged(index(LabelId::FibCrcErr, 0), index(LabelId::FibErrRate, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::resetMscStat()
{
    m_mscStatsSum = 0;
    m_mscStatsErrSum = 0;
    m_crcErrorCounter = 0;
    m_rsUncorrCounter = 0;

    for (int n = LabelId::RsUncorr; n <= LabelId::AudioCrcErrRate; ++n)
    {
        m_modelData.at(n)->setInfo("");
    }
    memset(m_mscStats, 0, sizeof(uint16_t) * StatsHistorySize * 2);
    emit dataChanged(index(LabelId::RsUncorr, 0), index(LabelId::AudioCrcErrRate, 0), {Roles::InfoRole});
}

void EnsembleInfoBackend::enableRecording(bool ena)
{
    isRecordingVisible(ena);
    isRecordingEnabled(ena);
    recordingLength("");
    recordingSize("");
    if (!ena)
    {
        emit recordingStop();
    }
}

void EnsembleInfoBackend::onRecording(bool isActive)
{
    isRecordingOngoing(isActive);
    recordingLength("");
    recordingSize("");
    isRecordingEnabled(true);
}

void EnsembleInfoBackend::updateRecordingStatus(uint64_t bytes, float ms)
{
    recordingSize(QString::number(double(bytes / (1024 * 1024.0)), 'f', 1) + " MB");
    recordingLength(QString::number(double(ms * 0.001), 'f', 1) + tr(" sec"));
}

void EnsembleInfoBackend::startStopRecording()
{
    isRecordingEnabled(false);
    if (isRecordingOngoing())
    {
        emit recordingStop();
    }
    else
    {
        emit recordingStart(isTimeoutEnabled() ? recordingTimeout() : 0);
    }
}

void EnsembleInfoBackend::saveCSV()
{
    emit requestEnsembleCSV();
}

void EnsembleInfoBackend::onEnsembleCSV(const QString &csvString)
{
    static const QRegularExpression regexp("[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    QString ensemblename = m_ensembleName;
    ensemblename.replace(regexp, "_");
    QString fileName = QString("%1_%2_%3.csv")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(m_frequency), ensemblename);

    const QString ensemblePath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, ENSEMBLE_DIR_NAME);

    // Ensure directory exists and is writable
    if (!AndroidFileHelper::mkpath(m_settings->dataStoragePath, ENSEMBLE_DIR_NAME))
    {
        qCWarning(application) << "Failed to create ensemble export directory:" << AndroidFileHelper::lastError();
        showInfoMessage(tr("Ensemble information export failed"), -1);
        return;
    }

    if (!AndroidFileHelper::hasWritePermission(ensemblePath))
    {
        qCWarning(application) << "No permission to write to:" << ensemblePath;
        qCWarning(application) << "Please select a new data storage folder in settings.";
        showInfoMessage(tr("No permission to write ensemble information"), -1);
        return;
    }

    if (AndroidFileHelper::writeTextFile(ensemblePath, fileName, csvString, "text/csv"))
    {
        qCInfo(application) << "Ensemble CSV exported to:" << QString("%1/%2").arg(ensemblePath, fileName);
        // showInfoMessage(tr("Ensemble information exported to\n%1/%2").arg(ensemblePath, fileName), 0);
        showInfoMessage(tr("Ensemble information exported"), 0);
    }
    else
    {
        qCWarning(application) << "Failed to export ensemble CSV:" << AndroidFileHelper::lastError();
        showInfoMessage(tr("Failed to export ensemble information"), -1);
    }
}

void EnsembleInfoBackend::enableEnsembleInfoUpload()
{
    isCsvUploadEnabled(true);
}

void EnsembleInfoBackend::setEnsembleInfoUploaded(bool ensembleInfoUploaded)
{
    m_ensembleInfoUploaded = ensembleInfoUploaded;
    isCsvUploadEnabled(!m_ensembleInfoUploaded);
}

void EnsembleInfoBackend::uploadCSV()
{
    isCsvUploadEnabled(false);
    emit requestUploadCVS();
}

int EnsembleInfoModel::group() const
{
    return m_group;
}

void EnsembleInfoModel::setGroup(int group)
{
    if (m_group == group)
    {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    m_group = group;
    emit groupChanged();

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

bool EnsembleInfoModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (m_group < 0)
    {
        return true;
    }

    QModelIndex idx = sourceModel()->index(source_row, 0);
    if (idx.isValid())
    {
        return sourceModel()->data(idx, EnsembleInfoBackend::Roles::GroupRole).toInt() == m_group;
    }
    return false;
}

QString EnsembleInfoBackend::ensembleConfigurationText() const
{
    return m_ensembleConfigurationText;
}

void EnsembleInfoBackend::setEnsembleConfigurationText(const QString &ensembleConfigurationText)
{
    if (m_ensembleConfigurationText == ensembleConfigurationText)
    {
        return;
    }
    m_ensembleConfigurationText = ensembleConfigurationText;
    emit ensembleConfigurationTextChanged();
}
