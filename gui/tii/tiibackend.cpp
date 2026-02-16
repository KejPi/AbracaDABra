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

#include <QQmlContext>

#include "androidfilehelper.h"
#include "linechartitem.h"
#if QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>

#include "tiibackend.h"

Q_LOGGING_CATEGORY(tii, "TII", QtInfoMsg)

TIIBackend::TIIBackend(Settings *settings, QObject *parent) : TxMapBackend(settings, true, parent)
{
    m_columnProxyModel = new TxTableColumnProxyModel(this);
    m_columnProxyModel->setSourceModel(m_sortedFilteredModel);
    m_tableModel = m_columnProxyModel;
    setupSelectionModel();

    loadSettings();
    connect(this, &TxMapBackend::centerToCurrentPositionChanged, this,
            [this]() { m_settings->tii.centerMapToCurrentPosition = centerToCurrentPosition(); });
    connect(this, &TxMapBackend::mapCenterChanged, this, [this]() { m_settings->tii.mapCenter = mapCenter(); });
    connect(this, &TxMapBackend::zoomLevelChanged, this, [this]() { m_settings->tii.mapZoom = zoomLevel(); });
}

TIIBackend::~TIIBackend()
{
    if (m_logFile)
    {
        m_logFile->close();
        delete m_logFile;
    }
    if (m_inactiveCleanupTimer)
    {
        m_inactiveCleanupTimer->stop();
        delete m_inactiveCleanupTimer;
    }
}

void TIIBackend::onTiiData(const RadioControlTIIData &data)
{
    ServiceListId ensId = ServiceListId(m_currentEnsemble.frequency, m_currentEnsemble.ueid);
    if (!m_currentEnsemble.isValid())
    {  // when we receive data without valid ensemble
        reset();
        return;
    }

    m_model->updateTiiData(data.idList, ensId, m_currentEnsemble.label, 0, m_snr);

    // forcing update of UI
    onSelectionChanged(QItemSelection(), QItemSelection());

    emit ensembleInfoChanged();

    addToPlot(data);
    logTiiData();
}

void TIIBackend::logTiiData() const
{
    if (m_logFile)
    {
        QTextStream out(m_logFile);

        int lastCol = m_exportCoordinates ? (TxTableModel::NumCols - 1) : (TxTableModel::NumColsWithoutCoordinates - 1);

        // Body
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            for (int col = 0; col < lastCol; ++col)
            {
                if (col != TxTableModel::ColNumServices && col != TxTableModel::ColCode)
                {  // num services and code is not logged
                    out << m_model->data(m_model->index(row, col), m_exportRole).toString() << ";";
                }
            }
            out << m_model->data(m_model->index(row, lastCol), m_exportRole).toString() << Qt::endl;
        }
        out.flush();
    }
}

void TIIBackend::onChannelSelection()
{
    onEnsembleInformation(RadioControlEnsemble());
    // clear spectrum plot
    if (m_tiiSpectrumPlot)
    {
        m_tiiSpectrumPlot->replaceBuffer(m_sSpect, {QPointF(0, 0), QPointF(192, 0)});
        m_tiiSpectrumPlot->clear(m_sTii);
    }
    // reset backend
    reset();
}

void TIIBackend::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    if (ens.ueid != m_currentEnsemble.ueid)
    {
        m_model->clear();
        m_currentEnsemble = ens;
        emit ensembleInfoChanged();
    }
}

void TIIBackend::loadSettings()
{
    TIIBackend::onSettingsChanged();
    m_columnProxyModel->setColumns(m_settings->tii.txTable);
    setZoomLevel(m_settings->tii.mapZoom);
    if (m_settings->tii.mapCenter.isValid())
    {
        setMapCenter(m_settings->tii.mapCenter);
    }
    setCenterToCurrentPosition(m_settings->tii.centerMapToCurrentPosition);
}

void TIIBackend::onSettingsChanged()
{
    TxMapBackend::onSettingsChanged();

    if (m_settings->tii.showInactiveTx && m_settings->tii.inactiveTxTimeoutEna)
    {
        if (m_inactiveCleanupTimer == nullptr)
        {
            m_inactiveCleanupTimer = new QTimer(this);
            m_inactiveCleanupTimer->setTimerType(Qt::VeryCoarseTimer);
            m_inactiveCleanupTimer->setInterval(10 * 1000);  // 10 seconds interval
            connect(m_inactiveCleanupTimer, &QTimer::timeout, this, [this]() { m_model->removeInactive(m_settings->tii.inactiveTxTimeout * 60); });
            m_inactiveCleanupTimer->start();
        }
        else
        {
            // do nothing, timer is already running
        }
    }
    else
    {
        if (m_inactiveCleanupTimer)
        {
            m_inactiveCleanupTimer->stop();
            delete m_inactiveCleanupTimer;
            m_inactiveCleanupTimer = nullptr;
        }
    }
    emit ensembleInfoChanged();
    setShowSpetrumPlot(m_settings->tii.showSpectumPlot);
}

void TIIBackend::onTxTableSettingsChanged()
{
    m_columnProxyModel->setColumns(m_settings->tii.txTable);
    emit txTableColChanged();
}

void TIIBackend::onSelectedRowChanged()
{
    m_txInfo.clear();
    if (selectedRow() < 0)
    {  // reset info
        emit txInfoChanged();
        updateTiiPlot();
        return;
    }

    TxTableModelItem item = m_model->data(m_model->index(selectedRow(), 0), TxTableModel::TxTableModelRoles::ItemRole).value<TxTableModelItem>();
    if (item.hasTxData())
    {
        m_txInfo.append(QString("<b>%1</b>").arg(item.transmitterData().location()));
        QGeoCoordinate coord = QGeoCoordinate(item.transmitterData().coordinates().latitude(), item.transmitterData().coordinates().longitude());
        m_txInfo.append(QString(tr("ERP: <b>%1 kW</b>")).arg(static_cast<double>(item.transmitterData().power()), 3, 'f', 1));
        float alt = item.transmitterData().coordinates().altitude();
        if (alt)
        {
            int antHeight = item.transmitterData().antHeight();
            if (antHeight)
            {
                m_txInfo.append(QString(tr("Altitude: <b>%1 m</b> + <b>%2 m</b>")).arg(static_cast<int>(alt)).arg(static_cast<int>(antHeight)));
            }
            else
            {
                m_txInfo.append(QString(tr("Altitude: <b>%1 m</b>")).arg(static_cast<int>(alt)));
            }
        }
        m_txInfo.append(QString("GPS: <b>%1</b>").arg(coord.toString(QGeoCoordinate::DegreesWithHemisphere)));
    }
    emit txInfoChanged();
    updateTiiPlot();
}

void TIIBackend::startStopLog()
{
    if (isRecordingLog() == false)
    {
        const QString tiiPath = AndroidFileHelper::buildSubdirPath(m_settings->dataStoragePath, TII_DIR_NAME);

        // Ensure directory exists and is writable
        if (!AndroidFileHelper::mkpath(m_settings->dataStoragePath, TII_DIR_NAME))
        {
            qCCritical(tii) << "Failed to create TII log directory:" << AndroidFileHelper::lastError();
            return;
        }

        if (!AndroidFileHelper::hasWritePermission(tiiPath))
        {
            qCCritical(tii) << "No permission to write to:" << tiiPath;
            qCCritical(tii) << "Please select a new data storage folder in settings.";
            return;
        }

        QString fileName = QString("%1_TII.csv").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));
        if (m_logFile)
        {
            m_logFile->close();
            delete m_logFile;
        }

        m_logFile = AndroidFileHelper::openFileForWriting(tiiPath, fileName, "text/csv");
        if (m_logFile)
        {
            qCInfo(tii) << "Recording TII log to:" << QString("%1/%2").arg(tiiPath, fileName);

            setIsRecordingLog(true);

            // write header
            QTextStream out(m_logFile);

            // need to keep local copy to avoid changing this settings during logging
            m_exportCoordinates = m_settings->tii.saveCoordinates;
            m_exportRole =
                m_settings->tii.timestampInUTC ? TxTableModel::TxTableModelRoles::ExportRoleUTC : TxTableModel::TxTableModelRoles::ExportRole;

            // Header
            int lastCol = m_exportCoordinates ? (TxTableModel::NumCols - 1) : (TxTableModel::NumColsWithoutCoordinates - 1);
            for (int col = 0; col < lastCol; ++col)
            {
                if (col != TxTableModel::ColNumServices && col != TxTableModel::ColCode)
                {  // num services and code is not logged
                    out << m_model->headerData(col, Qt::Horizontal, m_exportRole).toString() << ";";
                }
            }
            out << m_model->headerData(lastCol, Qt::Horizontal, m_exportRole).toString() << Qt::endl;
        }
        else
        {
            qCCritical(tii) << "Unable to write TII log:" << AndroidFileHelper::lastError();
            setIsRecordingLog(false);
        }
    }
    else
    {
        if (m_logFile)
        {
            m_logFile->close();
            qCInfo(tii) << "TII log finished";
            delete m_logFile;
            m_logFile = nullptr;
        }
        setIsRecordingLog(false);
    }
}

void TIIBackend::registerTiiSpectrumPlot(QQuickItem *item)
{
    if (item == nullptr)
    {
        m_tiiSpectrumPlot = nullptr;
        return;
    }

    LineChartItem *tiiSpectrum = dynamic_cast<LineChartItem *>(item);
    if (tiiSpectrum)
    {
        m_tiiSpectrumPlot = tiiSpectrum;
        m_tiiSpectrumPlot->setDecimationEnabled(false);
        m_tiiSpectrumPlot->setXLabelDecimals(0);
        m_tiiSpectrumPlot->setYLabelDecimals(1);
        m_tiiSpectrumPlot->setXMin(MinX);
        m_tiiSpectrumPlot->setXMax(MaxX);
        m_tiiSpectrumPlot->setYMin(MinY);
        m_tiiSpectrumPlot->setYMax(MaxY);

        m_tiiSpectrumPlot->setXAxisTitle(tr("Carrier pairs"));

        // m_tiiSpectrumPlot->setDefaultYMin(0.0);
        // m_tiiSpectrumPlot->setDefaultYMax(1.0);
        m_sSpect = m_tiiSpectrumPlot->addSeries("spectrum", QColor(0x66bb6a), 1.0);
        m_tiiSpectrumPlot->setSeriesStyle(m_sSpect, (int)LineSeries::FilledWithStroke);
        m_tiiSpectrumPlot->setSeriesBaseline(m_sSpect, 0.0);
        m_tiiSpectrumPlot->setSeriesBrush(m_sSpect, QBrush(QColor(120, 120, 120, 100)));
        m_tiiSpectrumPlot->setSeriesPen(m_sSpect, QPen(Qt::lightGray, 1.0));

        m_sTii = m_tiiSpectrumPlot->addSeries("tii", Qt::cyan, 1.0);
        m_tiiSpectrumPlot->setSeriesStyle(m_sTii, (int)LineSeries::Impulse);
        m_tiiSpectrumPlot->setSeriesWidth(m_sTii, 2.0);  // impulse bar thickness
        m_tiiSpectrumPlot->setSeriesColor(m_sTii, Qt::cyan);

        m_tiiSpectrumPlot->setDefaultXMin(MinX);
        m_tiiSpectrumPlot->setDefaultXMax(MaxX);
        m_tiiSpectrumPlot->setDefaultYMin(MinY);
        m_tiiSpectrumPlot->setDefaultYMax(MaxY);

        for (int n = 24; n < 192; n += 24)
        {
            m_tiiSpectrumPlot->addMarkerLine(true, n, "CarrierPairDivider", QColor(255, 84, 84), 1.0);
        }

        connect(m_tiiSpectrumPlot, &QObject::destroyed, this, [this]() { m_tiiSpectrumPlot = nullptr; });
    }
}

void TIIBackend::setIsActive(bool isActive)
{
    TxMapBackend::setIsActive(isActive);
    if (isActive)
    {
        reset();
    }
    emit setTii(isActive);
}

void TIIBackend::addToPlot(const RadioControlTIIData &data)
{
    float norm = 1.0 / (*std::max_element(data.spectrum.begin(), data.spectrum.end()));

    QVector<QPointF> bins;
    bins.reserve(2 * 192);
    for (int n = 0; n < 2 * 192; ++n)
    {
        double x = n * 0.5;
        double y = data.spectrum.at(n) * norm;
        bins.append(QPointF(x, y));
    }
    // Replace snapshot in one call
    if (m_tiiSpectrumPlot)
    {
        m_tiiSpectrumPlot->replaceBuffer(m_sSpect, bins);

        updateTiiPlot();
    }
}

void TIIBackend::updateTiiPlot()
{
    if (m_tiiSpectrumPlot == nullptr)
    {
        return;
    }

    QVector<QPointF> bins;
    if (selectedRow() < 0)
    {  // draw all
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const auto &item = m_model->itemAt(row);
            if (item.isActive())
            {
                QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
                for (const auto &c : std::as_const(subcar))
                {
                    float val = m_tiiSpectrumPlot->bufferValueAt(m_sSpect, 2 * c);
                    bins.append(QPointF(c, val));
                    val = m_tiiSpectrumPlot->bufferValueAt(m_sSpect, 2 * c + 1);
                    bins.append(QPointF(c + 0.5, val));
                }
            }
        }
        std::sort(bins.begin(), bins.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    }
    else
    {  // draw only selected TII item
        if (selectedRow() < m_model->rowCount())
        {
            const auto &item = m_model->itemAt(selectedRow());
            QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
            for (const auto &c : std::as_const(subcar))
            {
                float val = m_tiiSpectrumPlot->bufferValueAt(m_sSpect, 2 * c);
                bins.append(QPointF(c, val));
                val = m_tiiSpectrumPlot->bufferValueAt(m_sSpect, 2 * c + 1);
                bins.append(QPointF(c + 0.5, val));
            }
        }
    }
    m_tiiSpectrumPlot->replaceBuffer(m_sTii, bins);
}
