/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef SCANNERBACKEND_H
#define SCANNERBACKEND_H

#include <QGeoPositionInfoSource>
#include <QItemSelectionModel>
#include <QQmlApplicationEngine>
#include <QQuickView>

#include "radiocontrol.h"
#include "txmapbackend.h"
#include "uicontrolprovider.h"

class Settings;
class ChannelSelectionModel;
class MessageBoxBackend;
class ContextMenuModel;
class ScannerBackend : public TxMapBackend
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ScannerBackend cannot be instantiated")

    UI_PROPERTY_DEFAULT(bool, isScanning, false)
    UI_PROPERTY_DEFAULT(bool, isStartStopEnabled, true)
    UI_PROPERTY(QString, scanningLabel)
    UI_PROPERTY(QString, progressChannel)
    UI_PROPERTY_DEFAULT(int, progressValue, 0)
    UI_PROPERTY_DEFAULT(int, progressMax, 0)

    Q_PROPERTY(ChannelSelectionModel *channelSelectionModel READ channelSelectionModel CONSTANT FINAL)
    Q_PROPERTY(MessageBoxBackend *messageBoxBackend READ messageBoxBackend CONSTANT FINAL)
    Q_PROPERTY(ContextMenuModel *contextMenuModel READ menuModel CONSTANT)
    UI_PROPERTY_SETTINGS(QVariant, splitterState, m_settings->scanner.splitterState)
    UI_PROPERTY_SETTINGS(bool, clearOnStart, m_settings->scanner.clearOnStart)
    UI_PROPERTY_SETTINGS(bool, autoSave, m_settings->scanner.autoSave)
    UI_PROPERTY_SETTINGS(bool, hideLocalTx, m_settings->scanner.hideLocalTx)
    UI_PROPERTY_SETTINGS(int, mode, m_settings->scanner.mode)
    UI_PROPERTY_SETTINGS(int, numCycles, m_settings->scanner.numCycles)
    UI_PROPERTY_SETTINGS(int, txTableSortCol, m_settings->scanner.txTableSortCol)
    UI_PROPERTY_SETTINGS(int, txTableSortOrder, m_settings->scanner.txTableSortOrder)

public:
    explicit ScannerBackend(Settings *settings, QObject *parent = nullptr);
    ~ScannerBackend();

    // methods for menu
    Q_INVOKABLE void clearTableAction();
    Q_INVOKABLE void importAction();
    Q_INVOKABLE void clearLocalTxAction();

    Q_INVOKABLE void startStopAction();
    Q_INVOKABLE QUrl csvPath() const;
    Q_INVOKABLE void saveCSV();
    Q_INVOKABLE void loadCSV(const QUrl &fileUrl);
    Q_INVOKABLE void createContextMenu(int row);
    Q_INVOKABLE void showEnsembleConfig(int row);
    Q_INVOKABLE void saveEnsembleCSV(int srcModelRow);

    void setIsActive(bool isActive);
    void onTuneDone(uint32_t freq);
    void onSignalState(uint8_t sync, float snr);
    void onEnsembleInformation(const RadioControlEnsemble &ens) override;
    void onServiceListEntry(const RadioControlEnsemble &, const RadioControlServiceComponent &);
    void onTiiData(const RadioControlTIIData &data) override;
    void onEnsembleConfigurationAndCSV(const QString &config, const QString &csvString);
    void onInputDeviceError(const InputDevice::ErrorCode);
    // void setSelectedRow(int modelRow) override;
    void setServiceToRestore(const DabSId &sid, uint8_t scids) { m_serviceToRestore = ServiceListId(sid.value(), scids); }
    ServiceListId getServiceToRestore() const { return m_serviceToRestore; };
    void loadSettings();

    ChannelSelectionModel *channelSelectionModel() const { return m_channelSelectionModel; }
    MessageBoxBackend *messageBoxBackend() const { return m_messageBoxBackend; }
    ContextMenuModel *menuModel() const { return m_contextMenuModel; }

signals:
    void scanStarts();
    void scanFinished();
    void tuneChannel(uint32_t freq);
    void requestEnsembleConfiguration();
    void openFileDialog();
    void showEnsembleConfigDialog(int row, const QString &ensConfig);

protected:
    void onSelectedRowChanged() override;

public:
    enum Mode
    {
        Mode_Fast = 1,
        Mode_Normal = 2,
        Mode_Precise = 4
    };
    Q_ENUM(Mode)

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

    enum ContextMenuActionId
    {
        MarkLocal = 0,
        ShowEnsembleInfo,
    };

    QTimer *m_timer = nullptr;
    ChannelSelectionModel *m_channelSelectionModel = nullptr;
    MessageBoxBackend *m_messageBoxBackend = nullptr;

    bool m_isTiiActive = false;
    int m_scanCycleCntr;
    ScannerState m_state = ScannerState::Idle;

    uint32_t m_frequency;
    RadioControlEnsemble m_ensemble;
    int m_numServicesFound = 0;
    dabChannelList_t::ConstIterator m_channelIt;
    int m_numSelectedChannels = 0;
    uint m_tiiCntr = 0;
    float m_snr;
    uint m_snrCntr;
    QDateTime m_scanStartTime;

    // this is used in precise mode
    bool m_isPreciseMode = false;
    RadioControlTIIData m_tiiData;

    ServiceListId m_serviceToRestore;

    void startScan();
    void scanStep();
    void stopScan();
    void saveToFile(const QString &fileName);
    void storeEnsembleData(const RadioControlTIIData &tiiData, const QString &conf, const QString &csvConf);
    void handleContextMenuAction(int actionId, const QVariant &data);
    ContextMenuModel *m_contextMenuModel = nullptr;

    // Real-time CSV auto-save
    QFile *m_autoSaveFile = nullptr;
    int m_autoSaveExportRole = 0;
    void startAutoSaveCsv();
    void appendAutoSaveRows(int firstRow, int lastRow);
    void stopAutoSaveCsv();
};

class ChannelSelectionModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ChannelSelectionModelRoles
    {
        NameRole = Qt::UserRole,
        IsSelectedRole
    };
    explicit ChannelSelectionModel(Settings *settings, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_modelData.count(); }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void setChecked(int row, bool checked);
    Q_INVOKABLE void save() const;
    int numChecked() const;
    bool isChecked(uint32_t freq) const;

private:
    Settings *m_settings;

    struct ItemData
    {
        uint32_t freq;
        bool isSelected;
    };
    QList<ItemData> m_modelData;
};

#endif  // SCANNERBACKEND_H
