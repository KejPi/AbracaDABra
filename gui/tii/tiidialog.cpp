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

#include <QMenu>
#include <QQuickStyle>
#include <QQmlContext>
#include <QBoxLayout>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QMessageBox>
#include "tiidialog.h"

TIIDialog::TIIDialog(const SetupDialog::Settings &settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{        
    m_model = new TiiTableModel(this);
    m_sortModel = new TiiTableSortModel(this);
    m_sortModel->setSourceModel(m_model);
    m_tiiTableSelectionModel = new QItemSelectionModel(m_sortModel, this);

    // UI
    setWindowTitle(tr("TII Decoder"));
    resize(1250, 700);
    setMinimumSize(QSize(780, 520));

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    // TII plot
    m_tiiSpectrumPlot = new QCustomPlot(this);
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_tiiSpectrumPlot->sizePolicy().hasHeightForWidth());
    m_tiiSpectrumPlot->setSizePolicy(sizePolicy);
    m_tiiSpectrumPlot->setMinimumHeight(200);
#endif

    // QML View
    m_qmlView = new QQuickView();
    QQmlContext * context = m_qmlView->rootContext();
    context->setContextProperty("tii", this);
    context->setContextProperty("tiiTable", m_model);
    context->setContextProperty("tiiTableSorted", m_sortModel);
    context->setContextProperty("tiiTableSelectionModel", m_tiiTableSelectionModel);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicyContainer(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicyContainer.setVerticalStretch(255);
    container->setSizePolicy(sizePolicyContainer);

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    QSplitter * splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(container);
    splitter->addWidget(m_tiiSpectrumPlot);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(splitter);
#else
    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(container);
#endif

    connect(m_tiiTableSelectionModel, &QItemSelectionModel::selectionChanged, this, &TIIDialog::onSelectionChanged);
    connect(this, &TIIDialog::selectedRowChanged, this, &TIIDialog::onSelectedRowChanged);

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    m_tiiSpectrumPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->xAxis->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->yAxis->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->xAxis->setLabel(tr("TII Subcarrier [kHz]"));

    m_tiiSpectrumPlot->graph(GraphId::Spect)->setLineStyle(QCPGraph::lsLine);
    m_tiiSpectrumPlot->graph(GraphId::TII)->setLineStyle(QCPGraph::lsImpulse);

    QCPItemStraightLine *verticalLine;
    for (int n = -2; n <= 2; ++n)
    {
        verticalLine = new QCPItemStraightLine(m_tiiSpectrumPlot);
        verticalLine->point1->setCoords(n*384, 0);  // location of point 1 in plot coordinate
        verticalLine->point2->setCoords(n*384, 1);  // location of point 2 in plot coordinate
        verticalLine->setPen(QPen(Qt::red, 1, Qt::DashLine));
    }

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    m_tiiSpectrumPlot->xAxis->setTicker(freqTicker);

    freqTicker->setTickStep(100.0); // tick step shall be 100.0
    freqTicker->setScaleStrategy(QCPAxisTickerFixed::ssNone); // and no scaling of the tickstep (like multiples or powers) is allowed

    m_tiiSpectrumPlot->xAxis->setTicker(freqTicker);
    m_tiiSpectrumPlot->axisRect()->setupFullAxesBox();
    m_tiiSpectrumPlot->xAxis->setRange(GraphRange::MinX, GraphRange::MaxX);
    m_tiiSpectrumPlot->yAxis->setRange(GraphRange::MinY, GraphRange::MaxY);
    m_tiiSpectrumPlot->xAxis2->setRange(GraphRange::MinX, GraphRange::MaxX);
    m_tiiSpectrumPlot->yAxis2->setRange(GraphRange::MinY, GraphRange::MaxY);

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(m_tiiSpectrumPlot, &QCustomPlot::selectionChangedByUser, this, &TIIDialog::onPlotSelectionChanged);
    // connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
    connect(m_tiiSpectrumPlot, &QCustomPlot::mousePress, this, &TIIDialog::onPlotMousePress);
    connect(m_tiiSpectrumPlot, &QCustomPlot::mouseWheel, this, &TIIDialog::onPlotMouseWheel);

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(m_tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), m_tiiSpectrumPlot->xAxis2, QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(m_tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), m_tiiSpectrumPlot->yAxis2, QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(m_tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onXRangeChanged);
    connect(m_tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onYRangeChanged);

    // setup policy and connect slot for context menu popup:
    m_tiiSpectrumPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tiiSpectrumPlot, &QCustomPlot::customContextMenuRequested, this, &TIIDialog::onContextMenuRequest);

    connect(m_tiiSpectrumPlot, &QCustomPlot::mouseMove, this,  &TIIDialog::showPointToolTip);
#endif
}

void TIIDialog::positionUpdated(const QGeoPositionInfo &position)
{
    setCurrentPosition(position.coordinate());
    m_model->setCoordinates(m_currentPosition);
    setPositionValid(true);
}

TIIDialog::~TIIDialog()
{    
    delete m_qmlView;
}

void TIIDialog::showEvent(QShowEvent *event)
{
    reset();
    emit setTii(true, 0.0);
    startLocationUpdate();
    setIsVisible(true);

    QDialog::showEvent(event);
}

void TIIDialog::closeEvent(QCloseEvent *event)
{
    emit setTii(false, 0.0);
    setIsVisible(false);
    stopLocationUpdate();
    QDialog::closeEvent(event);
}

void TIIDialog::reset()
{
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    QList<double> f;
    QList<double> none;
    for (int n = -1024; n<1024; ++n)
    {
        f.append(n);
        none.append(0.0);
    }    
    m_tiiSpectrumPlot->graph(GraphId::Spect)->setData(f, none, true);
    m_tiiSpectrumPlot->graph(GraphId::TII)->setData({0.0}, {-1.0});
    m_tiiSpectrumPlot->rescaleAxes();
    m_tiiSpectrumPlot->deselectAll();
    m_tiiSpectrumPlot->replot();
    m_isZoomed = false;
#endif

    //m_currentEnsemble.frequency = 0;
    m_model->clear();
    setSelectedRow(-1);
}

void TIIDialog::onTiiData(const RadioControlTIIData &data)
{
    ServiceListId ensId = ServiceListId(m_currentEnsemble.frequency, m_currentEnsemble.ueid);
    if (!ensId.isValid())
    {   // when we receive data without valid ensemble
        reset();
        return;
    }

    // handle selection
    int id = -1;
    QModelIndexList	selectedList = m_tiiTableSelectionModel->selectedRows();
    if (!selectedList.isEmpty())
    {
        QModelIndex currentIndex = selectedList.at(0);
        id = m_sortModel->data(currentIndex, TiiTableModel::TiiTableModelRoles::IdRole).toInt();
    }
    m_model->updateData(data.idList, ensId);

    if (id >= 0)
    {   // update selection
        QModelIndexList	selectedList = m_tiiTableSelectionModel->selectedRows();
        if (!selectedList.isEmpty())
        {
            QModelIndex currentIndex = selectedList.at(0);
            if (id != m_sortModel->data(currentIndex, TiiTableModel::TiiTableModelRoles::IdRole).toInt())
            {  // selection was updated
                m_tiiTableSelectionModel->clear();
            }
        }
    }

    // forcing update of UI
    onSelectionChanged(QItemSelection(),QItemSelection());

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    addToPlot(data);
#endif
}

void TIIDialog::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    QModelIndexList selectedRows = m_tiiTableSelectionModel->selectedRows();
    if (selectedRows.isEmpty())
    {   // no selection => return
        setSelectedRow(-1);
        return;
    }

    QModelIndex currentIndex = selectedRows.at(0);
    currentIndex = m_sortModel->mapToSource(currentIndex);
    setSelectedRow(currentIndex.row());
}

void TIIDialog::onSelectedRowChanged()
{
    if (m_selectedRow == -1)
    {
        m_tiiTableSelectionModel->clear();
        return;
    }

    // m_selectedRow is in source model while selection uses indexes of sort model!!!
    QModelIndexList selection = m_tiiTableSelectionModel->selectedRows();
    QModelIndex idx = m_sortModel->mapFromSource(m_model->index(m_selectedRow, 0));
    if (idx.isValid() && (selection.isEmpty() || selection.at(0) != idx))
    {
        m_tiiTableSelectionModel->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }
}

void TIIDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
        m_tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::white, 0));
        m_tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 1, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->setBackground(QBrush(Qt::black));

        m_tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::lightGray, 1));
        m_tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(120, 120, 120, 100)));
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::cyan, 1));

        QColor axisSelectionColor(Qt::red);
        axisSelectionColor = qApp->style()->standardPalette().color(QPalette::Active, QPalette::Link);
        m_tiiSpectrumPlot->xAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->xAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->yAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->yAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedTickLabelColor(axisSelectionColor);

        m_tiiSpectrumPlot->replot();
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    }
    else
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
        m_tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::black, 0));
        m_tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 01, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->setBackground(QBrush(Qt::white));

        m_tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::gray, 1));
        m_tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(80, 80, 80, 100)));
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::blue, 1));

        QColor axisSelectionColor(48, 140, 198);
        axisSelectionColor = qApp->style()->standardPalette().color(QPalette::Active, QPalette::Link);
        m_tiiSpectrumPlot->xAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->xAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->xAxis2->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->yAxis->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis->setSelectedTickLabelColor(axisSelectionColor);
        m_tiiSpectrumPlot->yAxis2->setSelectedBasePen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedSubTickPen(QPen(axisSelectionColor, 1));
        m_tiiSpectrumPlot->yAxis2->setSelectedTickLabelColor(axisSelectionColor);

        m_tiiSpectrumPlot->replot();
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    }    
}

void TIIDialog::startLocationUpdate()
{
    switch (m_settings.tii.locationSource)
    {
    case GeolocationSource::System:
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
            qDebug() << "LocationPermission Undetermined";
            return;
        case Qt::PermissionStatus::Denied:
        {
            qDebug() << "LocationPermission Denied";
            QMessageBox msgBox(QMessageBox::Warning, tr("Warning"), tr("Device location access is denied."), {}, this);
            msgBox.setInformativeText(tr("If you want to display current position on map, grant the location permission in Settings then open the app again."));
            msgBox.exec();
        }
            return;
        case Qt::PermissionStatus::Granted:
            qDebug() << "LocationPermission Granted";
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
            qDebug() << "Start position update";
            connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &TIIDialog::positionUpdated);
            m_geopositionSource->startUpdates();
        }
    }
    break;
    case GeolocationSource::Manual:
        if (m_geopositionSource != nullptr)
        {
            delete m_geopositionSource;
            m_geopositionSource = nullptr;
        }
        positionUpdated(QGeoPositionInfo(m_settings.tii.coordinates, QDateTime::currentDateTime()));
        break;
    case GeolocationSource::SerialPort:
    {
        // serial port
        QVariantMap params;
        params["nmea.source"] = "serial:"+m_settings.tii.serialPort;
        m_geopositionSource = QGeoPositionInfoSource::createSource("nmea", params, this);
        if (m_geopositionSource != nullptr)
        {
            qDebug() << "Start position update";
            connect(m_geopositionSource, &QGeoPositionInfoSource::positionUpdated, this, &TIIDialog::positionUpdated);
            m_geopositionSource->startUpdates();
        }
    }
        break;
    }
}

void TIIDialog::stopLocationUpdate()
{
    if (m_geopositionSource != nullptr)
    {
        m_geopositionSource->stopUpdates();
    }
}

void TIIDialog::onChannelSelection()
{
    onEnsembleInformation(RadioControlEnsemble());
    reset();
}

void TIIDialog::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_currentEnsemble = ens;
    emit ensembleInfoChanged();
}

void TIIDialog::onSettingsChanged()
{
    qDebug() << Q_FUNC_INFO;
    if (m_geopositionSource != nullptr)
    {
        delete m_geopositionSource;
        m_geopositionSource = nullptr;
    }
    if (m_isVisible)
    {
        startLocationUpdate();
    }
}


QGeoCoordinate TIIDialog::currentPosition() const
{
    return m_currentPosition;
}

void TIIDialog::setCurrentPosition(const QGeoCoordinate &newCurrentPosition)
{
    if (m_currentPosition == newCurrentPosition)
        return;
    m_currentPosition = newCurrentPosition;
    emit currentPositionChanged();
}

bool TIIDialog::positionValid() const
{
    return m_positionValid;
}

void TIIDialog::setPositionValid(bool newPositionValid)
{
    if (m_positionValid == newPositionValid)
        return;
    m_positionValid = newPositionValid;
    emit positionValidChanged();
}

bool TIIDialog::isVisible() const
{
    return m_isVisible;
}

void TIIDialog::setIsVisible(bool newIsVisible)
{
    if (m_isVisible == newIsVisible)
        return;
    m_isVisible = newIsVisible;
    emit isVisibleChanged();
}

QStringList TIIDialog::ensembleInfo() const
{
    if (!m_currentEnsemble.isValid())
    {
        return QStringList{"","",""};
    }

    QStringList info;
    info.append(tr("Ensemble: <b>%1</b>").arg(m_currentEnsemble.label));
    info.append(tr("ECC: <b>%1</b> | EID: <b>%2</b>").arg(m_currentEnsemble.ecc(), 2, 16, QChar('0'))
                    .arg(m_currentEnsemble.eid(), 4, 16, QChar('0'))
                    .toUpper());
    info.append(QString(tr("Channel: <b>%1 (%2 kHz)</b>")).arg(DabTables::channelList[m_currentEnsemble.frequency]).arg(m_currentEnsemble.frequency));
    return info;
}

QStringList TIIDialog::txInfo() const
{
    return m_txInfo;
}

int TIIDialog::selectedRow() const
{
    return m_selectedRow;
}

void TIIDialog::setSelectedRow(int modelRow)
{
    if (m_selectedRow == modelRow)
    {
        return;
    }
    m_selectedRow = modelRow;
    emit selectedRowChanged();

    m_txInfo.clear();
    if (modelRow < 0)
    {   // reset info
        emit txInfoChanged();
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
        updateTiiPlot();
#endif
        return;
    }

    TiiTableModelItem item = m_model->data(m_model->index(modelRow, 0), TiiTableModel::TiiTableModelRoles::ItemRole).value<TiiTableModelItem>();
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
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
    updateTiiPlot();
#endif
}

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
void TIIDialog::showPointToolTip(QMouseEvent *event)
{
    int x = qRound(m_tiiSpectrumPlot->xAxis->pixelToCoord(event->pos().x()));
    x = qMin(static_cast<int>(GraphRange::MaxX), x);
    x = qMax(static_cast<int>(GraphRange::MinX), x);
    double y = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(x + 1024)->value;

    if (x > 0 && x <= 2*384)
    {
        //qDebug() << x << (((x-1) % 384) / 2) % 24;
        int subId = (((x-1) % 384) / 2) % 24;
        m_tiiSpectrumPlot->setToolTip(QString(tr("Carrier: %1<br>SubId: %3<br>Level: %2")).arg(x).arg(y).arg(subId, 2, 10, QChar('0')));

    }
    else if (x < 0 && x >= -2*384)
    {
        int subId = (((x+2*384) % 384) / 2) % 24;
        m_tiiSpectrumPlot->setToolTip(QString(tr("Carrier: %1<br>SubId: %3<br>Level: %2")).arg(x).arg(y).arg(subId, 2, 10, QChar('0')));
    }
    else
    {
        m_tiiSpectrumPlot->setToolTip(QString(tr("Carrier: %1<br>Level: %2")).arg(x).arg(y));
    }
}

void TIIDialog::addToPlot(const RadioControlTIIData &data)
{
    float norm = 1.0/(*std::max_element(data.spectrum.begin(), data.spectrum.end()));

    QSharedPointer<QCPGraphDataContainer> container = m_tiiSpectrumPlot->graph(GraphId::Spect)->data();
    QCPGraphDataContainer::iterator it = container->begin();
    for (int n = 1024; n < 2048; ++n)
    {
        (*it++).value = data.spectrum.at(n)*norm;
    }
    for (int n = 0; n < 1024; ++n)
    {
        (*it++).value = data.spectrum.at(n)*norm;
    }
    updateTiiPlot();
}

void TIIDialog::updateTiiPlot()
{
    m_tiiSpectrumPlot->graph(GraphId::TII)->data()->clear();
    if (m_selectedRow < 0)
    {  // draw all
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const auto & item = m_model->itemAt(row);
            QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
            for (const auto & c : subcar)
            {
                float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(c + 1024)->value;
                m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c, val);
            }
        }
    }
    else
    {   // draw only selected TII item
        if (m_selectedRow < m_model->rowCount())
        {
            const auto & item = m_model->itemAt(m_selectedRow);
            QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
            for (const auto & c : subcar)
            {
                float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(c + 1024)->value;
                m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c, val);
            }
        }
    }

    m_tiiSpectrumPlot->replot();
}

void TIIDialog::onPlotSelectionChanged()
{
    // normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
    // the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
    // and the axis base line together.

    // The selection state of the left and right axes shall be synchronized as well as the state of the
    // bottom and top axes.


    // make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis)
        || m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels)
        || m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxisLabel)
        || m_tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spAxis)
        || m_tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        m_tiiSpectrumPlot->xAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        m_tiiSpectrumPlot->xAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels|QCPAxis::spAxisLabel);
    }
    // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        m_tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || m_tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        m_tiiSpectrumPlot->yAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        m_tiiSpectrumPlot->yAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    }
}

void TIIDialog::onPlotMousePress(QMouseEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be dragged
    // if no axis is selected, both directions may be dragged

    if (m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        m_tiiSpectrumPlot->axisRect()->setRangeDrag(m_tiiSpectrumPlot->xAxis->orientation());
    }
    else if (m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        m_tiiSpectrumPlot->axisRect()->setRangeDrag(m_tiiSpectrumPlot->yAxis->orientation());
    }
    else
    {
        m_tiiSpectrumPlot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    }
}

void TIIDialog::onPlotMouseWheel(QWheelEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be zoomed
    // if no axis is selected, both directions may be zoomed
    if (m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        m_tiiSpectrumPlot->axisRect()->setRangeZoom(m_tiiSpectrumPlot->xAxis->orientation());
    }
    else if (m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        m_tiiSpectrumPlot->axisRect()->setRangeZoom(m_tiiSpectrumPlot->yAxis->orientation());
    }
    else
    {
        m_tiiSpectrumPlot->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
    }
    m_isZoomed = true;
}

void TIIDialog::onContextMenuRequest(QPoint pos)
{
    if (m_isZoomed)
    {
        QMenu *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->addAction("Restore default zoom", this, [this]() {
            m_tiiSpectrumPlot->rescaleAxes();
            m_tiiSpectrumPlot->deselectAll();
            m_tiiSpectrumPlot->replot();
            m_isZoomed = false;
        });
        menu->popup(m_tiiSpectrumPlot->mapToGlobal(pos));
    }
}

void TIIDialog::onXRangeChanged(const QCPRange &newRange)
{
    double lowerBound = GraphRange::MinX;
    double upperBound = GraphRange::MaxX;
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        m_tiiSpectrumPlot->xAxis->setRange(fixedRange);
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        m_tiiSpectrumPlot->xAxis->setRange(fixedRange);
    }
}

void TIIDialog::onYRangeChanged(const QCPRange &newRange)
{
    double lowerBound = GraphRange::MinY;
    double upperBound = GraphRange::MaxY;
    QCPRange fixedRange(newRange);
    if (fixedRange.lower < lowerBound)
    {
        fixedRange.lower = lowerBound;
        fixedRange.upper = lowerBound + newRange.size();
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        m_tiiSpectrumPlot->yAxis->setRange(fixedRange);
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        m_tiiSpectrumPlot->yAxis->setRange(fixedRange);
    }
}
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT_ENA
