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

#include <QBoxLayout>
#include <QMenu>
#include <QQmlContext>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && QT_CONFIG(permissions)
#include <QPermissions>
#endif
#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QMessageBox>

#include "tiidialog.h"

Q_LOGGING_CATEGORY(tii, "TII", QtInfoMsg)

TIIDialog::TIIDialog(Settings *settings, QWidget *parent) : TxMapDialog(settings, true, parent)
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
    QQmlContext *context = m_qmlView->rootContext();
    context->setContextProperty("tiiBackend", this);
    context->setContextProperty("tiiTable", m_sortedFilteredModel);
    context->setContextProperty("tiiTableSelectionModel", m_tableSelectionModel);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/map.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QSizePolicy sizePolicyContainer(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
    sizePolicyContainer.setVerticalStretch(255);
    container->setSizePolicy(sizePolicyContainer);

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    /*QSplitter * */ m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(container);
    m_splitter->addWidget(m_tiiSpectrumPlot);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    if (!m_settings->tii.splitterState.isEmpty())
    {
        m_splitter->restoreState(m_settings->tii.splitterState);
    }
#else
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container);
#endif

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QCPItemStraightLine *verticalLine;
    for (int n = 1; n < 8; ++n)
    {
        verticalLine = new QCPItemStraightLine(m_tiiSpectrumPlot);
        verticalLine->point1->setCoords(n * 24, 0);  // location of point 1 in plot coordinate
        verticalLine->point2->setCoords(n * 24, 1);  // location of point 2 in plot coordinate
        m_verticalLineList.append(verticalLine);
    }

    m_tiiSpectrumPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->addGraph();
    m_tiiSpectrumPlot->xAxis->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->xAxis2->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->yAxis->grid()->setSubGridVisible(true);
    m_tiiSpectrumPlot->yAxis2->grid()->setSubGridVisible(true);

    m_tiiSpectrumPlot->graph(GraphId::Spect)->setLineStyle(QCPGraph::lsLine);
    m_tiiSpectrumPlot->graph(GraphId::TII)->setLineStyle(QCPGraph::lsImpulse);

    QSharedPointer<QCPAxisTickerFixed> freqTicker(new QCPAxisTickerFixed);
    m_tiiSpectrumPlot->xAxis->setTicker(freqTicker);
    m_tiiSpectrumPlot->xAxis2->setTicker(freqTicker);

    freqTicker->setTickStep(8.0);                              // tick step shall be 8.0
    freqTicker->setScaleStrategy(QCPAxisTickerFixed::ssNone);  // and no scaling of the tickstep (like multiples or powers) is allowed

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
    connect(m_tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), m_tiiSpectrumPlot->xAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(m_tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), m_tiiSpectrumPlot->yAxis2,
            QOverload<const QCPRange &>::of(&QCPAxis::setRange));
    connect(m_tiiSpectrumPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onXRangeChanged);
    connect(m_tiiSpectrumPlot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &TIIDialog::onYRangeChanged);

    // setup policy and connect slot for context menu popup:
    m_tiiSpectrumPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tiiSpectrumPlot, &QCustomPlot::customContextMenuRequested, this, &TIIDialog::onContextMenuRequest);

    connect(m_tiiSpectrumPlot, &QCustomPlot::mouseMove, this, &TIIDialog::showPointToolTip);

    onSettingsChanged();
#endif

    if (!m_settings->tii.geometry.isEmpty())
    {
        restoreGeometry(m_settings->tii.geometry);
    }
}

TIIDialog::~TIIDialog()
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
    delete m_qmlView;
    delete m_sortedFilteredModel;
}

void TIIDialog::showEvent(QShowEvent *event)
{
    if (!isVisible())
    {
        emit setTii(true);
    }
    TxMapDialog::showEvent(event);
}

void TIIDialog::closeEvent(QCloseEvent *event)
{
    emit setTii(false);
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    m_settings->tii.splitterState = m_splitter->saveState();
#endif
    m_settings->tii.geometry = saveGeometry();

    TxMapDialog::closeEvent(event);
}

void TIIDialog::reset()
{
    TxMapDialog::reset();

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    QList<double> f;
    QList<double> none;
    for (int n = 0; n < 2 * 192; ++n)
    {
        f.append(n / 2.0);
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
    if (!m_currentEnsemble.isValid())
    {  // when we receive data without valid ensemble
        reset();
        return;
    }

    m_model->updateTiiData(data.idList, ensId, m_currentEnsemble.label, 0, m_snr);

    // forcing update of UI
    onSelectionChanged(QItemSelection(), QItemSelection());

    emit ensembleInfoChanged();

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    addToPlot(data);
#endif
    logTiiData();
}

void TIIDialog::logTiiData() const
{
    if (m_logFile)
    {
        QTextStream out(m_logFile);
        // Body
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            for (int col = 0; col < TxTableModel::NumCols - 1; ++col)
            {
                if (col != TxTableModel::ColNumServices)
                {  // num services is not logged
                    out << m_model->data(m_model->index(row, col), TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
                }
            }
            out << m_model->data(m_model->index(row, TxTableModel::NumCols - 1), TxTableModel::TxTableModelRoles::ExportRole).toString() << Qt::endl;
        }
        out.flush();
    }
}

void TIIDialog::setupDarkMode(bool darkModeEna)
{
    if (darkModeEna)
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        m_tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::white, 1));
        m_tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::white);
        m_tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::white);
        if (devicePixelRatio() > 1)
        {
            m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(190, 190, 190), 1, Qt::DotLine));
        }
        else
        {
            m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(Qt::white, 1, Qt::DashLine));
        }
        m_tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(150, 150, 150), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(190, 190, 190), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->setBackground(QBrush(Qt::black));

        m_tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::lightGray, 1));
        m_tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(120, 120, 120, 100)));
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::cyan, 1 + 1));

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
        for (const auto verticalLine : m_verticalLineList)
        {
            verticalLine->setPen(QPen(QColor(255, 84, 84), 1, Qt::SolidLine));
        }

        m_tiiSpectrumPlot->replot();
#endif  // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        m_qmlView->setColor(Qt::black);
    }
    else
    {
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        m_tiiSpectrumPlot->xAxis->setBasePen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis->setBasePen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis2->setBasePen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis2->setBasePen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis->setTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis->setTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis2->setTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis2->setTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis->setSubTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis->setSubTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis2->setSubTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->yAxis2->setSubTickPen(QPen(Qt::black, 1));
        m_tiiSpectrumPlot->xAxis->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->yAxis->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->xAxis2->setTickLabelColor(Qt::black);
        m_tiiSpectrumPlot->yAxis2->setTickLabelColor(Qt::black);
        if (devicePixelRatio() > 1)
        {
            m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
        }
        else
        {
            m_tiiSpectrumPlot->xAxis->grid()->setPen(QPen(Qt::black, 1, Qt::DashLine));
        }
        m_tiiSpectrumPlot->yAxis->grid()->setPen(QPen(QColor(100, 100, 100), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        m_tiiSpectrumPlot->yAxis->grid()->setSubGridPen(QPen(QColor(60, 60, 60), 0, Qt::DotLine));
        m_tiiSpectrumPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
        m_tiiSpectrumPlot->setBackground(QBrush(Qt::white));

        m_tiiSpectrumPlot->graph(GraphId::Spect)->setPen(QPen(Qt::gray, 1));
        m_tiiSpectrumPlot->graph(GraphId::Spect)->setBrush(QBrush(QColor(80, 80, 80, 100)));
        m_tiiSpectrumPlot->graph(GraphId::TII)->setPen(QPen(Qt::blue, 1 + 1));

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
        for (const auto verticalLine : m_verticalLineList)
        {
            verticalLine->setPen(QPen(Qt::red, 1, Qt::SolidLine));
        }

        m_tiiSpectrumPlot->replot();
#endif  // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
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
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    m_tiiSpectrumPlot->setVisible(m_settings->tii.showSpectumPlot);
#endif
}

void TIIDialog::onSelectedRowChanged()
{
    m_txInfo.clear();
    if (selectedRow() < 0)
    {  // reset info
        emit txInfoChanged();
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
        updateTiiPlot();
#endif
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
#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
    updateTiiPlot();
#endif
}

void TIIDialog::startStopLog()
{
    if (isRecordingLog() == false)
    {
        if (!m_settings->tii.logFolder.isEmpty())
        {
            QDir().mkpath(m_settings->tii.logFolder);
            QString filePath = QString("%1/%2_TII.csv").arg(m_settings->tii.logFolder, QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));
            if (m_logFile)
            {
                m_logFile->close();
                delete m_logFile;
            }
            m_logFile = new QFile(filePath);
            if (m_logFile->open(QIODevice::WriteOnly))
            {
                setIsRecordingLog(true);

                // write header
                QTextStream out(m_logFile);

                // Header
                for (int col = 0; col < TxTableModel::NumCols - 1; ++col)
                {
                    if (col != TxTableModel::ColNumServices)
                    {  // num services is not logged
                        out << m_model->headerData(col, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString() << ";";
                    }
                }
                out << m_model->headerData(TxTableModel::NumCols - 1, Qt::Horizontal, TxTableModel::TxTableModelRoles::ExportRole).toString()
                    << Qt::endl;
            }
            else
            {
                delete m_logFile;
                m_logFile = nullptr;
                qCCritical(tii) << "Unable to write TII log:" << filePath;
                setIsRecordingLog(false);
            }
        }
    }
    else
    {
        if (m_logFile)
        {
            m_logFile->close();
            delete m_logFile;
            m_logFile = nullptr;
        }
        setIsRecordingLog(false);
    }
}

#if HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
void TIIDialog::showPointToolTip(QMouseEvent *event)
{
    int x = qRound(m_tiiSpectrumPlot->xAxis->pixelToCoord(event->pos().x()) * 2);
    x = qMin(static_cast<int>(GraphRange::MaxX) * 2 + 1, x);
    x = qMax(static_cast<int>(GraphRange::MinX), x);

    // x is in range 0 ... 383

    double y = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(x)->value;

    int subId = (x % 48) / 2;
    m_tiiSpectrumPlot->setToolTip(QString(tr("Carrier Pair: %1<br>SubId: %3<br>Level: %2")).arg(x / 2).arg(y).arg(subId, 2, 10, QChar('0')));
}

void TIIDialog::addToPlot(const RadioControlTIIData &data)
{
    float norm = 1.0 / (*std::max_element(data.spectrum.begin(), data.spectrum.end()));

    QSharedPointer<QCPGraphDataContainer> container = m_tiiSpectrumPlot->graph(GraphId::Spect)->data();
    QCPGraphDataContainer::iterator it = container->begin();
    for (int n = 0; n < 2 * 192; ++n)
    {
        (*it++).value = data.spectrum.at(n) * norm;
    }
    updateTiiPlot();
}

void TIIDialog::updateTiiPlot()
{
    m_tiiSpectrumPlot->graph(GraphId::TII)->data()->clear();

    if (selectedRow() < 0)
    {  // draw all
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const auto &item = m_model->itemAt(row);
            if (item.isActive())
            {
                QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
                for (const auto &c : subcar)
                {
                    float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(2 * c)->value;
                    m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c, val);
                    val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(2 * c + 1)->value;
                    m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c + 0.5, val);
                }
            }
        }
    }
    else
    {  // draw only selected TII item
        if (selectedRow() < m_model->rowCount())
        {
            const auto &item = m_model->itemAt(selectedRow());
            QList<int> subcar = DabTables::getTiiSubcarriers(item.mainId(), item.subId());
            for (const auto &c : subcar)
            {
                float val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(2 * c)->value;
                m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c, val);
                val = m_tiiSpectrumPlot->graph(GraphId::Spect)->data()->at(2 * c + 1)->value;
                m_tiiSpectrumPlot->graph(GraphId::TII)->addData(c + 0.5, val);
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
    if (m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
        m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        m_tiiSpectrumPlot->xAxis->selectedParts().testFlag(QCPAxis::spAxisLabel) ||
        m_tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
        m_tiiSpectrumPlot->xAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        m_tiiSpectrumPlot->xAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        m_tiiSpectrumPlot->xAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels | QCPAxis::spAxisLabel);
    }
    // make left and right axes be selected synchronously, and handle axis and tick labels as one selectable object:
    if (m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis) ||
        m_tiiSpectrumPlot->yAxis->selectedParts().testFlag(QCPAxis::spTickLabels) ||
        m_tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spAxis) ||
        m_tiiSpectrumPlot->yAxis2->selectedParts().testFlag(QCPAxis::spTickLabels))
    {
        m_tiiSpectrumPlot->yAxis2->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
        m_tiiSpectrumPlot->yAxis->setSelectedParts(QCPAxis::spAxis | QCPAxis::spTickLabels);
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
        m_tiiSpectrumPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
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
        m_tiiSpectrumPlot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    }
    m_isZoomed = true;
}

void TIIDialog::onContextMenuRequest(QPoint pos)
{
    if (m_isZoomed)
    {
        QMenu *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        auto restoreZoomAction = new QAction(tr("Restore default zoom"), this);
        restoreZoomAction->setEnabled(m_isZoomed);
        connect(restoreZoomAction, &QAction::triggered, this,
                [this]()
                {
                    m_tiiSpectrumPlot->rescaleAxes();
                    m_tiiSpectrumPlot->deselectAll();
                    m_tiiSpectrumPlot->replot();
                    m_isZoomed = false;
                });

        connect(menu, &QWidget::destroyed, [=]() { // delete actions
            restoreZoomAction->deleteLater();
        });

        menu->addAction(restoreZoomAction);
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
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        m_tiiSpectrumPlot->xAxis->setRange(fixedRange);
    }
    else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
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
        if (fixedRange.upper > upperBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.upper = upperBound;
        }
        m_tiiSpectrumPlot->yAxis->setRange(fixedRange);
    }
    else if (fixedRange.upper > upperBound)
    {
        fixedRange.upper = upperBound;
        fixedRange.lower = upperBound - newRange.size();
        if (fixedRange.lower < lowerBound || qFuzzyCompare(newRange.size(), upperBound - lowerBound))
        {
            fixedRange.lower = lowerBound;
        }
        m_tiiSpectrumPlot->yAxis->setRange(fixedRange);
    }
}
#endif  // HAVE_QCUSTOMPLOT && TII_SPECTRUM_PLOT
