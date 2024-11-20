/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QGridLayout>
#include "scannerdialog.h"
#include "dabtables.h"
#include "settings.h"

Q_LOGGING_CATEGORY(scanner, "Scanner", QtInfoMsg)

ScannerDialog::ScannerDialog(Settings * settings, QWidget *parent) :
    TxMapDialog(settings, false, parent)
{
    // UI
    setWindowTitle(tr("Scanning Tool"));
    setModal(true);
    setMinimumSize(QSize(600, 400));
    resize(QSize(800, 800));

    // Set window flags to add maximize and minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    m_sortedFilteredModel->setFilter(false);

    // QML View
    m_qmlView = new QQuickView();
    QQmlContext * context = m_qmlView->rootContext();
    context->setContextProperty("tii", this);
    context->setContextProperty("tiiTable", m_model);
    context->setContextProperty("tiiTableSorted", m_sortedFilteredModel);
    context->setContextProperty("tiiTableSelectionModel", m_tableSelectionModel);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicyContainer(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicyContainer.setVerticalStretch(255);
    container->setSizePolicy(sizePolicyContainer);

    // scanner widget
    QGridLayout * gridLayout = new QGridLayout(this);
    QHBoxLayout * horizontalLayout_1 = new QHBoxLayout();
    m_scanningLabel = new QLabel(this);

    horizontalLayout_1->addWidget(m_scanningLabel);

    m_progressChannel = new QLabel(this);

    horizontalLayout_1->addWidget(m_progressChannel);

    QHBoxLayout * horizontalLayout_2 = new QHBoxLayout();
    horizontalLayout_2->addLayout(horizontalLayout_1);


    horizontalLayout_2->addItem(new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
    gridLayout->addLayout(horizontalLayout_2, 0, 0, 1, 4);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setValue(24);
    m_progressBar->setTextVisible(false);

    gridLayout->addWidget(m_progressBar, 1, 0, 1, 4);

    m_txTableView = new QTableView(this);

    gridLayout->addWidget(m_txTableView, 2, 0, 1, 4);

    m_startStopButton = new QPushButton(this);
    gridLayout->addWidget(m_startStopButton, 3, 3, 1, 1);

    gridLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum), 3, 2, 1, 1);

    m_exportButton = new QPushButton(this);
    gridLayout->addWidget(m_exportButton, 3, 0, 1, 2);

    QWidget * scannerWidget = new QWidget(this);
    scannerWidget->setLayout(gridLayout);

    m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(container);
    m_splitter->addWidget(scannerWidget);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_splitter);


    m_txTableView->setModel(m_sortedFilteredModel);
    m_txTableView->setSelectionModel(m_tableSelectionModel);
    m_txTableView->verticalHeader()->hide();
    m_txTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_txTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_txTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_txTableView->setSortingEnabled(false);

    m_startStopButton->setText(tr("Start"));
    connect(m_startStopButton, &QPushButton::clicked, this, &ScannerDialog::startStopClicked);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(DabTables::channelList.size());
    m_progressBar->setValue(0);
    m_progressChannel->setText(tr("None"));
    m_progressChannel->setVisible(false);
    m_scanningLabel->setText("");
    m_exportButton->setText(tr("Export as CSV"));
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &ScannerDialog::exportClicked);

    m_ensemble.ueid = RADIO_CONTROL_UEID_INVALID;
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
    {   // stop pressed
        m_startStopButton->setEnabled(false);

        // the state machine has 4 possible states
        // 1. wait for tune (event)
        // 2. wait for sync (timer or event)
        // 4. wait for ensemble (timer or event)
        // 5. wait for services (timer)
        if (m_timer->isActive())
        {   // state 2, 3, 4
            m_timer->stop();
            stopScan();
        }
        else {
            // timer not running -> state 1
            m_state = ScannerState::Interrupted;  // ==> it will be finished when tune is complete
        }
    }
    else
    {   // start pressed
        m_startStopButton->setText(tr("Stop"));
        startScan();
    }
}

void ScannerDialog::stopScan()
{
    if (m_isTiiActive)
    {
        emit setTii(false, 0.0);
        m_isTiiActive = false;
    }

    if (m_exitRequested)
    {
        done(QDialog::Accepted);
    }

    // restore UI
    m_progressChannel->setVisible(false);
    m_scanningLabel->setText(tr("Scanning finished"));
    m_progressBar->setValue(0);
    m_progressChannel->setText(tr("None"));
    m_startStopButton->setText(tr("Start"));
    m_startStopButton->setEnabled(true);

    m_isScanning = false;
    m_state = ScannerState::Init;
}

void ScannerDialog::exportClicked()
{
    static const QRegularExpression regexp( "[" + QRegularExpression::escape("/:*?\"<>|") + "]");

    QString f = QString("%1/%2.csv").arg(m_settings->scanner.exportPath, m_scanStartTime.toString("yyyy-MM-dd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export CSV file"),
                                                    QDir::toNativeSeparators(f),
                                                    tr("CSV Files")+" (*.csv)");

    if (!fileName.isEmpty())
    {
        m_settings->scanner.exportPath = QFileInfo(fileName).path(); // store path for next time
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly))
        {
            QTextStream out(&file);

            // Header
            for (int col = 0; col < TxTableModel::NumCols-1; ++col)
            {
                out << m_model->headerData(col, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
            }
            out << m_model->headerData(TxTableModel::NumCols - 1, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString() <<  Qt::endl;

            // Body
            for (int row = 0; row < m_model->rowCount(); ++row)
            {
                for (int col = 0; col < TxTableModel::NumCols-1; ++col)
                {
                    out << m_model->data(m_model->index(row, col), TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
                }
                out << m_model->data(m_model->index(row, TxTableModel::NumCols-1), TxTableModel::TxTableModelRoles::ExportRole).toString() << Qt::endl;
            }
            file.close();
        }
    }
}

void ScannerDialog::startScan()
{
    m_isScanning = true;

    m_scanStartTime = QDateTime::currentDateTime();
    m_scanningLabel->setText(tr("Scanning channel:"));
    m_progressChannel->setVisible(true);

    m_model->clear();
    m_exportButton->setEnabled(false);

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
}

void ScannerDialog::scanStep()
{
    if (ScannerState::Init == m_state)
    {   // first step
        m_channelIt = DabTables::channelList.constBegin();
    }
    else
    {   // next step
        ++m_channelIt;
    }    

    if (DabTables::channelList.constEnd() == m_channelIt)
    {
        // scan finished
        stopScan();
        return;
    }

    m_progressBar->setValue(m_progressBar->value()+1);
    //m_progressChannel->setText(QString("%1 [ %2 MHz ]").arg(m_channelIt.value()).arg(m_channelIt.key()/1000.0, 3, 'f', 3, QChar('0')));
    m_progressChannel->setText(m_channelIt.value());
    m_numServicesFound = 0;
    m_ensemble.reset();
    m_state = ScannerState::WaitForTune;
    emit tuneChannel(m_channelIt.key());
}

void ScannerDialog::onTuneDone(uint32_t freq)
{
    if (ScannerState::Init == m_state)
    {
        if (m_timer->isActive())
        {
            m_timer->stop();
        }
        scanStep();
    }
    else if (ScannerState::Interrupted == m_state)
    {   // exit
        stopScan();
    }
    else
    {   // tuned to some frequency -> wait for sync
        m_state = ScannerState::WaitForSync;
        m_timer->start(3000);
    }
}

void ScannerDialog::onSyncStatus(uint8_t sync, float snr)
{
    if (DabSyncLevel::NullSync <= DabSyncLevel(sync))
    {
        if (ScannerState::WaitForSync == m_state)
        {   // if we are waiting for sync (move to next step)
            m_timer->stop();
            m_state = ScannerState::WaitForEnsemble;
            m_timer->start(6000);
        }
    }
    if (m_ensemble.isValid())
    {
        m_snr = snr;
    }
}

void ScannerDialog::onEnsembleInformation(const RadioControlEnsemble & ens)
{
    if (ScannerState::Idle == m_state)
    {   // do nothing
        return;
    }

    m_timer->stop();

    m_state = ScannerState::WaitForServices;

    // thiw will not be interrupted -> collecting data from now
    m_timer->start(10000);

    //qDebug() << m_channelIt.value() << ens.eid() << ens.label;
    //qDebug() << Q_FUNC_INFO << QString("---------------------------------------------------------------- %1   %2   '%3'").arg(m_channelIt.value()).arg(ens.ueid, 6, 16, QChar('0')).arg(ens.label);
    m_ensemble = ens;
    m_snr = 0.0;
    emit setTii(true, 0.0);
    m_isTiiActive = true;
}

void ScannerDialog::onServiceFound(const ServiceListId &)
{
    //ui->numServicesFoundLabel->setText(QString("%1").arg(++m_numServicesFound));
    m_numServicesFound += 1;
}

void ScannerDialog::onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &)
{
    m_numServicesFound += 1;

    //ui->numServicesFoundLabel->setText(QString("%1").arg(++m_numServicesFound));
}

void ScannerDialog::onTiiData(const RadioControlTIIData &data)
{
    if (m_ensemble.isValid())
    {
        m_model->appendEnsData(data.idList, ServiceListId(m_ensemble), m_ensemble.label, m_numServicesFound, m_snr);
        m_exportButton->setEnabled(true);

        //m_txTableView->resizeColumnsToContents();
        m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_txTableView->horizontalHeader()->setSectionResizeMode(TxTableModel::ColName, QHeaderView::Stretch);
        //m_txTableView->horizontalHeader()->setStretchLastSection(true);

        emit setTii(false, 0.0);
        m_isTiiActive = false;


        // handle selection
        int id = -1;
        QModelIndexList	selectedList = m_tableSelectionModel->selectedRows();
        if (!selectedList.isEmpty())
        {
            QModelIndex currentIndex = selectedList.at(0);
            id = m_sortedFilteredModel->data(currentIndex, TxTableModel::TxTableModelRoles::IdRole).toInt();
        }

        // forcing update of UI
        onSelectionChanged(QItemSelection(),QItemSelection());


        if ((nullptr != m_timer) && (m_timer->isActive()))
        {
            m_timer->stop();
            scanStep();
        }
    }
}

void ScannerDialog::setSelectedRow(int modelRow)
{
    if (m_selectedRow == modelRow)
    {
        return;
    }
    m_selectedRow = modelRow;
    emit selectedRowChanged();

    m_txInfo.clear();
    m_currentEnsemble.reset();
    if (modelRow < 0)
    {   // reset info
        emit txInfoChanged();
        emit ensembleInfoChanged();
        return;
    }

    TxTableModelItem item = m_model->data(m_model->index(modelRow, 0), TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
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
}

void ScannerDialog::showEvent(QShowEvent *event)
{
    // calculate size of the table
    // int hSize = 0;
    // for (int n = 0; n < m_txTableView->horizontalHeader()->count(); ++n) {
    //     hSize += m_txTableView->horizontalHeader()->sectionSize(n);
    // }
    //m_txTableView->setMinimumWidth(hSize + m_txTableView->verticalScrollBar()->sizeHint().width() + TxTableModel::NumCols);
    m_txTableView->setMinimumWidth(700);

    //m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    //m_txTableView->horizontalHeader()->setSectionResizeMode(TxTableModel::ColName, QHeaderView::Stretch);
    m_txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //m_txTableView->setMinimumWidth(m_txTableView->width());

    //m_txTableView->horizontalHeader()->setStretchLastSection(true);

    TxMapDialog::showEvent(event);
}

void ScannerDialog::closeEvent(QCloseEvent *event)
{
    if (m_isScanning)
    {   // stopping scanning first
        m_exitRequested = true;
        startStopClicked();
        return;
    }

    TxMapDialog::closeEvent(event);
}

void ScannerDialog::onServiceListComplete(const RadioControlEnsemble &)
{   // this means that ensemble information is complete => stop timer and do next set
    qDebug() << Q_FUNC_INFO;

    // if ((nullptr != m_timer) && (m_timer->isActive()))
    // {
    //     m_timer->stop();
    //     scanStep();
    // }
}


