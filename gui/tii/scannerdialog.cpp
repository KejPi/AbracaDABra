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

#include <QDebug>
#include <QHeaderView>

#include "channelselectiondialog.h"
#include "ensembleconfigdialog.h"
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QQmlContext>
#include <QSpinBox>

#include "dabtables.h"
#include "scannerdialog.h"
#include "settings.h"

Q_LOGGING_CATEGORY(scanner, "Scanner", QtDebugMsg)

ScannerDialog::ScannerDialog(Settings *settings, QWidget *parent) : TxMapDialog(settings, false, parent)
{
    // UI
    setWindowTitle(tr("DAB Scanning Tool"));
    setMinimumSize(QSize(800, 500));
    resize(QSize(900, 800));

    // Set window flags to add maximize and minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    m_sortedFilteredModel->setFilter(false);

    // QML View
    m_qmlView = new QQuickView();
    QQmlContext *context = m_qmlView->rootContext();
    context->setContextProperty("tiiBackend", this);
    context->setContextProperty("tiiTable", m_model);
    context->setContextProperty("tiiTableSorted", m_sortedFilteredModel);
    context->setContextProperty("tiiTableSelectionModel", m_tableSelectionModel);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *qmlContainer = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicyContainer(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicyContainer.setVerticalStretch(255);
    qmlContainer->setSizePolicy(sizePolicyContainer);

    // scanner widget
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *controlsLayout = new QHBoxLayout();

    m_scanningLabel = new QLabel(this);
    m_progressChannel = new QLabel(this);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setTextVisible(false);

    m_exportButton = new QPushButton(this);
    m_channelListButton = new QPushButton(this);
    m_startStopButton = new QPushButton(this);
    m_startStopButton->setDefault(true);

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::Shape::VLine);
    line->setFrameShadow(QFrame::Shadow::Sunken);

    QLabel *labelMode = new QLabel(this);
    labelMode->setText(tr("Mode:"));

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Fast"), Mode_Fast);
    m_modeCombo->addItem(tr("Normal"), Mode_Normal);
    m_modeCombo->addItem(tr("Precise"), Mode_Precise);
    int idx = m_modeCombo->findData(m_settings->scanner.mode, Qt::UserRole);
    if (idx < 0)
    {  // set Mode_Normal as default
        idx = 1;
    }
    m_modeCombo->setCurrentIndex(idx);

    QLabel *labelCycles = new QLabel(this);
    m_numCyclesSpinBox = new QSpinBox(this);
    m_numCyclesSpinBox->setSpecialValueText(tr("Inf"));
    m_numCyclesSpinBox->setValue(m_settings->scanner.numCycles);
    labelCycles->setText(tr("Number of cycles:"));

    controlsLayout->addWidget(m_scanningLabel);
    controlsLayout->addWidget(m_progressChannel);
    controlsLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
    controlsLayout->addWidget(m_exportButton);
    controlsLayout->addWidget(line);
    controlsLayout->addWidget(labelMode);
    controlsLayout->addWidget(m_modeCombo);
    controlsLayout->addWidget(labelCycles);
    controlsLayout->addWidget(m_numCyclesSpinBox);

    controlsLayout->addWidget(m_channelListButton);
    controlsLayout->addWidget(m_startStopButton);
    mainLayout->addLayout(controlsLayout);

    mainLayout->addWidget(m_progressBar);

    m_txTableView = new QTableView(this);
    mainLayout->addWidget(m_txTableView);

    QWidget *scannerWidget = new QWidget(this);
    scannerWidget->setLayout(mainLayout);

    m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(qmlContainer);
    m_splitter->addWidget(scannerWidget);
    m_splitter->setChildrenCollapsible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    m_startStopButton->setFocus();

    if (!m_settings->scanner.splitterState.isEmpty())
    {
        m_splitter->restoreState(m_settings->scanner.splitterState);
    }

    m_txTableView->setModel(m_sortedFilteredModel);
    m_txTableView->setSelectionModel(m_tableSelectionModel);
    m_txTableView->verticalHeader()->hide();
    m_txTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_txTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_txTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_txTableView->setSortingEnabled(true);
    m_txTableView->sortByColumn(TxTableModel::ColTime, Qt::AscendingOrder);
    m_txTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_txTableView, &QTableView::customContextMenuRequested, this, &ScannerDialog::showContextMenu);
    connect(m_txTableView, &QTableView::doubleClicked, this, &ScannerDialog::showEnsembleConfig);

    m_startStopButton->setText(tr("Start"));
    connect(m_startStopButton, &QPushButton::clicked, this, &ScannerDialog::startStopClicked);
    m_progressBar->setMinimum(0);
    m_progressBar->setValue(0);
    m_progressChannel->setText(tr("None"));
    m_progressChannel->setVisible(false);
    m_scanningLabel->setText("");
    m_channelListButton->setText(tr("Select channels"));
    m_exportButton->setText(tr("Export as CSV"));
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &ScannerDialog::exportClicked);

    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        if (m_settings->scanner.channelSelection.contains(it.key()))
        {
            m_channelSelection.insert(it.key(), m_settings->scanner.channelSelection.value(it.key()));
        }
        else
        {
            m_channelSelection.insert(it.key(), true);
        }
    }
    connect(m_channelListButton, &QPushButton::clicked, this, &ScannerDialog::channelSelectionClicked);

    m_ensemble.ueid = RADIO_CONTROL_UEID_INVALID;

    QSize sz = size();
    if (!m_settings->scanner.geometry.isEmpty())
    {
        restoreGeometry(m_settings->scanner.geometry);
        sz = size();
    }
    QTimer::singleShot(10, this, [this, sz]() { resize(sz); });
}

ScannerDialog::~ScannerDialog()
{
    delete m_qmlView;

    if (nullptr != m_timer)
    {
        m_timer->stop();
        delete m_timer;
    }
}

void ScannerDialog::startStopClicked()
{
    if (m_isScanning)
    {  // stop pressed
        m_startStopButton->setEnabled(false);
        m_isScanning = false;
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
        m_startStopButton->setText(tr("Stop"));
        m_numCyclesSpinBox->setEnabled(false);
        m_modeCombo->setEnabled(false);
        m_numSelectedChannels = 0;
        m_isPreciseMode = (m_modeCombo->currentData().toInt() == Mode::Mode_Precise);
        for (const auto ch : m_channelSelection)
        {
            m_numSelectedChannels += ch ? 1 : 0;
        }
        if (m_numCyclesSpinBox->value() > 0)
        {
            m_progressBar->setMaximum(m_numSelectedChannels * m_numCyclesSpinBox->value());
        }
        else
        {
            m_progressBar->setMaximum(m_numSelectedChannels);
        }
        startScan();
    }
}

void ScannerDialog::stopScan()
{
    if (m_isTiiActive)
    {
        emit setTii(false);
        m_isTiiActive = false;
    }

    if (m_exitRequested)
    {
        close();
    }

    // restore UI
    m_progressChannel->setVisible(false);
    m_scanningLabel->setText(tr("Scanning finished"));
    m_progressBar->setValue(0);
    m_progressChannel->setText(tr("None"));
    m_startStopButton->setText(tr("Start"));
    m_startStopButton->setEnabled(true);
    m_numCyclesSpinBox->setEnabled(true);
    m_channelListButton->setEnabled(true);
    m_modeCombo->setEnabled(true);

    m_isScanning = false;
    m_state = ScannerState::Idle;

    emit scanFinished();
}

void ScannerDialog::exportClicked()
{
    QString f = QString("%1/%2.csv").arg(m_settings->scanner.exportPath, m_scanStartTime.toString("yyyy-MM-dd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export CSV file"), QDir::toNativeSeparators(f), tr("CSV Files") + " (*.csv)");

    if (!fileName.isEmpty())
    {
        m_settings->scanner.exportPath = QFileInfo(fileName).path();  // store path for next time
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream out(&file);

            // Header
            for (int col = 0; col < TxTableModel::NumCols - 1; ++col)
            {
                out << m_model->headerData(col, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
            }
            out << m_model->headerData(TxTableModel::NumCols - 1, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString() << Qt::endl;

            // Body
            for (int row = 0; row < m_model->rowCount(); ++row)
            {
                for (int col = 0; col < TxTableModel::NumCols - 1; ++col)
                {
                    out << m_model->data(m_model->index(row, col), TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
                }
                out << m_model->data(m_model->index(row, TxTableModel::NumCols - 1), TxTableModel::TxTableModelRoles::ExportRole).toString()
                    << Qt::endl;
            }
            file.close();
        }
    }
}

void ScannerDialog::channelSelectionClicked()
{
    auto dialog = new ChannelSelectionDialog(m_channelSelection, this);
    connect(dialog, &QDialog::accepted, this, [this, dialog]() { dialog->getChannelList(m_channelSelection); });
    connect(dialog, &ChannelSelectionDialog::finished, dialog, &QObject::deleteLater);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->open();
    // dialog->setModal(true);
    // dialog->show();
}

void ScannerDialog::startScan()
{
    m_isScanning = true;

    reset();
    m_scanStartTime = QDateTime::currentDateTime();
    m_scanningLabel->setText(tr("Scanning channel:"));
    m_progressChannel->setVisible(true);
    m_exportButton->setEnabled(false);
    m_channelListButton->setEnabled(false);
    m_scanCycleCntr = 0;
    m_frequency = 0;

    if (m_timer == nullptr)
    {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &ScannerDialog::scanStep);
    }

    m_state = ScannerState::Init;

    // using timer for mainwindow to cleanup and tune to 0 potentially (no timeout in case)
    m_timer->start(2000);
    emit scanStarts();

    qCInfo(scanner) << "Scanning starts";
}

void ScannerDialog::scanStep()
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
    while ((m_channelSelection.value(m_channelIt.key()) == false) && (DabTables::channelList.constEnd() != m_channelIt))
    {
        ++m_channelIt;
    }

    if (DabTables::channelList.constEnd() == m_channelIt)
    {
        if (++m_scanCycleCntr == m_numCyclesSpinBox->value())
        {  // scan finished
            stopScan();
            return;
        }

        // restarting
        m_channelIt = DabTables::channelList.constBegin();
        if (m_numCyclesSpinBox->value() == 0)
        {  // endless scan
            m_progressBar->setValue(0);
        }

        // find first active channel
        while ((m_channelSelection.value(m_channelIt.key()) == false) && (DabTables::channelList.constEnd() != m_channelIt))
        {
            ++m_channelIt;
        }
    }

    m_progressBar->setValue(m_progressBar->value() + 1);
    // m_progressChannel->setText(QString("%1 [ %2 MHz ]").arg(m_channelIt.value()).arg(m_channelIt.key()/1000.0, 3, 'f', 3, QChar('0')));
    // m_progressChannel->setText(QString("%1 (%2 / %3)").arg(m_channelIt.value()).arg(m_progressBar->value()).arg(m_progressBar->maximum()));
    if (m_numCyclesSpinBox->value() == 1)
    {
        m_progressChannel->setText(m_channelIt.value());
    }
    else
    {
        m_progressChannel->setText(QString(tr("%1  (cycle %2)")).arg(m_channelIt.value()).arg(m_scanCycleCntr + 1));
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

void ScannerDialog::onTuneDone(uint32_t freq)
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
            break;
    }
}

void ScannerDialog::onSignalState(uint8_t sync, float snr)
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

void ScannerDialog::onEnsembleInformation(const RadioControlEnsemble &ens)
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
        m_timer->start(5000 + m_modeCombo->currentData().toInt() * 5000);

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

void ScannerDialog::onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &)
{
    if (m_state > ScannerState::WaitForEnsemble)
    {
        m_numServicesFound += 1;
    }
}

void ScannerDialog::onTiiData(const RadioControlTIIData &data)
{
    if ((ScannerState::WaitForTII == m_state) && m_ensemble.isValid())
    {
        qCDebug(scanner) << "TII data @" << m_frequency;
        if (++m_tiiCntr == m_modeCombo->currentData().toInt())
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

void ScannerDialog::storeEnsembleData(const RadioControlTIIData &tiiData, const QString &conf, const QString &csvConf)
{
    qCDebug(scanner) << "Storing results @" << m_frequency;

    m_model->appendEnsData(tiiData.idList, ServiceListId(m_ensemble), m_ensemble.label, conf, csvConf, m_numServicesFound, m_snr / m_snrCntr);
    m_exportButton->setEnabled(true);

    m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_txTableView->horizontalHeader()->setSectionResizeMode(TxTableModel::ColLocation, QHeaderView::Stretch);

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

void ScannerDialog::showEnsembleConfig(const QModelIndex &index)
{
    if (m_isPreciseMode)
    {
        QModelIndex srcIndex = m_sortedFilteredModel->mapToSource(index);
        if (srcIndex.isValid())
        {
            auto dialog = new EnsembleConfigDialog(m_model->itemAt(srcIndex.row()), this);
            connect(dialog, &QDialog::finished, this,
                    [this, dialog]()
                    {
                        m_settings->scanner.exportPath = dialog->exportPath();
                        dialog->deleteLater();
                    });

            dialog->setExportPath(m_settings->scanner.exportPath);
            dialog->setWindowModality(Qt::WindowModal);
            dialog->open();
        }
    }
}

void ScannerDialog::showContextMenu(const QPoint &pos)
{
    if (m_isPreciseMode)
    {
        QModelIndex index = m_txTableView->indexAt(pos);
        if (index.isValid())
        {
            QMenu *menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            auto action = new QAction(tr("Show ensemble information"), this);
            connect(menu, &QWidget::destroyed, [=]() { // delete actions
                action->deleteLater();
            });
            menu->addAction(action);
            QAction *selectedItem = menu->exec(m_txTableView->viewport()->mapToGlobal(pos));
            if (nullptr != selectedItem)
            {
                showEnsembleConfig(index);
            }
        }
    }
}

void ScannerDialog::onEnsembleConfigurationAndCSV(const QString &config, const QString &csvString)
{
    qCDebug(scanner) << "Ensemble config received @" << m_frequency;
    storeEnsembleData(m_tiiData, config, csvString);
}

void ScannerDialog::onInputDeviceError(const InputDevice::ErrorCode)
{
    if (m_isScanning)
    {  // stop pressed
        m_startStopButton->setEnabled(false);
        m_isScanning = false;
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
        m_scanningLabel->setText(tr("Scanning failed"));
    }
}

void ScannerDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
        m_qmlView->setColor(Qt::black);
    }
    else
    {
        m_qmlView->setColor(Qt::white);
    }
}

void ScannerDialog::onSelectedRowChanged()
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
        m_txInfo.append(QString(tr("ERP: <b>%1 kW</b>")).arg(static_cast<double>(item.transmitterData().power()), 3, 'f', 1));
    }
    emit txInfoChanged();

    m_currentEnsemble.label = item.ensLabel();
    m_currentEnsemble.ueid = item.ensId().ueid();
    m_currentEnsemble.frequency = item.ensId().freq();

    emit ensembleInfoChanged();

    m_txTableView->scrollTo(m_sortedFilteredModel->mapFromSource(m_model->index(selectedRow(), 0)));
}

void ScannerDialog::showEvent(QShowEvent *event)
{
    if (!isVisible())
    {
        m_txTableView->setMinimumWidth(700);
        m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }

    TxMapDialog::showEvent(event);
}

void ScannerDialog::closeEvent(QCloseEvent *event)
{
    if (m_isScanning)
    {  // stopping scanning first
        m_exitRequested = true;
        QTimer::singleShot(50, this, [this]() { startStopClicked(); });
        event->ignore();
        return;
    }

    m_settings->scanner.splitterState = m_splitter->saveState();
    m_settings->scanner.geometry = saveGeometry();
    m_settings->scanner.numCycles = m_numCyclesSpinBox->value();
    m_settings->scanner.channelSelection = m_channelSelection;
    m_settings->scanner.mode = m_modeCombo->currentData().toInt();

    TxMapDialog::closeEvent(event);
}
