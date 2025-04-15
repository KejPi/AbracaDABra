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

#include "ensembleinfodialog.h"

#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QRegularExpression>
#include <QScrollBar>

#include "settings.h"
#include "ui_ensembleinfodialog.h"

EnsembleInfoDialog::EnsembleInfoDialog(QWidget *parent) : QDialog(parent), ui(new Ui::EnsembleInfoDialog)
{
    ui->setupUi(this);

#ifndef Q_OS_MAC
    // Set window flags to add minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
#endif

    connect(ui->recordButton, &QPushButton::clicked, this, &EnsembleInfoDialog::onRecordingButtonClicked);
    connect(ui->csvExportButton, &QPushButton::clicked, this, &EnsembleInfoDialog::requestEnsembleCSV);
    connect(ui->uploadButton, &QPushButton::clicked, this,
            [this]()
            {
                ui->uploadButton->setEnabled(false);
                emit requestUploadCVS();
            });

    clearFreqInfo();
    clearSignalInfo();
    clearServiceInfo();
    clearSubchInfo();
    resetFibStat();
    resetMscStat();

    // 16x '_' (max label size is 16 characters)
    int minWidth = 1.7 * ui->service->fontMetrics().boundingRect("________________").width();
    ui->FIBframe->setMinimumWidth(minWidth);
    ui->serviceFrame->setMinimumWidth(minWidth);
    ui->signalFrame->setMinimumWidth(minWidth);

    ui->FIBframe->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->FIBframe, &QWidget::customContextMenuRequested, this, &EnsembleInfoDialog::fibFrameContextMenu);

    // set tooltips
    ui->freqLabel->setToolTip(tr("Tuned frequency"));
    ui->freq->setToolTip(tr("Tuned frequency"));
    ui->channelLabel->setToolTip(tr("Tuned DAB channel"));
    ui->channel->setToolTip(tr("Tuned DAB channel"));
    ui->freqOffsetLabel->setToolTip(tr("Estimated frequency offset"));
    ui->freqOffset->setToolTip(tr("Estimated frequency offset"));
    ui->snrLabel->setToolTip(tr("Estimated SNR"));
    ui->snr->setToolTip(tr("Estimated SNR"));
    ui->agcGainLabel->setToolTip(tr("Current AGC gain<br>(only in software mode)"));
    ui->agcGain->setToolTip(tr("Current AGC gain<br>(only in software mode)"));
    ui->rfLevelLabel->setToolTip(tr("Estimated RF level<br>(only on supported devices)"));
    ui->rfLevel->setToolTip(tr("Estimated RF level<br>(only on supported devices)"));

    ui->serviceLabel->setToolTip(tr("Current service name"));
    ui->service->setToolTip(tr("Current service name"));
    ui->serviceIdLabel->setToolTip(tr("Current Service Identifier"));
    ui->serviceId->setToolTip(tr("Current Service Identifier"));
    ui->scidsLabel->setToolTip(tr("Service Component Identifier within the Service"));
    ui->scids->setToolTip(tr("Service Component Identifier within the Service"));
    ui->numCULabel->setToolTip(tr("Number of capacity units used by sub-channel"));
    ui->numCU->setToolTip(tr("Number of capacity units used by sub-channel"));
    ui->startCULabel->setToolTip(tr("First capacity unit used by sub-channel"));
    ui->startCU->setToolTip(tr("First capacity unit used by sub-channel"));
    ui->subChannelLabel->setToolTip(tr("Sub-channel Identifier"));
    ui->subChannel->setToolTip(tr("Sub-channel Identifier"));

    ui->fibCountLabel->setToolTip(tr("Total number of FIB's"));
    ui->fibCount->setToolTip(tr("Total number of FIB's"));
    ui->fibErrCountLabel->setToolTip(tr("Number of FIB's with CRC error"));
    ui->fibErrCount->setToolTip(tr("Number of FIB's with CRC error"));
    ui->fibErrRateLabel->setToolTip(tr("FIB error rate"));
    ui->fibErrRate->setToolTip(tr("FIB error rate"));
    ui->crcCountLabel->setToolTip(tr("Total number of audio frames (AU for DAB+)"));
    ui->crcCount->setToolTip(tr("Total number of audio frames (AU for DAB+)"));
    ui->crcErrCountLabel->setToolTip(tr("Total number of audio frames with CRC error (AU for DAB+)"));
    ui->crcErrCount->setToolTip(tr("Total number of audio frames with CRC error (AU for DAB+)"));
    ui->crcErrRateLabel->setToolTip(tr("Audio frame (AU for DAB+) error rate"));
    ui->crcErrRate->setToolTip(tr("Audio frame (AU for DAB+) error rate"));

    ui->recordButton->setToolTip(tr("Record raw IQ stream to file"));
    ui->csvExportButton->setToolTip(tr("Export ensemble information to CSV file"));
    ui->csvExportButton->setEnabled(false);
    ui->uploadButton->setToolTip(tr("Upload ensemble information to FMLIST"));
    ui->uploadButton->setEnabled(false);
    m_ensembleInfoUploaded = false;

    ui->timeoutCheckBox->setToolTip(tr("When checked recording stops automatically when timeout is reached."));
    ui->timeoutSpinBox->setToolTip(tr("Raw IQ recording duration"));

    enableRecording(false);
    ui->recordButton->setDefault(true);
    connect(ui->timeoutCheckBox, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                ui->timeoutSpinBox->setEnabled(checked);
                m_settings->ensembleInfo.recordingTimeoutEna = checked;
            });
    connect(ui->timeoutSpinBox, &QSpinBox::valueChanged, this, [this](int value) { m_settings->ensembleInfo.recordingTimeoutSec = value; });

    m_subChServicesLabel[0] = ui->subChService1;
    m_subChServicesLabel[1] = ui->subChService2;
    connect(ui->subchStruct, &EnsembleBar::subchannelClicked, this, &EnsembleInfoDialog::onSubchannelClicked);
}

EnsembleInfoDialog::~EnsembleInfoDialog()
{
    delete ui;
}

void EnsembleInfoDialog::loadSettings(Settings *settings)
{
    m_settings = settings;
    ui->timeoutSpinBox->setValue(m_settings->ensembleInfo.recordingTimeoutSec);
    ui->timeoutCheckBox->setChecked(m_settings->ensembleInfo.recordingTimeoutEna);
    if (!m_settings->ensembleInfo.geometry.isEmpty())
    {
        restoreGeometry(m_settings->ensembleInfo.geometry);
    }
}

void EnsembleInfoDialog::refreshEnsembleConfiguration(const QString &txt)
{
    if (isVisible())
    {
        ui->ensStructureTextEdit->setHtml(txt);
        if (txt.isEmpty())
        {  // empty ensemble configuration means tuning to new frequency
            clearSignalInfo();
            clearServiceInfo();

            resetFibStat();
            resetMscStat();

            ui->ensStructureTextEdit->setDocumentTitle("");
            ui->csvExportButton->setEnabled(false);
            ui->uploadButton->setEnabled(false);
        }
        else
        {
            ui->ensStructureTextEdit->setDocumentTitle(tr("Ensemble information"));

            int minWidth = ui->ensStructureTextEdit->document()->idealWidth() + ui->ensStructureTextEdit->contentsMargins().left() +
                           ui->ensStructureTextEdit->contentsMargins().right() + ui->ensStructureTextEdit->verticalScrollBar()->width();

            if (minWidth > 1000)
            {
                minWidth = 1000;
            }
            ui->ensStructureTextEdit->setMinimumWidth(minWidth);
            ui->csvExportButton->setEnabled(true);
        }
    }
}

void EnsembleInfoDialog::onEnsembleCSV(const QString &csvString)
{
    static const QRegularExpression regexp("[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    QString ensemblename = m_ensembleName;
    ensemblename.replace(regexp, "_");

    QString f = QString("%1/%2_%3_%4.csv")
                    .arg(m_settings->ensembleInfo.exportPath, QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"),
                         DabTables::channelList.value(m_frequency), ensemblename);

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export CSV file"), QDir::toNativeSeparators(f), tr("CSV Files") + " (*.csv)");

    if (!fileName.isEmpty())
    {
        m_settings->ensembleInfo.exportPath = QFileInfo(fileName).path();  // store path for next time
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream out(&file);
            out << csvString;
            file.close();
        }
    }
}

void EnsembleInfoDialog::onServiceComponentsList(const QList<RadioControlServiceComponent> &scList)
{
    clearSubchInfo();
    m_subchannelMap.clear();

    int allocatedCU = 0;
    int audioCU = 0;
    QHash<int, EnsembleBar::Subchannel> subchMap;
    for (const auto &sc : scList)
    {
        if (m_subchannelMap.contains(sc.SubChId))
        {  // subchannel is reused for more services
            if (!sc.label.isEmpty())
            {
                m_subchannelMap[sc.SubChId].services.append(sc.label);
            }
        }
        else
        {
            EnsembleBar::Subchannel ensembleBarSubch;
            ensembleBarSubch.id = sc.SubChId;
            ensembleBarSubch.startCU = sc.SubChAddr;
            ensembleBarSubch.numCU = sc.SubChSize;
            ensembleBarSubch.isAudio = sc.isAudioService();
            subchMap[sc.SubChId] = ensembleBarSubch;

            EnsembleInfoDialog::Subchannel ensInfoSubchannel;
            ensInfoSubchannel.id = sc.SubChId;
            ensInfoSubchannel.startCU = sc.SubChAddr;
            ensInfoSubchannel.numCU = sc.SubChSize;
            ensInfoSubchannel.protection = sc.protection;

            if (sc.isDataPacketService())
            {  // packet data
                switch (sc.protection.level)
                {
                    case DabProtectionLevel::EEP_1A:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 12 * 8;
                        break;
                    case DabProtectionLevel::EEP_2A:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 8 * 8;
                        break;
                    case DabProtectionLevel::EEP_3A:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 6 * 8;
                        break;
                    case DabProtectionLevel::EEP_4A:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 4 * 8;
                        break;
                    case DabProtectionLevel::EEP_1B:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 27 * 32;
                        break;
                    case DabProtectionLevel::EEP_2B:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 21 * 32;
                        break;
                    case DabProtectionLevel::EEP_3B:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 18 * 32;
                        break;
                    case DabProtectionLevel::EEP_4B:
                        ensInfoSubchannel.bitrate = sc.SubChSize / 25 * 32;
                        break;
                    default:
                        ensInfoSubchannel.bitrate = 0;
                        break;
                }
                ensInfoSubchannel.content = Subchannel::Data;
            }
            else
            {  // stream data
                ensInfoSubchannel.bitrate = sc.streamAudioData.bitRate;
                if (sc.isAudioService())
                {
                    audioCU += sc.SubChSize;
                    if (sc.streamAudioData.scType == DabAudioDataSCty::DABPLUS_AUDIO)
                    {
                        ensInfoSubchannel.content = Subchannel::AAC;
                    }
                    else
                    {
                        ensInfoSubchannel.content = Subchannel::MP2;
                    }
                }
                else
                {
                    ensInfoSubchannel.content = Subchannel::Data;
                }
            }
            ensInfoSubchannel.services = {sc.label};

            m_subchannelMap[sc.SubChId] = ensInfoSubchannel;

            allocatedCU += sc.SubChSize;
        }
    }
    ui->subchStruct->setSubchannels(subchMap.values());
    ui->ensAllocatedCU->setText(QString(tr("%1 CU (%2%)")).arg(allocatedCU).arg(allocatedCU * 100.0 / 864, 1, 'f', 1));
    ui->ensFreeCU->setText(QString(tr("%1 CU (%2%)")).arg(864 - allocatedCU).arg((864 - allocatedCU) * 100.0 / 864, 1, 'f', 1));
    ui->ensAudioCU->setText(QString(tr("%1 CU (%2%)")).arg(audioCU).arg(audioCU * 100.0 / 864, 1, 'f', 1));
    ui->ensDataCU->setText(QString(tr("%1 CU (%2%)")).arg(allocatedCU - audioCU).arg((allocatedCU - audioCU) * 100.0 / 864, 1, 'f', 1));

    ui->subChID->setText(tr("No subchannel selected"));
}

void EnsembleInfoDialog::onSubchannelClicked(int subchId)
{
    if (m_subchannelMap.contains(subchId))
    {
        const auto subCh = m_subchannelMap.value(subchId);
        ui->subChID->setText(QString::number(subCh.id));
        ui->subChCU->setText(tr("%1 CU [%2..%3]").arg(subCh.numCU).arg(subCh.startCU).arg(subCh.startCU + subCh.numCU - 1));
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
        ui->subChErrorProtection->setText(protection);

        ui->subChBitrate->setText(QString(tr("%1 kbps")).arg(subCh.bitrate));
        switch (subCh.content)
        {
            case Subchannel::AAC:
                ui->subChContents->setText(tr("Audio AAC"));
                break;
            case Subchannel::MP2:
                ui->subChContents->setText(tr("Audio MP2"));
                break;
            case Subchannel::Data:
                ui->subChContents->setText(tr("Data"));
                break;
        }

        ui->subChService0->setText("");
        ui->subChService1->setText("");
        ui->subChService2->setText("");

        ui->subChService0->setText(subCh.services.first());
        for (int n = 1; n < subCh.services.count(); ++n)
        {
            auto label = m_subChServicesLabel[(n - 1) % 2];
            if (label->text().isEmpty())
            {
                label->setText(subCh.services.at(n));
            }
            else
            {
                label->setText(label->text() + ", " + subCh.services.at(n));
            }
        }
    }
    ui->subChFrame0->setEnabled(true);
    ui->subChFrame1->setEnabled(true);
}

void EnsembleInfoDialog::updateSnr(uint8_t, float snr)
{
    ui->snr->setText(QString("%1 dB").arg(snr, 0, 'f', 1));
}

void EnsembleInfoDialog::updateFreqOffset(float offset)
{
    ui->freqOffset->setText(QString("%1 Hz").arg(offset, 0, 'f', 1));
}

void EnsembleInfoDialog::enableRecording(bool ena)
{
    ui->recordButton->setVisible(ena);
    ui->timeoutCheckBox->setVisible(ena);
    ui->timeoutSpinBox->setVisible(ena);
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");
    showRecordingStat(false);
    if (!ena)
    {
        emit recordingStop();
    }
}

void EnsembleInfoDialog::onRecordingButtonClicked()
{
    ui->recordButton->setEnabled(false);
    if (!m_isRecordingActive)
    {
        ui->recordButton->setEnabled(false);
        emit recordingStart(this, ui->timeoutCheckBox->isChecked() ? ui->timeoutSpinBox->value() : 0);
    }
    else
    {
        emit recordingStop();
    }
}

void EnsembleInfoDialog::onRecording(bool isActive)
{
    m_isRecordingActive = isActive;
    if (isActive)
    {
        ui->recordButton->setText(tr("Stop recording"));
        ui->recordButton->setToolTip(tr("Stop recording of raw IQ stream"));
    }
    else
    {
        ui->recordButton->setText(tr("Record raw data"));
        ui->recordButton->setToolTip(tr("Record raw IQ stream to file"));
    }
    ui->dumpSize->setText("");
    ui->dumpLength->setText("");

    showRecordingStat(m_isRecordingActive);
    ui->recordButton->setEnabled(true);
}

void EnsembleInfoDialog::updateRecordingStatus(uint64_t bytes, float ms)
{
    ui->dumpSize->setText(QString::number(double(bytes / (1024 * 1024.0)), 'f', 1) + " MB");
    ui->dumpLength->setText(QString::number(double(ms * 0.001), 'f', 1) + tr(" sec"));
}

void EnsembleInfoDialog::updateAgcGain(float gain)
{
    if (std::isnan(gain) || m_frequency == 0)
    {  // gain is not available (input device in HW mode)
        ui->agcGain->setText(tr("N/A"));
        return;
    }
    ui->agcGain->setText(QString::number(double(gain), 'f', 1) + " dB");
}

void EnsembleInfoDialog::updateRfLevel(float rfLevel, float)
{
    if (std::isnan(rfLevel) || m_frequency == 0)
    {  // level is not available (input device in HW mode or not RTL-SDR)
        ui->rfLevel->setText(tr("N/A"));
        return;
    }
    ui->rfLevel->setText(QString::number(double(rfLevel), 'f', 1) + " dBm");
}

void EnsembleInfoDialog::setAudioParameters(const AudioParameters &params)
{
    if (params.coding == AudioCoding::MP2)
    {
        m_serviceBitrateNet = m_serviceBitrate;
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
    ui->serviceCapacity->setText(QString(tr("%1 kbps").arg(m_serviceBitrateNet, 1, 'f', 1)));
}

void EnsembleInfoDialog::updatedDecodingStats(const RadioControlDecodingStats &stats)
{
    m_fibCounter += (stats.fibCntr - stats.fibErrorCntr);
    m_fibErrorCounter += stats.fibErrorCntr;
    ui->fibCount->setText(QString::number(m_fibCounter));
    ui->fibErrCount->setText(QString::number(m_fibErrorCounter));
    if (m_fibCounter > 0)
    {
        ui->fibErrRate->setText(QString::number(double(m_fibErrorCounter) / m_fibCounter, 'e', 4));
    }
    else
    {
        ui->fibErrRate->setText(tr("N/A"));
    }

    m_crcCounter += (stats.mscCrcOkCntr + stats.mscCrcErrorCntr);
    m_crcErrorCounter += stats.mscCrcErrorCntr;
    ui->crcCount->setText(QString::number(m_crcCounter));
    ui->crcErrCount->setText(QString::number(m_crcErrorCounter));
    if (m_crcCounter > 0)
    {
        ui->crcErrRate->setText(QString::number(double(m_crcErrorCounter) / m_crcCounter, 'e', 4));
    }
    else
    {
        ui->crcErrRate->setText(tr("N/A"));
    }

    if ((stats.audioServiceBytes > 0) && (m_serviceBitrateNet > 0))
    {
        float padRatio = qRound(stats.padBytes * 1000.0 / stats.audioServiceBytes) * 0.1;
        float padBitrate = qRound(m_serviceBitrateNet * padRatio * 0.1) * 0.1;
        ui->padBitrate->setText(QString(tr("%1 kbps")).arg(padBitrate, 1, 'f', 1));
        ui->padRatio->setText(QString("%1 %").arg(padRatio, 1, 'f', 1));
        float audioRatio = 100.0 - padRatio;
        float audioBitrate = qRound(m_serviceBitrateNet * audioRatio * 0.1) * 0.1;
        ui->audioBitrate->setText(QString(tr("%1 kbps")).arg(audioBitrate, 1, 'f', 1));
        ui->audioRatio->setText(QString("%1 %").arg(audioRatio, 1, 'f', 1));
    }
}

void EnsembleInfoDialog::resetFibStat()
{
    m_fibCounter = 0;
    m_fibErrorCounter = 0;
    ui->fibCount->setText("0");
    ui->fibErrCount->setText("0");
    ui->fibErrRate->setText(tr("N/A"));
}

void EnsembleInfoDialog::resetMscStat()
{
    m_crcCounter = 0;
    m_crcErrorCounter = 0;
    ui->crcCount->setText("0");
    ui->crcErrCount->setText("0");
    ui->crcErrRate->setText(tr("N/A"));
}

void EnsembleInfoDialog::newFrequency(quint32 f)
{
    if (f != m_frequency)
    {
        m_frequency = f;

        clearFreqInfo();
        clearServiceInfo();
        clearSubchInfo();
        resetMscStat();
        resetFibStat();
        m_ensembleName.clear();
        m_ensembleInfoUploaded = false;
        if (m_frequency)
        {
            ui->freq->setText(QString::number(m_frequency) + " kHz");
            ui->channel->setText(DabTables::channelList.value(m_frequency));
        }
    }
}

void EnsembleInfoDialog::serviceChanged(const RadioControlServiceComponent &s)
{
    setServiceInformation(s);
    resetMscStat();
}

void EnsembleInfoDialog::setServiceInformation(const RadioControlServiceComponent &s)
{
    clearServiceInfo();
    if (!s.SId.isValid())
    {  // service component not valid -> can happen during reconfig
        return;
    }

    ui->service->setText(QString("%1").arg(s.label, 16));
    ui->serviceId->setText("0x" + QString("%1").arg(s.SId.countryServiceRef(), 4, 16, QChar('0')).toUpper());
    ui->scids->setText(QString::number(s.SCIdS));
    ui->subChannel->setText(QString::number(s.SubChId));
    ui->startCU->setText(QString::number(s.SubChAddr));
    ui->numCU->setText(QString::number(s.SubChSize));
    m_serviceBitrate = s.streamAudioData.bitRate;
    ui->serviceBitrate->setText(QString(tr("%1 kbps").arg(m_serviceBitrate)));
}

void EnsembleInfoDialog::showEvent(QShowEvent *event)
{
    emit requestEnsembleConfiguration();
    QDialog::showEvent(event);
}

void EnsembleInfoDialog::closeEvent(QCloseEvent *event)
{
    if (m_isRecordingActive)
    {
        emit recordingStop();
    }
    QDialog::closeEvent(event);
}

void EnsembleInfoDialog::setEnsembleInfoUploaded(bool newEnsembleInfoUploaded)
{
    m_ensembleInfoUploaded = newEnsembleInfoUploaded;
    ui->uploadButton->setEnabled(!m_ensembleInfoUploaded);
}

void EnsembleInfoDialog::enableEnsembleInfoUpload()
{
    ui->uploadButton->setEnabled(true);
}

void EnsembleInfoDialog::fibFrameContextMenu(const QPoint &pos)
{
    QPoint globalPos = ui->FIBframe->mapToGlobal(pos);
    QMenu menu(this);
    QAction *resetAllAction = menu.addAction(tr("Reset statistics"));
    menu.addSeparator();
    QAction *fibResetAction = menu.addAction(tr("Reset FIB statistics"));
    QAction *mscResetAction = menu.addAction(tr("Reset MSC statistics"));
    QAction *selectedItem = menu.exec(globalPos);
    if (nullptr == selectedItem)
    {  // nothing was chosen
        return;
    }

    if (selectedItem == mscResetAction)
    {  // msc reset
        resetMscStat();
    }
    else if (selectedItem == fibResetAction)
    {  // fib reset
        resetFibStat();
    }
    else
    {  // reset all
        resetFibStat();
        resetMscStat();
    }
}

void EnsembleInfoDialog::clearServiceInfo()
{
    ui->service->setText(QString("%1").arg("", 16));
    ui->serviceId->setText("");
    ui->scids->setText("");
    ui->subChannel->setText("");
    ui->startCU->setText("");
    ui->numCU->setText("");
    ui->serviceBitrate->setText("");
    ui->serviceCapacity->setText("");
    ui->audioBitrate->setText("");
    ui->padBitrate->setText("");
    ui->audioRatio->setText("");
    ui->padRatio->setText("");
    m_serviceBitrate = 0;
    m_serviceBitrateNet = 0;
}

void EnsembleInfoDialog::clearSignalInfo()
{
    ui->snr->setText(tr("N/A"));
    ui->freqOffset->setText(tr("N/A"));
    ui->rfLevel->setText(tr("N/A"));
    ui->agcGain->setText(tr("N/A"));
}

void EnsembleInfoDialog::clearFreqInfo()
{
    ui->freq->setText("");
    ui->channel->setText("");
}

void EnsembleInfoDialog::clearSubchInfo()
{
    ui->subchStruct->clearSubchannels();
    ui->ensAllocatedCU->setText("");
    ui->ensFreeCU->setText("");
    ui->ensAudioCU->setText("");
    ui->ensDataCU->setText("");
    ui->subChID->setText("");
    ui->subChCU->setText("");
    ui->subChErrorProtection->setText("");
    ui->subChBitrate->setText("");
    ui->subChContents->setText("");
    ui->subChService0->setText("");
    ui->subChService1->setText("");
    ui->subChService2->setText("");
    ui->subChFrame0->setEnabled(false);
    ui->subChFrame1->setEnabled(false);
}

void EnsembleInfoDialog::showRecordingStat(bool ena)
{
    ui->dumpLengthLabel->setVisible(ena);
    ui->dumpSizeLabel->setVisible(ena);
    ui->dumpVLine1->setVisible(ena);
    ui->dumpVLine2->setVisible(ena);
    ui->timeoutCheckBox->setEnabled(!ena);
    ui->timeoutSpinBox->setEnabled(!ena && ui->timeoutCheckBox->isChecked());
}
