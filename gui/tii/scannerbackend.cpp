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

#include <QDebug>

#include "contextmenumodel.h"

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QTextStream>
#include <QUrl>

#include "androidfilehelper.h"
#include "dabtables.h"
#include "messageboxbackend.h"
#include "scannerbackend.h"
#include "settings.h"

Q_LOGGING_CATEGORY(scanner, "Scanner", QtDebugMsg)

ScannerBackend::ScannerBackend(Settings *settings, QObject *parent) : TxMapBackend(settings, false, parent)
{
    m_sortedFilteredModel->setColumnsFilter(false);
    m_channelSelectionModel = new ChannelSelectionModel(settings, this);
    m_messageBoxBackend = new MessageBoxBackend(this);
    m_contextMenuModel = new ContextMenuModel(this);
    connect(m_contextMenuModel, &ContextMenuModel::actionTriggered, this, &ScannerBackend::handleContextMenuAction);
    m_model->loadLocalTxList(m_settings->filePath + "/LocalTx.json");
    m_ensemble.ueid = RADIO_CONTROL_UEID_INVALID;

    loadSettings();

    connect(this, &ScannerBackend::mapCenterChanged, this, [this]() { m_settings->scanner.mapCenter = mapCenter(); });
    connect(this, &ScannerBackend::zoomLevelChanged, this, [this]() { m_settings->scanner.mapZoom = zoomLevel(); });
    connect(this, &ScannerBackend::centerToCurrentPositionChanged, this,
            [this]() { m_settings->scanner.centerMapToCurrentPosition = centerToCurrentPosition(); });
    connect(this, &ScannerBackend::hideLocalTxChanged, this, [this]() { m_sortedFilteredModel->setLocalTxFilter(m_settings->scanner.hideLocalTx); });

    onSettingsChanged();
}

ScannerBackend::~ScannerBackend()
{
    stopAutoSaveCsv();

    delete m_channelSelectionModel;
    delete m_messageBoxBackend;
    if (nullptr != m_timer)
    {
        m_timer->stop();
        delete m_timer;
    }
}

void ScannerBackend::startStopAction()
{
    if (m_isScanning)
    {  // stop pressed
        isStartStopEnabled(false);
        isScanning(false);
        m_ensemble.reset();

        // the state machine has 4 possible states
        // 1. wait for tune (event)
        // 2. wait for sync (timer or event)
        // 4. wait for ensemble (timer or event)
        // 5. wait for tii (timer)
        if (m_timer->isActive())
        {  // state 2, 3, 4
            m_timer->stop();
            stopScan();
        }
        else
        {
            // timer not running -> state 1
            m_state = ScannerState::Interrupted;  // ==> it will be finished when tune is complete
        }
    }
    else
    {  // start pressed
        m_numSelectedChannels = 0;
        m_isPreciseMode = (mode() == Mode::Mode_Precise);
        m_numSelectedChannels = m_channelSelectionModel->numChecked();
        if (numCycles() > 0)
        {
            progressMax(m_numSelectedChannels * numCycles());
        }
        else
        {
            progressMax(m_numSelectedChannels);
        }
        startScan();
    }
}

void ScannerBackend::stopScan()
{
    if (m_isTiiActive)
    {
        emit setTii(false);
        m_isTiiActive = false;
    }

    stopAutoSaveCsv();

    if (m_exitRequested)
    {
        //         close();
    }

    // restore UI
    isScanning(false);
    scanningLabel(tr("Scanning finished"));
    progressValue(0);
    progressChannel("");

    // adding timeout to avoid timing issues due to double click on start button
    isStartStopEnabled(false);
    QTimer::singleShot(2500, this, [this]() { isStartStopEnabled(true); });

    isScanning(false);
    m_state = ScannerState::Idle;

    emit scanFinished();
}

void ScannerBackend::importAction()
{
    if (m_model->rowCount() > 0)
    {
        m_messageBoxBackend->showQuestion(
            tr("Replace data in the table?"), tr("Current data in the table will be deleted. This action is irreversible."),
            [this](MessageBoxBackend::StandardButton result)
            {
                if (result == MessageBoxBackend::StandardButton::Ok)
                {
                    QTimer::singleShot(10, this, [this]() { emit openFileDialog(); });
                }
                else
                {  // do nothing
                }
            },
            {{MessageBoxBackend::StandardButton::Ok, tr("Replace"), MessageBoxBackend::ButtonRole::Negative},
             {MessageBoxBackend::StandardButton::Cancel, tr("Cancel"), MessageBoxBackend::ButtonRole::Neutral}},
            MessageBoxBackend::StandardButton::Ok);
    }
    else
    {
        emit openFileDialog();
    }
}

QUrl ScannerBackend::csvPath() const
{
    return QUrl::fromLocalFile(m_settings->dataStoragePath + '/' + SCANNER_DIR_NAME);
}

void ScannerBackend::loadCSV(const QUrl &fileUrl)
{
    if (fileUrl.isEmpty())
    {
        return;
    }

    QString fileName;
#ifdef Q_OS_ANDROID
    if (fileUrl.scheme() == "content" || fileUrl.isLocalFile())
    {
        fileName = fileUrl.toString();
    }
    else
    {
        return;
    }
#else
    if (!fileUrl.isLocalFile())
    {
        return;
    }
    fileName = fileUrl.toLocalFile();
#endif

    qCInfo(scanner) << "Loading file:" << fileName;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        bool timeIsUTC = in.readLine().contains("(UTC)");
        int lineNum = 2;
        bool res = true;
        while (!in.atEnd())
        {
            QString line = in.readLine();
            // qDebug() << line;

            QStringList qsl = line.split(';');
            if (qsl.size() == TxTableModel::NumColsWithoutCoordinates)
            {
#if (QT_VERSION < QT_VERSION_CHECK(6, 7, 0))
                QDateTime time = QDateTime::fromString(qsl.at(TxTableModel::ColTime), "yyyy-MM-dd hh:mm:ss").addYears(100);
                if (time.isValid() == false)
                {  // old format
                    time = QDateTime::fromString(qsl.at(TxTableModel::ColTime), "yy-MM-dd hh:mm:ss").addYears(100);
                }

#else
                QDateTime time = QDateTime::fromString(qsl.at(TxTableModel::ColTime), "yyyy-MM-dd hh:mm:ss", 2000);
                if (time.isValid() == false)
                {  // old format
                    time = QDateTime::fromString(qsl.at(TxTableModel::ColTime), "yy-MM-dd hh:mm:ss", 2000);
                }
#endif
                if (!time.isValid())
                {
                    qCWarning(scanner) << "Invalid time value" << qsl.at(TxTableModel::ColTime) << "line #" << lineNum;
                    res = false;
                    break;
                }

                if (timeIsUTC)
                {
                    time.setTimeSpec(Qt::TimeSpec::UTC);
                }
                else
                {
                    time.setTimeSpec(Qt::TimeSpec::LocalTime);
                }

                bool isOk = false;
                uint32_t freq = qsl.at(TxTableModel::ColFreq).toUInt(&isOk);
                if (!isOk)
                {
                    qCWarning(scanner) << "Invalid frequency value" << qsl.at(TxTableModel::ColFreq) << "line #" << lineNum;
                    res = false;
                    break;
                }

                uint32_t ueid = qsl.at(TxTableModel::ColEnsId).toUInt(&isOk, 16);
                if (!isOk)
                {
                    qCWarning(scanner) << "Invalid UEID value" << qsl.at(TxTableModel::ColEnsId) << "line #" << lineNum;
                    res = false;
                    break;
                }
                int numServices = qsl.at(TxTableModel::ColNumServices).toInt(&isOk);
                if (!isOk)
                {
                    qCWarning(scanner) << "Invalid number of services value" << qsl.at(TxTableModel::ColNumServices) << "line #" << lineNum;
                    res = false;
                    break;
                }

                float snr = qsl.at(TxTableModel::ColSnr).toFloat(&isOk);
                if (!isOk)
                {
                    qCWarning(scanner) << "Invalid SNR value" << qsl.at(TxTableModel::ColSnr) << "line #" << lineNum;
                    res = false;
                    break;
                }
                QList<dabsdrTii_t> tiiList;
                if (!qsl.at(TxTableModel::ColMainId).isEmpty())
                {
                    uint8_t main = qsl.at(TxTableModel::ColMainId).toUInt(&isOk);
                    if (!isOk)
                    {
                        qCWarning(scanner) << "Invalid TII code" << qsl.at(TxTableModel::ColMainId) << "line #" << lineNum;
                        res = false;
                        break;
                    }
                    uint8_t sub = qsl.at(TxTableModel::ColSubId).toUInt(&isOk);
                    if (!isOk)
                    {
                        qCWarning(scanner) << "Invalid TII code" << qsl.at(TxTableModel::ColSubId) << "line #" << lineNum;
                        res = false;
                        break;
                    }
                    float level = qsl.at(TxTableModel::ColLevel).toFloat(&isOk);
                    if (!isOk)
                    {
                        qCWarning(scanner) << "Invalid TX level value" << qsl.at(TxTableModel::ColLevel) << "line #" << lineNum;
                        res = false;
                        break;
                    }
                    dabsdrTii_t tiiItem({.main = main, .sub = sub, .level = level});
                    tiiList.append(tiiItem);
                }

                m_model->appendEnsData(time.toLocalTime(), tiiList, ServiceListId(freq, ueid), qsl.at(TxTableModel::ColEnsLabel), "", "", numServices,
                                       snr);
            }
            else
            {
                qCWarning(scanner) << "Unexpected number of cols, line #" << lineNum;
                res = false;
                break;
            }
            lineNum += 1;
        }
        file.close();

        m_model->endLoadingFromFile();

        if (res == false)
        {
            qCWarning(scanner) << "Failed to load file:" << fileName;
            reset();
        }
    }
}

void ScannerBackend::saveCSV()
{
    const QString fileName = QString("%1.csv").arg(m_scanStartTime.toString("yyyy-MM-dd_hhmmss"));
    saveToFile(fileName);
}

void ScannerBackend::saveToFile(const QString &fileName)
{
    // Build CSV content
    QString csvContent;
    QTextStream out(&csvContent);

    auto exportRole = m_settings->tii.timestampInUTC ? TxTableModel::TxTableModelRoles::ExportRoleUTC : TxTableModel::TxTableModelRoles::ExportRole;

    // Header
    for (int col = 0; col < TxTableModel::NumColsWithoutCoordinates - 1; ++col)
    {
        out << m_model->headerData(col, Qt::Horizontal, exportRole).toString() << ";";
    }
    out << m_model->headerData(TxTableModel::NumColsWithoutCoordinates - 1, Qt::Horizontal, exportRole).toString() << Qt::endl;

    // Body
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        if (m_settings->scanner.hideLocalTx && m_model->data(m_model->index(row, 0), TxTableModel::IsLocalRole).toBool())
        {  // do not export local TX if local filter is set
            continue;
        }

        for (int col = 0; col < TxTableModel::NumColsWithoutCoordinates - 1; ++col)
        {
            out << m_model->data(m_model->index(row, col), exportRole).toString() << ";";
        }
        out << m_model->data(m_model->index(row, TxTableModel::NumColsWithoutCoordinates - 1), exportRole).toString() << Qt::endl;
    }
    out.flush();

    // Ensure path exists and writable
    const QString basePath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, SCANNER_DIR_NAME);
    if (!AndroidFileHelper::mkpath(basePath))
    {
        qCWarning(scanner) << "Failed to create export directory:" << AndroidFileHelper::lastError();
        return;
    }

    QString targetBase = basePath;
    if (AndroidFileHelper::isContentUri(basePath))
    {
        // No additional subdir, just ensure trailing encoding handled
        if (!AndroidFileHelper::hasWritePermission(basePath))
        {
            qCWarning(scanner) << "No permission to write to:" << basePath;
            return;
        }
    }

    if (AndroidFileHelper::writeTextFile(targetBase, fileName, csvContent, "text/csv"))
    {
        qCInfo(scanner) << "Log CSV saved to:" << QString("%1/%2").arg(targetBase, fileName);
    }
    else
    {
        qCWarning(scanner) << "Failed to save log CSV:" << AndroidFileHelper::lastError();
    }
}

void ScannerBackend::startAutoSaveCsv()
{
    stopAutoSaveCsv();  // close any previously open file

    const QString fileName = QString("%1.csv").arg(m_scanStartTime.toString("yyyy-MM-dd_hhmmss"));

    // Ensure path exists and writable
    const QString basePath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, SCANNER_DIR_NAME);
    if (!AndroidFileHelper::mkpath(basePath))
    {
        qCWarning(scanner) << "Failed to create export directory:" << AndroidFileHelper::lastError();
        return;
    }

    if (AndroidFileHelper::isContentUri(basePath))
    {
        // No additional subdir, just ensure trailing encoding handled
        if (!AndroidFileHelper::hasWritePermission(basePath))
        {
            qCWarning(scanner) << "No permission to write to:" << basePath;
            return;
        }
    }

    m_autoSaveFile = AndroidFileHelper::openFileForWriting(basePath, fileName);
    if (m_autoSaveFile == nullptr)
    {
        qCWarning(scanner) << "Failed to open auto-save file:" << AndroidFileHelper::lastError();
        return;
    }

    m_autoSaveExportRole =
        m_settings->tii.timestampInUTC ? TxTableModel::TxTableModelRoles::ExportRoleUTC : TxTableModel::TxTableModelRoles::ExportRole;

    // Write header
    QTextStream out(m_autoSaveFile);
    for (int col = 0; col < TxTableModel::NumColsWithoutCoordinates - 1; ++col)
    {
        out << m_model->headerData(col, Qt::Horizontal, m_autoSaveExportRole).toString() << ";";
    }
    out << m_model->headerData(TxTableModel::NumColsWithoutCoordinates - 1, Qt::Horizontal, m_autoSaveExportRole).toString() << Qt::endl;
    out.flush();
    m_autoSaveFile->flush();

    // If model already has rows (clearOnStart == false), write them now
    if (m_model->rowCount() > 0)
    {
        appendAutoSaveRows(0, m_model->rowCount() - 1);
    }

    qCInfo(scanner) << "Auto-save CSV started:" << fileName;
}

void ScannerBackend::appendAutoSaveRows(int firstRow, int lastRow)
{
    if (m_autoSaveFile == nullptr || !m_autoSaveFile->isOpen())
    {
        return;
    }

    QTextStream out(m_autoSaveFile);
    for (int row = firstRow; row <= lastRow; ++row)
    {
        if (m_settings->scanner.hideLocalTx && m_model->data(m_model->index(row, 0), TxTableModel::IsLocalRole).toBool())
        {  // do not export local TX if local filter is set
            continue;
        }

        for (int col = 0; col < TxTableModel::NumColsWithoutCoordinates - 1; ++col)
        {
            out << m_model->data(m_model->index(row, col), m_autoSaveExportRole).toString() << ";";
        }
        out << m_model->data(m_model->index(row, TxTableModel::NumColsWithoutCoordinates - 1), m_autoSaveExportRole).toString() << Qt::endl;
    }
    out.flush();
    m_autoSaveFile->flush();
}

void ScannerBackend::stopAutoSaveCsv()
{
    if (m_autoSaveFile != nullptr)
    {
        if (m_autoSaveFile->isOpen())
        {
            m_autoSaveFile->close();
            qCInfo(scanner) << "Auto-save CSV closed:" << m_autoSaveFile->fileName();
        }
        delete m_autoSaveFile;
        m_autoSaveFile = nullptr;
    }
}

void ScannerBackend::startScan()
{
    isScanning(true);

    if (m_settings->scanner.clearOnStart)
    {
        reset();
    }
    m_scanStartTime = QDateTime::currentDateTime();
    scanningLabel(tr("Channel:"));

    //    m_signalStateLabel->reset();

    m_scanCycleCntr = 0;
    m_frequency = 0;

    if (m_timer == nullptr)
    {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &ScannerBackend::scanStep);
    }

    if (m_settings->scanner.autoSave)
    {
        startAutoSaveCsv();
    }

    m_state = ScannerState::Init;

    // using timer for mainwindow to cleanup and tune to 0 potentially (no timeout in case)
#ifdef Q_OS_WIN
    m_timer->start(6000);
#else
    m_timer->start(2000);
#endif
    qCInfo(scanner) << "Scanning starts";

    emit scanStarts();
}

void ScannerBackend::scanStep()
{
    if (ScannerState::Init == m_state)
    {  // first step
        m_channelIt = DabTables::channelList.constBegin();
    }
    else
    {  // next step
        ++m_channelIt;
    }

    // find active channel
    while ((m_channelSelectionModel->isChecked(m_channelIt.key()) == false) && (DabTables::channelList.constEnd() != m_channelIt))
    {
        ++m_channelIt;
    }

    if (DabTables::channelList.constEnd() == m_channelIt)
    {
        if (++m_scanCycleCntr == numCycles())
        {  // scan finished
            stopScan();
            return;
        }

        // restarting
        m_channelIt = DabTables::channelList.constBegin();
        if (numCycles() == 0)
        {  // endless scan
            progressValue(0);
        }

        // find first active channel
        while ((m_channelSelectionModel->isChecked(m_channelIt.key()) == false) && (DabTables::channelList.constEnd() != m_channelIt))
        {
            ++m_channelIt;
        }
    }

    progressValue(m_progressValue + 1);
    if (numCycles() == 1)
    {
        progressChannel(m_channelIt.value());
    }
    else
    {
        progressChannel(QString(tr("%1  (cycle %2)")).arg(m_channelIt.value()).arg(m_scanCycleCntr + 1));
    }

    if (m_frequency != m_channelIt.key())
    {
        m_frequency = m_channelIt.key();
        m_numServicesFound = 0;
        m_ensemble.reset();
        m_state = ScannerState::WaitForTune;
        qCInfo(scanner) << "Tune:" << m_frequency;
        emit tuneChannel(m_frequency);
    }
    else
    {  // this is a case when only 1 channel is selected for scanning
        m_state = ScannerState::WaitForEnsemble;
        onEnsembleInformation(m_ensemble);
    }
}

void ScannerBackend::onTuneDone(uint32_t freq)
{
    switch (m_state)
    {
        case ScannerState::Init:
            if (m_timer->isActive())
            {
                m_timer->stop();
            }
            scanStep();
            break;
        case ScannerState::Interrupted:
            // exit
            stopScan();
            break;
        case ScannerState::WaitForTune:
            // tuned to some frequency -> wait for sync
            m_state = ScannerState::WaitForSync;
            m_timer->start(m_settings->scanner.waitForSync * 1000);
            qCDebug(scanner) << "Waiting for sync @" << m_frequency;
            break;
        default:
            // do nothing
            isStartStopEnabled(true);
            break;
    }
}

void ScannerBackend::onSignalState(uint8_t sync, float snr)
{
    if (DabSyncLevel::NullSync <= DabSyncLevel(sync))
    {
        if (ScannerState::WaitForSync == m_state)
        {  // if we are waiting for sync (move to next step)
            m_timer->stop();
            m_state = ScannerState::WaitForEnsemble;
            m_timer->start(m_settings->scanner.waitForEnsemble * 1000);
            qCInfo(scanner) << "Signal found, waiting for ensemble info @" << m_frequency;
        }
    }
    if (m_ensemble.isValid() && m_isScanning)
    {
        m_snr += snr;
        m_snrCntr += 1;
    }
}

void ScannerBackend::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    if (ScannerState::WaitForEnsemble != m_state)
    {  // do nothing
        return;
    }
    m_timer->stop();
    if (ens.isValid())
    {  // this shoud be the normal case
        m_state = ScannerState::WaitForTII;

        // this will stop when TII data is received
        m_timer->start(5000 + mode() * 5000);

        m_ensemble = ens;
        qCInfo(scanner, "Ensemble info: %s %6.6X @ %d kHz, waiting for TII", m_ensemble.label.toUtf8().data(), m_ensemble.ueid, m_ensemble.frequency);

        m_snr = 0.0;
        m_snrCntr = 0;
        m_tiiCntr = 0;
        if (m_isTiiActive == false)
        {
            emit setTii(true);
            m_isTiiActive = true;
        }
    }
    else
    {  // this can happen in single channel mode in no signal case
        // wait for ensemble
        qCDebug(scanner) << "Invalid ensemble info, still waiting @" << m_frequency;
        m_timer->start(m_settings->scanner.waitForEnsemble * 1000);
    }
}

void ScannerBackend::onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &)
{
    if (m_state > ScannerState::WaitForEnsemble)
    {
        m_numServicesFound += 1;
    }
}

void ScannerBackend::onTiiData(const RadioControlTIIData &data)
{
    if ((ScannerState::WaitForTII == m_state) && m_ensemble.isValid())
    {
        qCDebug(scanner) << "TII data @" << m_frequency;
        if (++m_tiiCntr == mode())
        {
            if (nullptr != m_timer && m_timer->isActive())
            {
                m_timer->stop();
            }

            if (m_isPreciseMode)
            {  // request ensemble info
                m_tiiData = data;
                qCDebug(scanner) << "Requesting ensemble config @" << m_frequency;
                emit requestEnsembleConfiguration();
            }
            else
            {
                storeEnsembleData(data, QString(), QString());
            }
        }
    }
}

void ScannerBackend::storeEnsembleData(const RadioControlTIIData &tiiData, const QString &conf, const QString &csvConf)
{
    qCDebug(scanner) << "Storing results @" << m_frequency;

    int firstNewRow = m_model->rowCount();

    m_model->appendEnsData(QDateTime::currentDateTime(), tiiData.idList, ServiceListId(m_ensemble), m_ensemble.label, conf, csvConf,
                           m_numServicesFound, m_snr / m_snrCntr);

    int lastNewRow = m_model->rowCount() - 1;
    if (m_autoSaveFile != nullptr && lastNewRow >= firstNewRow)
    {
        appendAutoSaveRows(firstNewRow, lastNewRow);
    }

    if (m_isTiiActive && m_numSelectedChannels > 1)
    {
        emit setTii(false);
        m_isTiiActive = false;
    }

    // handle selection
    int id = -1;
    QModelIndexList selectedList = m_tableSelectionModel->selectedRows();
    if (!selectedList.isEmpty())
    {
        QModelIndex currentIndex = selectedList.at(0);
    }

    // forcing update of UI
    onSelectionChanged(QItemSelection(), QItemSelection());

    qCInfo(scanner) << "Done:" << m_frequency;

    // next channel
    scanStep();
}

void ScannerBackend::createContextMenu(int row)
{
    auto selectedRows = m_tableSelectionModel->selectedRows();
    int isLocal = 0;
    for (auto it = selectedRows.cbegin(); it != selectedRows.cend(); ++it)
    {
        QModelIndex srcIndex = m_sortedFilteredModel->mapToSource(*it);
        if (srcIndex.isValid())
        {  // local is +1, not local is -1
            isLocal += m_model->data(srcIndex, TxTableModel::IsLocalRole).toBool() ? 1 : -1;
        }
    }
    // isLocal > 0 means that more Tx in selection is marked as local
    // isLocal < 0 means that more Tx in selection is NOT marked as local
    // isLocal = 0 means the same number of loc and NOT local => offering marking as local

    bool markAsLocalAction = (isLocal <= 0);
    m_contextMenuModel->addMenuItem(markAsLocalAction ? tr("Mark as local (known) transmitter") : tr("Unmark local (known) transmitter"),
                                    ContextMenuActionId::MarkLocal, QVariant(markAsLocalAction), true);
    m_contextMenuModel->addMenuItem(tr("Show ensemble information"), ContextMenuActionId::ShowEnsembleInfo, QVariant(row),
                                    m_isPreciseMode && m_tableSelectionModel->selectedRows().count() == 1);
}

void ScannerBackend::showEnsembleConfig(int row)
{
    auto index = m_sortedFilteredModel->index(row, 0);
    if (index.isValid() && m_isPreciseMode)
    {
        QModelIndex srcIndex = m_sortedFilteredModel->mapToSource(index);
        if (srcIndex.isValid())
        {
            emit showEnsembleConfigDialog(srcIndex.row(), m_model->itemAt(srcIndex.row()).ensConfig());
        }
    }
}

void ScannerBackend::handleContextMenuAction(int actionId, const QVariant &data)
{
    switch (static_cast<ContextMenuActionId>(actionId))
    {
        case MarkLocal:
        {
            auto selectedRows = m_tableSelectionModel->selectedRows();
            for (auto it = selectedRows.cbegin(); it != selectedRows.cend(); ++it)
            {
                QModelIndex srcIndex = m_sortedFilteredModel->mapToSource(*it);
                if (srcIndex.isValid())
                {
                    m_model->setAsLocalTx(srcIndex, data.toBool());
                }
            }
        }
        break;
        case ShowEnsembleInfo:
        {
            int row = data.toInt();
            showEnsembleConfig(row);
        }
        break;
        default:
            // do nothing
            break;
    }
}

void ScannerBackend::saveEnsembleCSV(int srcModelRow)
{
    auto item = m_model->itemAt(srcModelRow);

    static const QRegularExpression regexp("[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    QString ensemblename = item.ensLabel();
    ensemblename.replace(regexp, "_");
    uint32_t frequency = item.ensId().freq();

    const QString ensemblePath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, ENSEMBLE_DIR_NAME);

    if (!AndroidFileHelper::mkpath(m_settings->dataStoragePath, ENSEMBLE_DIR_NAME))
    {
        qCWarning(scanner) << "Failed to create ensemble export directory:" << AndroidFileHelper::lastError();
        return;
    }

    if (!AndroidFileHelper::hasWritePermission(ensemblePath))
    {
        qCWarning(scanner) << "No permission to write to:" << ensemblePath;
        return;
    }

    const QString fileName =
        QString("%1_%2_%3.csv").arg(item.rxTime().toString("yyyy-MM-dd_hhmmss"), DabTables::channelList.value(frequency), ensemblename);

    if (AndroidFileHelper::writeTextFile(ensemblePath, fileName, item.ensConfigCSV(), "text/csv"))
    {
        qCInfo(scanner) << "Ensemble CSV exported to:" << ensemblePath << "/" << fileName;
    }
    else
    {
        qCWarning(scanner) << "Failed to export ensemble CSV:" << AndroidFileHelper::lastError();
    }
}

void ScannerBackend::setIsActive(bool isActive)
{
    TxMapBackend::setIsActive(isActive);
}

void ScannerBackend::onEnsembleConfigurationAndCSV(const QString &config, const QString &csvString)
{
    qCDebug(scanner) << "Ensemble config received @" << m_frequency;
    storeEnsembleData(m_tiiData, config, csvString);
}

void ScannerBackend::onInputDeviceError(const InputDevice::ErrorCode)
{
    if (m_isScanning)
    {  // stop pressed
        isStartStopEnabled(false);
        isScanning(false);
        m_ensemble.reset();

        // the state machine has 4 possible states
        // 1. wait for tune (event)
        // 2. wait for sync (timer or event)
        // 4. wait for ensemble (timer or event)
        // 5. wait for tii (timer)
        if (m_timer->isActive())
        {  // state 2, 3, 4
            m_timer->stop();
        }
        stopScan();
        stopAutoSaveCsv();
        scanningLabel(tr("Scanning failed"));
    }
}

void ScannerBackend::loadSettings()
{
    setZoomLevel(m_settings->scanner.mapZoom);
    if (m_settings->scanner.mapCenter.isValid())
    {
        setMapCenter(m_settings->scanner.mapCenter);
    }
    setCenterToCurrentPosition(m_settings->scanner.centerMapToCurrentPosition);

    // check validity of mode -> forcing normal as fallback
    if (m_settings->scanner.mode != Mode_Fast && m_settings->scanner.mode != Mode_Precise)
    {
        m_settings->scanner.mode = Mode_Normal;
    }

    m_sortedFilteredModel->setLocalTxFilter(m_settings->scanner.hideLocalTx);

    // if (!m_settings->scanner.geometry.isEmpty())
    // {
    //     restoreGeometry(m_settings->scanner.geometry);
    // }
}

void ScannerBackend::onSelectedRowChanged()
{
    m_txInfo.clear();
    m_currentEnsemble.reset();
    if (selectedRow() < 0)
    {  // reset info
        emit txInfoChanged();
        emit ensembleInfoChanged();
        return;
    }

    TxTableModelItem item = m_model->data(m_model->index(selectedRow(), 0), TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
    if (item.hasTxData())
    {
        m_txInfo.append(QString("<b>%1</b>").arg(item.transmitterData().location()));
        QGeoCoordinate coord = QGeoCoordinate(item.transmitterData().coordinates().latitude(), item.transmitterData().coordinates().longitude());
        m_txInfo.append(QString("GPS: <b>%1</b>").arg(coord.toString(QGeoCoordinate::DegreesWithHemisphere)));
        float alt = item.transmitterData().coordinates().altitude();
        if (alt)
        {
            m_txInfo.append(QString(tr("Altitude: <b>%1 m</b>")).arg(static_cast<int>(alt)));
        }
        int antHeight = item.transmitterData().antHeight();
        if (antHeight)
        {
            m_txInfo.append(QString(tr("Antenna height: <b>%1 m</b>")).arg(static_cast<int>(antHeight)));
        }
        m_txInfo.append(QString(tr("ERP: <b>%1 kW</b>")).arg(static_cast<double>(item.transmitterData().power()), 3, 'f', 1));
    }
    emit txInfoChanged();

    m_currentEnsemble.label = item.ensLabel();
    m_currentEnsemble.ueid = item.ensId().ueid();
    m_currentEnsemble.frequency = item.ensId().freq();

    emit ensembleInfoChanged();
}

void ScannerBackend::clearTableAction()
{
    m_messageBoxBackend->showQuestion(
        tr("Clear scan results?"), tr("You will loose current scan results, this action is irreversible."),
        [this](MessageBoxBackend::StandardButton result)
        {
            if (result == MessageBoxBackend::StandardButton::Yes)
            {
                QTimer::singleShot(10, this, [this]() { reset(); });
            }
            else
            {  // do nothing
            }
        },
        {{MessageBoxBackend::StandardButton::Yes, tr("Clear"), MessageBoxBackend::ButtonRole::Negative},
         {MessageBoxBackend::StandardButton::No, tr("Cancel"), MessageBoxBackend::ButtonRole::Neutral}},
        MessageBoxBackend::StandardButton::Yes  // default button
    );
}

void ScannerBackend::clearLocalTxAction()
{
    m_messageBoxBackend->showQuestion(
        tr("Clear local (known) transmitter database?"), tr("You will loose all records in the database, this action is irreversible."),
        [this](MessageBoxBackend::StandardButton result)
        {
            if (result == MessageBoxBackend::StandardButton::Yes)
            {
                QTimer::singleShot(10, this, [this]() { m_model->clearLocalTx(); });
            }
            else
            {  // do nothing
            }
        },
        {{MessageBoxBackend::StandardButton::Yes, tr("Clear"), MessageBoxBackend::ButtonRole::Negative},
         {MessageBoxBackend::StandardButton::No, tr("Cancel"), MessageBoxBackend::ButtonRole::Neutral}},
        MessageBoxBackend::StandardButton::Yes  // default button
    );
}

ChannelSelectionModel::ChannelSelectionModel(Settings *settings, QObject *parent) : QAbstractListModel(parent), m_settings(settings)
{
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        m_modelData.append({it.key(), m_settings->scanner.channelSelection.value(it.key(), true)});
    }
}

QVariant ChannelSelectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    int row = index.row();
    if ((row < 0) || (row >= m_modelData.count()))
    {
        return QVariant();
    }

    if (role == NameRole)
    {
        return DabTables::channelList.value(m_modelData.at(row).freq);
    }

    if (role == IsSelectedRole)
    {
        return m_modelData.at(row).isSelected;
    }

    return QVariant();
}

QHash<int, QByteArray> ChannelSelectionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(NameRole, "channelName");
    roles.insert(IsSelectedRole, "isSelected");
    return roles;
}

void ChannelSelectionModel::setChecked(int row, bool checked)
{
    if ((row < 0) || (row >= m_modelData.count()))
    {
        return;
    }

    m_modelData[row].isSelected = checked;
    emit dataChanged(index(row, 0), index(row, 0), {IsSelectedRole});
}

void ChannelSelectionModel::save() const
{
    m_settings->scanner.channelSelection.clear();
    for (auto it = m_modelData.cbegin(); it != m_modelData.cend(); ++it)
    {
        m_settings->scanner.channelSelection[it->freq] = it->isSelected;
    }
}

int ChannelSelectionModel::numChecked() const
{
    int ret = 0;
    for (const auto ch : m_modelData)
    {
        ret += ch.isSelected ? 1 : 0;
    }
    return ret;
}

bool ChannelSelectionModel::isChecked(uint32_t freq) const
{
    for (int n = 0; n < m_modelData.size(); ++n)
    {
        if (m_modelData.at(n).freq == freq)
        {
            return m_modelData.at(n).isSelected;
        }
    }
    return false;
}
