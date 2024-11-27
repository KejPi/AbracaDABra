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

#ifndef SCANNERDIALOG_H
#define SCANNERDIALOG_H

#include <QDialog>
#include <QQuickView>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QSplitter>
#include <QProgressBar>
#include <QPushButton>
#include <QSpacerItem>
#include <QTableView>
#include <QSpinBox>


#include "radiocontrol.h"
#include "txmapdialog.h"

class Settings;
class ScannerDialog : public TxMapDialog
{
    Q_OBJECT

public:
    explicit ScannerDialog(Settings *settings, QWidget *parent = nullptr);
    ~ScannerDialog();

    void onTuneDone(uint32_t freq);
    void onSyncStatus(uint8_t sync, float snr);
    void onEnsembleInformation(const RadioControlEnsemble &ens) override;
    void onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &);
    void onTiiData(const RadioControlTIIData & data) override;
    void onInputDeviceError(const InputDeviceErrorCode);
    void setupDarkMode(bool darkModeEna) override;
    void setSelectedRow(int modelRow) override;
signals:
    void scanStarts();
    void scanFinished();
    void tuneChannel(uint32_t freq);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    enum class ScannerState
    {
        Idle = 0,
        Init,
        WaitForTune,
        WaitForSync,
        WaitForEnsemble,
        WaitForTII,
        Interrupted
    };

    // UI
    QSplitter * m_splitter;
    QQuickView * m_qmlView ;
    QTableView * m_txTableView;
    QProgressBar * m_progressBar;
    QPushButton * m_startStopButton;
    QLabel * m_scanningLabel;
    QLabel * m_progressChannel;
    QPushButton * m_exportButton;
    QPushButton * m_channelListButton;
    QSpinBox * m_spinBox;

    QTimer * m_timer = nullptr;

    bool m_isScanning = false;
    bool m_isTiiActive = false;
    bool m_exitRequested = false;
    int m_scanCycleCntr;
    ScannerState m_state = ScannerState::Idle;

    RadioControlEnsemble m_ensemble;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;
    QMap<uint32_t, bool> m_channelSelection;
    float m_snr;
    QDateTime m_scanStartTime;

    void startScan();
    void scanStep();
    void startStopClicked();
    void stopScan();
    void exportClicked();
    void channelSelectionClicked();
};

#endif // SCANNERDIALOG_H
