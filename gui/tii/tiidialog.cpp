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
#include <QQmlContext>
#include <QBoxLayout>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QMessageBox>
#include <QLoggingCategory>
#include "tiidialog.h"


Q_LOGGING_CATEGORY(tii, "TII", QtInfoMsg)

TIIDialog::TIIDialog(Settings *settings, QWidget *parent)
    : TxMapDialog(settings, true, parent)
{
    // UI
    setWindowTitle(tr("TII Decoder"));
    setMinimumSize(QSize(600, 400));
    resize(QSize(780, 520));

    // Set window flags to add maximize and minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
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
    context->setContextProperty("tiiTableSorted", m_sortedFilteredModel);
    context->setContextProperty("tiiTableSelectionModel", m_tableSelectionModel);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicyContainer(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicyContainer.setVerticalStretch(255);
    container->setSizePolicy(sizePolicyContainer);

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    /*QSplitter * */m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(container);
    m_splitter->addWidget(m_tiiSpectrumPlot);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_splitter);

    if (!m_settings->tii.splitterState.isEmpty()) {
        m_splitter->restoreState(m_settings->tii.splitterState);
    }
#else
    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(container);
#endif

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QCPItemStraightLine *verticalLine;
    for (int n = 1; n < 8; ++n)
    {
        verticalLine = new QCPItemStraightLine(m_tiiSpectrumPlot);
        verticalLine->point1->setCoords(n*24, 0);  // location of point 1 in plot coordinate
        verticalLine->point2->setCoords(n*24, 1);  // location of point 2 in plot coordinate
        verticalLine->setPen(QPen(Qt::red, 1, Qt::SolidLine));
    }

    m_tiiSpectrumPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->xAxis->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->yAxis->grid()->setSubGridVisible(true);

    m_tiiSpectrumPlot->graph(GraphId::Spect)->setLineStyle(QCPGraph::lsLine);
    m_tiiSpectrumPlot->graph(GraphId::TII)->setLineStyle(QCPGraph::lsImpulse);

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    m_tiiSpectrumPlot->xAxis->setTicker(freqTicker);

    freqTicker->setTickStep(8.0); // tick step shall be 8.0
    freqTicker->setScaleStrategy(QCPAxisTickerFixed::ssNone); // and no scaling of the tickstep (like multiples or powers) is allowed

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

    m_tiiSpectrumPlot->setVisible(m_settings->tii.showSpectumPlot);
#endif

    QSize sz = size();
    if (!m_settings->tii.geometry.isEmpty())
    {
        restoreGeometry(m_settings->tii.geometry);
        sz = size();
    }
    QTimer::singleShot(10, this, [this, sz](){ resize(sz); } );
}

TIIDialog::~TIIDialog()
{
    delete m_qmlView;
    delete m_sortedFilteredModel;
}

void TIIDialog::closeEvent(QCloseEvent *event)
{
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    m_settings->tii.splitterState = m_splitter->saveState();
#endif
    m_settings->tii.geometry = saveGeometry();

    QDialog::closeEvent(event);
}

void TIIDialog::reset()
{
    TxMapDialog::reset();

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QList<double> f;
    QList<double> none;
    for (int n = 0; n<192; ++n)
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
    QModelIndexList	selectedList = m_tableSelectionModel->selectedRows();
    if (!selectedList.isEmpty())
    {
        QModelIndex currentIndex = selectedList.at(0);
        id = m_sortedFilteredModel->data(currentIndex, TxTableModel::TxTableModelRoles::IdRole).toInt();
    }
    m_model->updateTiiData(data.idList, ensId);

    if (id >= 0)
    {   // update selection
        QModelIndexList	selectedList = m_tableSelectionModel->selectedRows();
        if (!selectedList.isEmpty())
        {
            QModelIndex currentIndex = selectedList.at(0);
            if (id != m_sortedFilteredModel->data(currentIndex, TxTableModel::TxTableModelRoles::IdRole).toInt())
            {  // selection was updated
                m_tableSelectionModel->clear();
            }
        }
    }

    // forcing update of UI
    onSelectionChanged(QItemSelection(),QItemSelection());

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    addToPlot(data);
#endif
}

void TIIDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
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
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::cyan, 3));

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
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        m_qmlView->setColor(Qt::black);
    }
    else
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
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
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::blue, 3));

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
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        m_qmlView->setColor(Qt::white);
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
    TxMapDialog::onSettingsChanged();

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    m_tiiSpectrumPlot->setVisible(m_settings->tii.showSpectumPlot);
#endif
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
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        updateTiiPlot();
#endif
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
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    updateTiiPlot();
#endif
}

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
void TIIDialog::showPointToolTip(QMouseEvent *event)
{
    int x = qRound(m_tiiSpectrumPlot->xAxis->pixelToCoord(event->pos().x()));
    x = qMin(static_cast<int>(GraphRange::MaxX), x);
    x = qMax(static_cast<int>(GraphRange::MinX), x);
    double y = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(x)->value;

    int subId = x % 24;
    m_tiiSpectrumPlot->setToolTip(QString(tr("Carrier Pair: %1<br>SubId: %3<br>Level: %2")).arg(x).arg(y).arg(subId, 2, 10, QChar('0')));
}

void TIIDialog::addToPlot(const RadioControlTIIData &data)
{
    float norm = 1.0/(*std::max_element(data.spectrum.begin(), data.spectrum.end()));

    QSharedPointer<QCPGraphDataContainer> container = m_tiiSpectrumPlot->graph(GraphId::Spect)->data();
    QCPGraphDataContainer::iterator it = container->begin();
    for (int n = 0; n < 192; ++n)
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
                float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(c)->value;
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
                float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(c)->value;
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
#endif // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
