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

#ifndef ENSEMBLEINFOBACKEND_H
#define ENSEMBLEINFOBACKEND_H

#include <QAbstractListModel>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QtQmlIntegration>

#include "audiodecoder.h"
#include "ensemblesubchmodel.h"
#include "radiocontrol.h"
#include "uicontrolprovider.h"

class EnsembleInfoModelItem;
class Settings;
// class EnsembleSubchModel;

class EnsembleInfoBackend : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString ensembleConfigurationText READ ensembleConfigurationText NOTIFY ensembleConfigurationTextChanged FINAL)
    Q_PROPERTY(EnsembleSubchModel *ensembleSubchModel READ ensembleSubchModel CONSTANT FINAL)

    UI_PROPERTY_DEFAULT(bool, isRecordingEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isRecordingVisible, false);
    UI_PROPERTY_DEFAULT(bool, isRecordingOngoing, false);
    UI_PROPERTY_DEFAULT(bool, isTimeoutEnabled, false);
    UI_PROPERTY_DEFAULT(int, recordingTimeout, 60);
    UI_PROPERTY_DEFAULT(bool, isCsvExportEnabled, false);
    UI_PROPERTY_DEFAULT(bool, isCsvUploadEnabled, false);
    UI_PROPERTY(QString, recordingSize);
    UI_PROPERTY(QString, recordingLength);

public:
    enum Roles
    {
        LabelRole = Qt::UserRole,
        LabelToolTipRole,
        InfoRole,
        IsVisibleRole,
        GroupRole
    };

    explicit EnsembleInfoBackend(QObject *parent = nullptr);
    ~EnsembleInfoBackend();
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_modelData.count(); }

    void loadSettings(Settings *settings);
    void saveSettings();
    void refreshEnsembleConfiguration(const QString &txt);
    void onServiceComponentsList(const QList<RadioControlServiceComponent> &scList);

    void newFrequency(quint32 f);
    void onSubchannelSelected();
    void serviceChanged(const RadioControlServiceComponent &s);
    void onEnsembleInformation(const RadioControlEnsemble &ens) { m_ensembleName = ens.label; }
    void onEnsembleCSV(const QString &csvString);
    void enableEnsembleInfoUpload();
    void setEnsembleInfoUploaded(bool ensembleInfoUploaded);
    void setServiceInformation(const RadioControlServiceComponent &s);
    void setAudioParameters(const AudioParameters &params);
    void updateSnr(uint8_t, float snr);
    void updateFreqOffset(float offset);
    void updateAgcGain(float gain);
    void updateRfLevel(float rfLevel, float);
    void updatedDecodingStats(const RadioControlDecodingStats &stats);

    void clearServiceInfo();
    void clearSignalInfo();
    void clearFreqInfo();
    void clearSubchInfo();
    void resetFibStat();
    void resetMscStat();

    void enableRecording(bool ena);
    void onRecording(bool isActive);
    void updateRecordingStatus(uint64_t bytes, float ms);
    Q_INVOKABLE void startStopRecording();
    Q_INVOKABLE void saveCSV();
    Q_INVOKABLE void uploadCSV();

    QString ensembleConfigurationText() const;
    void setEnsembleConfigurationText(const QString &ensembleConfigurationText);
    EnsembleSubchModel *ensembleSubchModel() const { return m_ensembleSubchModel; }

signals:
    void ensembleConfigurationTextChanged();
    void recordingStart(int timeoutSec);
    void recordingStop();
    void requestEnsembleConfiguration();
    void requestEnsembleCSV();
    void requestUploadCVS();

private:
    enum LabelId
    {  // WARNING: do NOT chnage the order
       // group 0
        Frequency = 0,
        Channel,
        Snr,
        FreqOffset,
        AgcGain,
        RfLevel,
        // group 1
        Service,
        SId,
        SCSId,
        Subchannel,
        StartCU,
        NumCU,
        // group 2
        ServiceBitrate,
        UsefulBitrate,
        AudioBitrate,
        PadBitrate,
        AudioRatio,
        PadRatio,
        // group 3
        FibCrcErr,
        FibErrRate,
        RsUncorr,
        RsBer,
        AudioCrcErr,
        AudioCrcErrRate,
        // group 4
        AllocatedCU,
        FreeCU,
        AudioCU,
        DataCU,
        // group 5
        SelectedSubchannel,
        SelectedCU,
        SelectedProtection,
        SelectedBitrate,
        // group 6
        SelectedContent,
        SelectedServices0,
        SelectedServices1,
        SelectedServices2,

        NumLabels

    };

    Settings *m_settings = nullptr;
    QList<EnsembleInfoModelItem *> m_modelData;
    quint32 m_frequency;
    bool m_ensembleInfoUploaded = false;
    QString m_ensembleName;

    enum
    {
        StatsHistorySize = 32,                      // N numbers are stored in history to calculate the error rate (must be power of 2)
        StatsIdxMask = (2 * StatsHistorySize - 1),  // (2*N - 1) mask used to wrap index
    };
    int m_fibStatsIdx = 0;
    int m_mscStatsIdx = 0;
    uint16_t *m_fibStats = nullptr;
    uint16_t *m_mscStats = nullptr;
    qint64 m_fibStatsSum = 0;
    qint64 m_fibStatsErrSum = 0;
    quint64 m_fibErrorCounter = 0;
    qint64 m_mscStatsSum = 0;
    qint64 m_mscStatsErrSum = 0;
    quint64 m_crcErrorCounter = 0;
    quint64 m_rsUncorrCounter = 0;

    int m_serviceBitrate = 0;
    float m_serviceBitrateNet = 0;
    QString m_ensembleConfigurationText;
    EnsembleSubchModel *m_ensembleSubchModel = nullptr;
};

class EnsembleInfoModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int group READ group WRITE setGroup NOTIFY groupChanged FINAL)
public:
    EnsembleInfoModel(QObject *parent = nullptr) : QSortFilterProxyModel{parent} {}

    int group() const;
    void setGroup(int group);

signals:
    void groupChanged();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    int m_group = -1;
};

class EnsembleInfoModelItem
{
public:
    EnsembleInfoModelItem(int group, const QString &label, const QString &labelToolTip) : m_group(group), m_label(label), m_labelToolTip(labelToolTip)
    {}

    int group() const { return m_group; }
    bool visible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    QString label() const { return m_label; }
    QString info() const { return m_info; }
    void setInfo(const QString &info) { m_info = info; }
    QString labelToolTip() const { return m_labelToolTip; }

private:
    const int m_group = 0;
    bool m_visible = true;
    const QString m_label;
    const QString m_labelToolTip;
    QString m_info;
};

#endif  // ENSEMBLEINFOBACKEND_H
