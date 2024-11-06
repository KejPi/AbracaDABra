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

#ifndef SCANNINGTOOLDIALOG_H
#define SCANNINGTOOLDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#include "radiocontrol.h"
#include "servicelistid.h"


namespace Ui {
class ScannerDialog;
}

namespace ScannerDialogResult {
enum
{
    Cancelled = 0,
    Interrupted = 1,
    Done = 2
};
}

class Settings;
class ScannerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScannerDialog(Settings *settings, QWidget *parent = nullptr);
    ~ScannerDialog();

    void onTuneDone(uint32_t freq);
    void onSyncStatus(uint8_t sync, float);
    void onEnsembleFound(const RadioControlEnsemble &ens);
    void onServiceFound(const ServiceListId &);
    void onServiceListComplete(const RadioControlEnsemble &);
    void onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &);
    void onTiiData(const RadioControlTIIData & data);
signals:
    void setTii(bool ena, float thr);
    void scanStarts();
    void tuneChannel(uint32_t freq);

private:
    enum class ScannerState
    {
        Idle = 0,
        Init,
        WaitForTune,
        WaitForSync,
        WaitForEnsemble,
        WaitForServices,
        Interrupted
    };

    Ui::ScannerDialog *ui;
    QPushButton * m_buttonStart;
    QPushButton * m_buttonStop;
    QTimer * m_timer = nullptr;

    Settings * m_settings;

    bool m_isScanning = false;
    ScannerState m_state = ScannerState::Idle;

    int m_numEnsemblesFound = 0;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;

    void startScan();
    void scanStep();
    void stopPressed();
    void stopScan(int status);
};

#endif // SCANNINGTOOLDIALOG_H
