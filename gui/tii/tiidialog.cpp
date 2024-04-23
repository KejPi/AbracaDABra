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

#include "tiidialog.h"
#include "ui_tiidialog.h"

TIIDialog::TIIDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TIIDialog)
{
    ui->setupUi(this);

    m_model = new TiiTableModel(this);
    m_sortModel = new TiiTableSortModel(this);
    m_sortModel->setSourceModel(m_model);    

    m_qmlView = new QQuickView();
    QQmlContext * context = m_qmlView->rootContext();
    context->setContextProperty("tii", this);
    context->setContextProperty("tiiTable", m_model);

    //context->setContextProperty("epgTime", EPGTime::getInstance());

    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicy.setVerticalStretch(255);
    container->setSizePolicy(sizePolicy);

    ui->tiiViewWidget->setMinimumHeight(200);

    QSplitter * splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(container);
    splitter->addWidget(ui->tiiViewWidget);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(splitter);

    ui->tiiTable->setModel(m_sortModel);
    ui->tiiTable->verticalHeader()->hide();
    ui->tiiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tiiTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    ui->tiiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tiiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tiiTable->setSortingEnabled(true);
    ui->tiiTable->sortByColumn(TiiTableModel::ColLevel, Qt::SortOrder::DescendingOrder);
    connect(ui->tiiTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TIIDialog::onSelectionChanged);
    connect(this, &TIIDialog::selectedRowChanged, this, &TIIDialog::onSelectedRowChanged);

    ui->tiiSpectrumPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);
    ui->tiiSpectrumPlot->addGraph();
    ui->tiiSpectrumPlot->addGraph();
    ui->tiiSpectrumPlot->addGraph();
    ui->tiiSpectrumPlot->xAxis->grid()->setSubGridVisible(true);
    ui->tiiSpectrumPlot->yAxis->grid()->setSubGridVisible(true);
    ui->tiiSpectrumPlot->xAxis->setLabel(tr("TII Subcarrier [kHz]"));

    ui->tiiSpectrumPlot->graph(GraphId::Spect)->setLineStyle(QCPGraph::lsLine);
    ui->tiiSpectrumPlot->graph(GraphId::TII)->setLineStyle(QCPGraph::lsImpulse);
    ui->tiiSpectrumPlot->graph(GraphId::Thr)->setLineStyle(QCPGraph::lsLine);

    QCPItemStraightLine *verticalLine;
    for (int n = -2; n <= 2; ++n)
    {
        verticalLine = new QCPItemStraightLine(ui->tiiSpectrumPlot);
        verticalLine->point1->setCoords(n*384, 0);  // location of point 1 in plot coordinate
        verticalLine->point2->setCoords(n*384, 1);  // location of point 2 in plot coordinate
        verticalLine->setPen(QPen(Qt::red, 1, Qt::DashLine));
    }

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    ui->tiiSpectrumPlot->xAxis->setTicker(freqTicker);

    freqTicker->setTickStep(100.0); // tick step shall be 100.0
    freqTicker->setScaleStrategy(QCPAxisTickerFixed::ssNone); // and no scaling of the tickstep (like multiples or powers) is allowed

    ui->tiiSpectrumPlot->xAxis->setTicker(freqTicker);
    ui->tiiSpectrumPlot->axisRect()->setupFullAxesBox();
    ui->tiiSpectrumPlot->xAxis->setRange(GraphRange::MinX, GraphRange::MaxX);
    ui->tiiSpectrumPlot->yAxis->setRange(GraphRange::MinY, GraphRange::MaxY);
    ui->tiiSpectrumPlot->xAxis2->setRange(GraphRange::MinX, GraphRange::MaxX);
    ui->tiiSpectrumPlot->yAxis2->setRange(GraphRange::MinY, GraphRange::MaxY);

    // connect slot that ties some axis selections together (especially opposite axes):
    connect(ui->tiiSpectrumPlot, &QCustomPlot::selectionChangedByUser, this, &TIIDialog::onPlotSelectionChanged);
    // connect slots that takes care that when an axis is selected, only that direction can be dragged and zoomed:
    connect(ui->tiiSpectrumPlot, &QCustomPlot::mousePress, this, &TIIDialog::onPlotMousePress);
    connect(ui->tiiSpectrumPlot, &QCustomPlot::mouseWheel, this, &TIIDialog::onPlotMouseWheel);

    // make bottom and left axes transfer their ranges to top and right axes:
    connect(ui->tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->tiiSpectrumPlot->xAxis2, QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), ui->tiiSpectrumPlot->yAxis2, QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(ui->tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onXRangeChanged);
    connect(ui->tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onYRangeChanged);

    // setup policy and connect slot for context menu popup:
    ui->tiiSpectrumPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tiiSpectrumPlot, &QCustomPlot::customContextMenuRequested, this, &TIIDialog::onContextMenuRequest);

    // TODO: it shows tooltip outside the plot - why?
    //connect(ui->tiiSpectrumPlot, &QCustomPlot::mouseMove, this,  &TIIDialog::showPointToolTip);
}

void TIIDialog::showPointToolTip(QMouseEvent *event)
{
    int x = qRound(ui->tiiSpectrumPlot->xAxis->pixelToCoord(event->pos().x()));
    x = qMin(static_cast<int>(GraphRange::MaxX), x);
    x = qMax(static_cast<int>(GraphRange::MinX), x);
    double y = ui->tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(x + 1024)->value;

    setToolTip(QString("%1, %2").arg(x).arg(y));
}

void TIIDialog::positionUpdated(const QGeoPositionInfo &position)
{
    qDebug() << Q_FUNC_INFO << position.coordinate();
    setCurrentPosition(position.coordinate());
    m_model->setCoordinates(m_currentPosition);
    setPositionValid(true);
}

TIIDialog::~TIIDialog()
{    
    delete m_qmlView;
    delete ui;    
}

void TIIDialog::reset()
{
    QList<double> f;
    QList<double> none;
    for (int n = -1024; n<1024; ++n)
    {
        f.append(n);
        none.append(-1.0);
    }
    ui->tiiSpectrumPlot->graph(GraphId::Spect)->setData(f, none, true);
    ui->tiiSpectrumPlot->graph(GraphId::Thr)->setData({-1024.0, 1023.0}, {-1.0, -1.0}, true);
    ui->tiiSpectrumPlot->graph(GraphId::TII)->setData({0.0}, {-1.0});
    ui->tiiSpectrumPlot->rescaleAxes();
    ui->tiiSpectrumPlot->deselectAll();
    ui->tiiSpectrumPlot->replot();
    m_isZoomed = false;

    //m_currentEnsemble.frequency = 0;
    m_model->clear();
    setSelectedRow(-1);
}

void TIIDialog::onTiiData(const RadioControlTIIData &data)
{
    ServiceListId ensId = ServiceListId(m_currentEnsemble.frequency, m_currentEnsemble.ueid);
    // handle selection

    int id = -1;
    if (ui->tiiTable->selectionModel()->hasSelection())
    {
        QModelIndexList	selectedList = ui->tiiTable->selectionModel()->selectedRows();
        QModelIndex currentIndex = selectedList.at(0);
        id = ui->tiiTable->model()->data(currentIndex, TiiTableModel::TiiTableModelRoles::IdRole).toInt();
    }
    m_model->updateData(data.idList, ensId);

    if (id >= 0)
    {   // update selection
        QModelIndexList	selectedList = ui->tiiTable->selectionModel()->selectedRows();
        if (!selectedList.isEmpty())
        {
            QModelIndex currentIndex = selectedList.at(0);
            if (id != ui->tiiTable->model()->data(currentIndex, TiiTableModel::TiiTableModelRoles::IdRole).toInt())
            {  // selection was updated
                ui->tiiTable->selectionModel()->clear();
            }
        }
    }

    addToPlot(data);
}

void TIIDialog::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList	selectedList = selected.indexes();
    //qDebug() << Q_FUNC_INFO << m_selectedRow << selectedList;
    if (selectedList.isEmpty()) {
        // no selection => return
        setSelectedRow(-1);
        return;
    }

    QModelIndex currentIndex = selectedList.at(0);
    currentIndex = m_sortModel->mapToSource(currentIndex);
    setSelectedRow(currentIndex.row());
}

void TIIDialog::onSelectedRowChanged()
{
    if (m_selectedRow == -1)
    {
        ui->tiiTable->selectionModel()->clear();
        return;
    }

    // m_selectedRow is in source model while selection uses indexes of sort model!!!
    QModelIndexList selection = ui->tiiTable->selectionModel()->selectedRows();
    QModelIndex idx = m_sortModel->mapFromSource(m_model->index(m_selectedRow, 0));
    if (idx.isValid() && (selection.isEmpty() || selection.at(0) != idx))
    {
        ui->tiiTable->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }
}

void TIIDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
        ui->tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::white, 0));
        ui->tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::white);
        ui->tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::white);
        ui->tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::white);
        ui->tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::white);
        ui->tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 1, Qt::DotLine));
        ui->tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->tiiSpectrumPlot->setBackground(QBrush(Qt::black));

        ui->tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::cyan, 1));
        ui->tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(0, 255, 255, 100)));

        ui->tiiSpectrumPlot->graph(GraphId::Thr)->setPen(QPen(QColor(Qt::red), 1, Qt::DashLine));
        ui->tiiSpectrumPlot->graph(GraphId::Thr)->setBrush(QBrush(QColor(255, 0, 0, 100)));

        ui->tiiSpectrumPlot->replot();
    }
    else
    {
        ui->tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::black, 0));
        ui->tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::black);
        ui->tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::black);
        ui->tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::black);
        ui->tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::black);
        ui->tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 01, Qt::DotLine));
        ui->tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        ui->tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        ui->tiiSpectrumPlot->setBackground(QBrush(Qt::white));

        ui->tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::gray, 1));
        ui->tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(80, 80, 80, 100)));
        ui->tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::blue, 1));
        //ui->tiiSpectrumPlot->graph(GraphId::TII)->setBrush(QBrush(QColor(0, 0, 255, 100)));
        ui->tiiSpectrumPlot->graph(GraphId::Thr)->setPen(QPen(QColor(Qt::gray), 0, Qt::SolidLine));
        ui->tiiSpectrumPlot->graph(GraphId::Thr)->setBrush(QBrush(QColor(128, 128, 128, 100)));

        ui->tiiSpectrumPlot->replot();
    }    
}

void TIIDialog::startLocationUpdate()
{
    // ask for permission
#if QT_CONFIG(permissions)
    QLocationPermission locationsPermission;
    locationsPermission.setAccuracy(QLocationPermission::Precise);
    locationsPermission.setAvailability(QLocationPermission::Always);
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
    // start location update
    QGeoPositionInfoSource *source = QGeoPositionInfoSource::createDefaultSource(this);
    if (source != nullptr)
    {
        qDebug() << "Start upadate";
        connect(source, &QGeoPositionInfoSource::positionUpdated, this, &TIIDialog::positionUpdated);
        source->startUpdates();
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

void TIIDialog::showEvent(QShowEvent *event)
{
    reset();
    emit setTii(true, 0.0);
    startLocationUpdate();
    setIsVisible(true);

    // calculate size of the table
    int hSize = 0;
    for (int n = 0; n < ui->tiiTable->horizontalHeader()->count(); ++n) {
        hSize += ui->tiiTable->horizontalHeader()->sectionSize(n);
    }
    ui->tiiTable->setMinimumWidth(hSize + ui->tiiTable->verticalScrollBar()->sizeHint().width() + TiiTableModel::NumCols);
    ui->tiiTable->horizontalHeader()->setStretchLastSection(true);

    QDialog::showEvent(event);
}

void TIIDialog::closeEvent(QCloseEvent *event)
{
    emit setTii(false, 0.0);
    setIsVisible(false);
    QDialog::closeEvent(event);
}


void TIIDialog::addToPlot(const RadioControlTIIData &data)
{
    float norm = 1.0/(*std::max_element(data.spectrum.begin(), data.spectrum.end()));

    QSharedPointer<QCPGraphDataContainer> container = ui->tiiSpectrumPlot->graph(GraphId::Spect)->data();
    QCPGraphDataContainer::iterator it = container->begin();
    for (int n = 1024; n < 2048; ++n)
    {
        (*it++).value = data.spectrum.at(n)*norm;
    }
    for (int n = 0; n < 1024; ++n)
    {
        (*it++).value = data.spectrum.at(n)*norm;
    }
    ui->tiiSpectrumPlot->graph(GraphId::TII)->data()->clear();
    for (const auto & tii : data.idList)
    {
        QList<int> subcar = DabTables::getTiiSubcarriers(tii.main, tii.sub);
        for (const auto & c : subcar)
        {
            float val = ui->tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(c + 1024)->value;
            ui->tiiSpectrumPlot->graph(GraphId::TII)->addData(c, val);
        }
    }
    ui->tiiSpectrumPlot->graph(GraphId::Thr)->setData({-1024.0, 1023.0}, {data.thr, data.thr}, true);
    ui->tiiSpectrumPlot->replot();
}

void TIIDialog::onPlotSelectionChanged()
{
    // normally, axis base line, axis tick labels and axis labels are selectable separately, but we want
    // the user only to be able to select the axis as a whole, so we tie the selected states of the tick labels
    // and the axis base line together.

    // The selection state of the left and right axes shall be synchronized as well as the state of the
    // bottom and top axes.


    // make top and bottom axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (ui->tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        ui->tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        ui->tiiSpectrumPlot->xAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        ui->tiiSpectrumPlot->xAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    }
    // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (ui->tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis) || ui->tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        ui->tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) || ui->tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        ui->tiiSpectrumPlot->yAxis2->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        ui->tiiSpectrumPlot->yAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
    }
}

void TIIDialog::onPlotMousePress(QMouseEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be dragged
    // if no axis is selected, both directions may be dragged

    if (ui->tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeDrag(ui->tiiSpectrumPlot->xAxis->orientation());
    }
    else if (ui->tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeDrag(ui->tiiSpectrumPlot->yAxis->orientation());
    }
    else
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeDrag(Qt::Horizontal|Qt::Vertical);
    }
}

void TIIDialog::onPlotMouseWheel(QWheelEvent *event)
{
    // if an axis is selected, only allow the direction of that axis to be zoomed
    // if no axis is selected, both directions may be zoomed
    if (ui->tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeZoom(ui->tiiSpectrumPlot->xAxis->orientation());
    }
    else if (ui->tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeZoom(ui->tiiSpectrumPlot->yAxis->orientation());
    }
    else
    {
        ui->tiiSpectrumPlot->axisRect()->setRangeZoom(Qt::Horizontal|Qt::Vertical);
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
            ui->tiiSpectrumPlot->rescaleAxes();
            ui->tiiSpectrumPlot->deselectAll();
            ui->tiiSpectrumPlot->replot();
            m_isZoomed = false;
        });
        menu->popup(ui->tiiSpectrumPlot->mapToGlobal(pos));
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
        ui->tiiSpectrumPlot->xAxis->setRange(fixedRange);
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        ui->tiiSpectrumPlot->xAxis->setRange(fixedRange);
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
        ui->tiiSpectrumPlot->yAxis->setRange(fixedRange);
    } else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound-lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        ui->tiiSpectrumPlot->yAxis->setRange(fixedRange);
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
    {   // reste info
        emit txInfoChanged();
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
}
