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

#include "tiidialog.h"
#include "ui_tiidialog.h"

TIIDialog::TIIDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TIIDialog)
{
    ui->setupUi(this);

    m_model = new TiiTableModel(this);

    ui->tiiTable->setModel(m_model);
    ui->tiiTable->verticalHeader()->hide();
    ui->tiiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tiiTable->horizontalHeader()->setSectionResizeMode(static_cast<int>(TiiTableModel::NumCols)-1, QHeaderView::Stretch);
    ui->tiiTable->horizontalHeader()->setStretchLastSection(true);
    ui->tiiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tiiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    //ui->tiiTable->horizontalHeader()->setMaximumSectionSize(70);
    // ui->tiiTable->setSortingEnabled(false);
    ui->tiiTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

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
    ui->tiiSpectrumPlot->xAxis->setRange(-1024, 1023);
    ui->tiiSpectrumPlot->yAxis->setRange(0, 1);
    ui->tiiSpectrumPlot->xAxis2->setRange(-1024, 1023);
    ui->tiiSpectrumPlot->yAxis2->setRange(0, 1);

    reset();
    setupDarkMode(false);
}

TIIDialog::~TIIDialog()
{
    emit setTii(false, 0.0);

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

    m_model->clear();
}

void TIIDialog::onTiiData(const RadioControlTIIData &data)
{
    qDebug() << "TII:" << data.idList.size();
    for (const auto tii : data.idList)
    {
        qDebug() << tii.main << tii.sub << tii.level;
    }
    m_model->populateModel(data.idList);
    addToPlot(data);
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
    }
}

void TIIDialog::showEvent(QShowEvent *event)
{
    emit setTii(true, 0.0);
    QDialog::showEvent(event);
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
