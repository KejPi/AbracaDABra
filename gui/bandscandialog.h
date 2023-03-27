/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef BANDSCANDIALOG_H
#define BANDSCANDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#include "radiocontrol.h"
#include "servicelistid.h"

namespace Ui {
class BandScanDialog;
}

namespace BandScanDialogResult {
enum
{
    Cancelled = 0,
    Interrupted = 1,
    Done = 2
};
}

enum class BandScanState
{
    Idle = 0,
    Init,
    WaitForTune,
    WaitForSync,
    WaitForEnsemble,
    WaitForServices,
    Interrupted
};

class BandScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BandScanDialog(QWidget *parent = nullptr, bool autoStart = false, Qt::WindowFlags f = Qt::WindowFlags());
    ~BandScanDialog();

    void onTuneDone(uint32_t freq);
    void onSyncStatus(uint8_t sync, float);
    void onEnsembleFound(const RadioControlEnsemble &ens);
    void onServiceFound(const ServiceListId &);
    void onServiceListComplete(const RadioControlEnsemble &);

signals:
    void scanStarts();
    void tuneChannel(uint32_t freq);

private:
    Ui::BandScanDialog *ui;
    QPushButton * m_buttonStart;
    QPushButton * m_buttonStop;
    QTimer * m_timer = nullptr;

    bool m_isScanning = false;
    BandScanState m_state = BandScanState::Idle;

    int m_numEnsemblesFound = 0;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;

    void startScan();
    void scanStep();
    void stopPressed();
};

#endif // BANDSCANDIALOG_H
