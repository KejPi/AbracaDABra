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
#include <QtWidgets/qscrollbar.h>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QLoggingCategory>
#include "scannerdialog.h"
#include "ui_scannerdialog.h"
#include "dabtables.h"
#include "settings.h"

Q_LOGGING_CATEGORY(scanner, "Scanner", QtInfoMsg)

ScannerDialog::ScannerDialog(Settings * settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScannerDialog),
    m_settings(settings)
{
    ui->setupUi(this);
    setModal(true);

    m_model = new TxTableModel(this);

    //setSizeGripEnabled(false);

    ui->txTableView->setModel(m_model);
    //ui->tiiTable->setSelectionModel(m_tiiTableSelectionModel);
    ui->txTableView->verticalHeader()->hide();
    ui->txTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //ui->txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->txTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->txTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->txTableView->setSortingEnabled(false);

    ui->startStopButton->setText(tr("Start"));
    connect(ui->startStopButton, &QPushButton::clicked, this, &ScannerDialog::startStopClicked);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(DabTables::channelList.size());
    ui->progressBar->setValue(0);
    ui->progressChannel->setText(tr("None"));
    ui->progressChannel->setVisible(false);
    ui->scanningLabel->setText("");
    ui->exportButton->setEnabled(false);
    connect(ui->exportButton, &QPushButton::clicked, this, &ScannerDialog::exportClicked);

    m_ensemble.ueid = RADIO_CONTROL_UEID_INVALID;
}

ScannerDialog::~ScannerDialog()
{
    if (nullptr != m_timer)
    {
        m_timer->stop();
        delete m_timer;
    }

    delete ui;
}

void ScannerDialog::startStopClicked()
{       
    if (m_isScanning)
    {   // stop pressed
        ui->startStopButton->setEnabled(false);

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
        ui->startStopButton->setText(tr("Stop"));
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
    ui->progressChannel->setVisible(false);
    ui->scanningLabel->setText(tr("Scanning finished"));
    ui->progressBar->setValue(0);
    ui->progressChannel->setText(tr("None"));
    ui->startStopButton->setText(tr("Start"));
    ui->startStopButton->setEnabled(true);

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
    ui->scanningLabel->setText(tr("Scanning channel:"));
    ui->progressChannel->setVisible(true);

    m_model->clear();
    ui->exportButton->setEnabled(false);

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

    ui->progressBar->setValue(ui->progressBar->value()+1);
    //ui->progressChannel->setText(QString("%1 [ %2 MHz ]").arg(m_channelIt.value()).arg(m_channelIt.key()/1000.0, 3, 'f', 3, QChar('0')));
    ui->progressChannel->setText(m_channelIt.value());
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

void ScannerDialog::onEnsembleFound(const RadioControlEnsemble & ens)
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
    m_model->appendEnsData(data.idList, ServiceListId(m_ensemble), m_ensemble.label, m_numServicesFound, m_snr);
    ui->exportButton->setEnabled(true);

    //ui->txTableView->resizeColumnsToContents();
    ui->txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->txTableView->horizontalHeader()->setSectionResizeMode(TxTableModel::ColName, QHeaderView::Stretch);
    //ui->txTableView->horizontalHeader()->setStretchLastSection(true);

    emit setTii(false, 0.0);
    m_isTiiActive = false;

    if ((nullptr != m_timer) && (m_timer->isActive()))
    {
        m_timer->stop();
        scanStep();
    }
}

void ScannerDialog::showEvent(QShowEvent *event)
{
    // calculate size of the table
    // int hSize = 0;
    // for (int n = 0; n < ui->txTableView->horizontalHeader()->count(); ++n) {
    //     hSize += ui->txTableView->horizontalHeader()->sectionSize(n);
    // }
    //ui->txTableView->setMinimumWidth(hSize + ui->txTableView->verticalScrollBar()->sizeHint().width() + TxTableModel::NumCols);
    ui->txTableView->setMinimumWidth(700);

    //ui->txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    //ui->txTableView->horizontalHeader()->setSectionResizeMode(TxTableModel::ColName, QHeaderView::Stretch);
    ui->txTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //ui->txTableView->setMinimumWidth(ui->txTableView->width());

    //ui->txTableView->horizontalHeader()->setStretchLastSection(true);

    startLocationUpdate();

    QDialog::showEvent(event);
}

void ScannerDialog::closeEvent(QCloseEvent *event)
{
    if (m_isScanning)
    {   // stopping scanning first
        m_exitRequested = true;
        startStopClicked();
        return;
    }

    stopLocationUpdate();

    QDialog::closeEvent(event);
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

void ScannerDialog::startLocationUpdate()
{
    switch (m_settings->tii.locationSource)
    {
    case Settings::GeolocationSource::System:
    {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        // ask for permission
#if QT_CONFIG(permissions)
        QLocationPermission locationsPermission;
        locationsPermission.setAccuracy(QLocationPermission::Precise);
        locationsPermission.setAvailability(QLocationPermission::WhenInUse);
        switch (qApp->checkPermission(locationsPermission)) {
        case Qt::PermissionStatus::Undetermined:
            qApp->requestPermission(locationsPermission, this, [this]() { startLocationUpdate(); } );
            qCDebug(scanner) << "LocationPermission Undetermined";
            return;
        case Qt::PermissionStatus::Denied:
        {
            qCInfo(scanner) << "LocationPermission Denied";
            QMessageBox msgBox(QMessageBox::Warning, tr("Warning"), tr("Device location access is denied."), {}, this);
            msgBox.setInformativeText(tr("If you want to display current position on map, grant the location permission in Settings then open the app again."));
            msgBox.exec();
        }
            return;
        case Qt::PermissionStatus::Granted:
            qCInfo(scanner) << "LocationPermission Granted";
            break; // Proceed
        }
#endif
#endif
    // start location update
        if (m_geopositionSource == nullptr)
        {
            m_geopositionSource = QGeoPositionInfoSource::createDefaultSource(this);
        }
        if (m_geopositionSource != nullptr)
        {
            qCDebug(scanner) << "Start position update";
            connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &ScannerDialog::positionUpdated);
            m_geopositionSource->startUpdates();
        }
    }
    break;
    case Settings::GeolocationSource::Manual:
        if (m_geopositionSource != nullptr)
        {
            delete m_geopositionSource;
            m_geopositionSource = nullptr;
        }
        positionUpdated(QGeoPositionInfo(m_settings->tii.coordinates, QDateTime::currentDateTime()));
        break;
    case Settings::GeolocationSource::SerialPort:
    {
        // serial port
        QVariantMap params;
        params["nmea.source"] = "serial:"+m_settings->tii.serialPort;
        m_geopositionSource = QGeoPositionInfoSource::createSource("nmea", params, this);
        if (m_geopositionSource != nullptr)
        {
            qCDebug(scanner) << "Start position update";
            connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &ScannerDialog::positionUpdated);
            m_geopositionSource->startUpdates();
        }
    }
    break;
    }
}

void ScannerDialog::stopLocationUpdate()
{
    if (m_geopositionSource != nullptr)
    {
        m_geopositionSource->stopUpdates();
    }
}

void ScannerDialog::positionUpdated(const QGeoPositionInfo &position)
{
    setCurrentPosition(position.coordinate());
    m_model->setCoordinates(m_currentPosition);
    setPositionValid(true);
}

QGeoCoordinate ScannerDialog::currentPosition() const
{
    return m_currentPosition;
}

void ScannerDialog::setCurrentPosition(const QGeoCoordinate &newCurrentPosition)
{
    if (m_currentPosition == newCurrentPosition)
        return;
    m_currentPosition = newCurrentPosition;
    emit currentPositionChanged();
}

bool ScannerDialog::positionValid() const
{
    return m_positionValid;
}

void ScannerDialog::setPositionValid(bool newPositionValid)
{
    if (m_positionValid == newPositionValid)
        return;
    m_positionValid = newPositionValid;
    emit positionValidChanged();
}


