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

#ifndef SCANNERDIALOG_H
#define SCANNERDIALOG_H

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QQuickView>
#include <QSpacerItem>
#include <QSpinBox>
#include <QSplitter>
#include <QTableView>

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
    void onSignalState(uint8_t sync, float snr);
    void onEnsembleInformation(const RadioControlEnsemble &ens) override;
    void onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &);
    void onTiiData(const RadioControlTIIData &data) override;
    void onEnsembleConfigurationAndCSV(const QString &config, const QString &csvString);
    void onInputDeviceError(const InputDeviceErrorCode);
    void setupDarkMode(bool darkModeEna) override;
    // void setSelectedRow(int modelRow) override;
signals:
    void scanStarts();
    void scanFinished();
    void tuneChannel(uint32_t freq);
    void requestEnsembleConfiguration();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void onSelectedRowChanged() override;

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

    enum Mode
    {
        Mode_Fast = 1,
        Mode_Normal = 2,
        Mode_Precise = 4
    };

    // UI
    QSplitter *m_splitter;
    QQuickView *m_qmlView;
    QTableView *m_txTableView;
    QProgressBar *m_progressBar;
    QPushButton *m_startStopButton;
    QLabel *m_scanningLabel;
    QLabel *m_progressChannel;
    QPushButton *m_exportButton;
    QPushButton *m_channelListButton;
    QSpinBox *m_numCyclesSpinBox;
    QComboBox *m_modeCombo;

    QTimer *m_timer = nullptr;

    bool m_isScanning = false;
    bool m_isTiiActive = false;
    bool m_exitRequested = false;
    int m_scanCycleCntr;
    ScannerState m_state = ScannerState::Idle;

    uint32_t m_frequency;
    RadioControlEnsemble m_ensemble;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;
    QMap<uint32_t, bool> m_channelSelection;
    int m_numSelectedChannels = 0;
    uint m_tiiCntr = 0;
    float m_snr;
    uint m_snrCntr;
    QDateTime m_scanStartTime;

    // this is used in precise mode
    bool m_isPreciseMode = false;
    RadioControlTIIData m_tiiData;

    void startScan();
    void scanStep();
    void startStopClicked();
    void stopScan();
    void exportClicked();
    void channelSelectionClicked();
    void storeEnsembleData(const RadioControlTIIData &tiiData, const QString &conf, const QString &csvConf);
    void showEnsembleConfig(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);
};

#endif  // SCANNERDIALOG_H
