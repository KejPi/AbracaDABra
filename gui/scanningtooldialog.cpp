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

#include "scanningtooldialog.h"
#include "ui_scanningtooldialog.h"
#include "dabtables.h"

ScanningToolDialog::ScanningToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScanningToolDialog)
{
    ui->setupUi(this);
    setModal(true);

    setSizeGripEnabled(false);

    m_buttonStart = ui->buttonBox->button(QDialogButtonBox::Ok);
    m_buttonStart->setText(tr("Start"));
    connect(m_buttonStart, &QPushButton::clicked, this, &ScanningToolDialog::startScan);

    m_buttonStop = ui->buttonBox->button(QDialogButtonBox::Cancel);
    m_buttonStop->setText(tr("Cancel"));
    m_buttonStop->setDefault(true);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ScanningToolDialog::stopPressed);

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(m_numEnsemblesFound));
    ui->numServicesFoundLabel->setText(QString("%1").arg(m_numServicesFound));
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(DabTables::channelList.size());
    ui->progressBar->setValue(0);
    ui->progressLabel->setText(QString("0 / %1").arg(DabTables::channelList.size()));
    ui->progressChannel->setText(tr("None"));

    ui->progressLabel->setVisible(false);
    ui->progressChannel->setVisible(false);

    // if (!autoStart)
    // {
        ui->scanningLabel->setText(tr("<span style=\"color:red\"><b>Warning:</b> Band scan deletes current service list!</span>"));
    //}

    // setFixedSize( sizeHint() );

    // if (autoStart)
    // {
    //     QTimer::singleShot(1, this, [this](){ startScan(); } );
    // }
}

ScanningToolDialog::~ScanningToolDialog()
{
    if (nullptr != m_timer)
    {
        m_timer->stop();
        delete m_timer;
    }

    delete ui;
}

void ScanningToolDialog::stopPressed()
{
    if (m_isScanning)
    {
        // the state machine has 4 possible states
        // 1. wait for tune (event)
        // 2. wait for sync (timer or event)
        // 4. wait for ensemble (timer or event)
        // 5. wait for services (timer)
        if (m_timer->isActive())
        {   // state 2, 3, 4
            m_timer->stop();
            done(ScanningToolDialogResult::Interrupted);
        }
        // timer not running -> state 1
        m_state = ScanningToolState::Interrupted;  // ==> it will be finished when tune is complete
    }
    else
    {
        done(ScanningToolDialogResult::Cancelled);
    }
}

void ScanningToolDialog::startScan()
{
    m_isScanning = true;

    ui->scanningLabel->setText(tr("Scanning channel:"));
    ui->progressLabel->setVisible(true);
    ui->progressChannel->setVisible(true);

    m_buttonStart->setVisible(false);
    m_buttonStop->setText(tr("Stop"));
    m_buttonStop->setDefault(false);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ScanningToolDialog::scanStep);

    m_state = ScanningToolState::Init;

    // using timer for mainwindow to cleanup and tune to 0 potentially (no timeout in case)
    m_timer->start(2000);
    emit scanStarts();
}

void ScanningToolDialog::scanStep()
{
    if (ScanningToolState::Init == m_state)
    {  // first step
       m_channelIt = DabTables::channelList.constBegin();
    }
    else
    {  // next step
       ++m_channelIt;
    }

    if (DabTables::channelList.constEnd() == m_channelIt)
    {
        // scan finished
        done(ScanningToolDialogResult::Done);
        return;
    }

    ui->progressBar->setValue(ui->progressBar->value()+1);
    ui->progressLabel->setText(QString("%1 / %2")
                               .arg(ui->progressBar->value())
                               .arg(DabTables::channelList.size()));
    ui->progressChannel->setText(m_channelIt.value());
    m_state = ScanningToolState::WaitForTune;
    emit tuneChannel(m_channelIt.key());
}

void ScanningToolDialog::onTuneDone(uint32_t freq)
{
    if (ScanningToolState::Init == m_state)
    {
        if (m_timer->isActive())
        {
            m_timer->stop();
        }
        scanStep();
    }
    else if (ScanningToolState::Interrupted == m_state)
    {   // exit
        done(ScanningToolDialogResult::Interrupted);
    }
    else
    {   // tuned to some frequency -> wait for sync
        m_state = ScanningToolState::WaitForSync;
        m_timer->start(3000);
    }
}

void ScanningToolDialog::onSyncStatus(uint8_t sync, float)
{
    if (DabSyncLevel::NullSync <= DabSyncLevel(sync))
    {
        if (ScanningToolState::WaitForSync == m_state)
        {   // if we are waiting for sync (move to next step)
            m_timer->stop();
            m_state = ScanningToolState::WaitForEnsemble;
            m_timer->start(6000);
        }
    }
}

void ScanningToolDialog::onEnsembleFound(const RadioControlEnsemble & ens)
{
    if (ScanningToolState::Idle == m_state)
    {   // do nothing
        return;
    }

    m_timer->stop();

    ui->numEnsemblesFoundLabel->setText(QString("%1").arg(++m_numEnsemblesFound));

    m_state = ScanningToolState::WaitForServices;

    // this can be interrupted by ensemble complete signal (ensembleConfiguration)
    m_timer->start(8000);

    //qDebug() << m_channelIt.value() << ens.eid() << ens.label;
    qDebug() << QString("---------------------------------------------------------------- %1   %2   '%3'").arg(m_channelIt.value()).arg(ens.ueid, 6, 16, QChar('0')).arg(ens.label);
}

void ScanningToolDialog::onServiceFound(const ServiceListId &)
{
    ui->numServicesFoundLabel->setText(QString("%1").arg(++m_numServicesFound));
}

void ScanningToolDialog::onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &)
{
    ui->numServicesFoundLabel->setText(QString("%1").arg(++m_numServicesFound));
}

void ScanningToolDialog::onServiceListComplete(const RadioControlEnsemble &)
{   // this means that ensemble information is complete => stop timer and do next set
    if ((nullptr != m_timer) && (m_timer->isActive()))
    {
        m_timer->stop();
        scanStep();
    }
}

