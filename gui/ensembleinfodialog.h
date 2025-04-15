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

#ifndef ENSEMBLEINFODIALOG_H
#define ENSEMBLEINFODIALOG_H

#include <QCloseEvent>
#include <QDialog>

#include "audiodecoder.h"
#include "radiocontrol.h"

class Settings;
class QLabel;

namespace Ui
{
class EnsembleInfoDialog;
}

class EnsembleInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnsembleInfoDialog(QWidget *parent = nullptr);
    ~EnsembleInfoDialog();
    void loadSettings(Settings *settings);
    void refreshEnsembleConfiguration(const QString &txt);
    void updateSnr(uint8_t, float snr);
    void updateFreqOffset(float offset);

    const QString &getDumpPath() const;
    void setDumpPath(const QString &newDumpPath);
    void enableRecording(bool ena);
    void onRecording(bool isActive);
    void updateRecordingStatus(uint64_t bytes, float ms);
    void updateAgcGain(float gain);
    void updateRfLevel(float rfLevel, float);
    void updatedDecodingStats(const RadioControlDecodingStats &stats);
    void resetFibStat();
    void resetMscStat();
    void newFrequency(quint32 f);
    void serviceChanged(const RadioControlServiceComponent &s);
    void setServiceInformation(const RadioControlServiceComponent &s);
    void onEnsembleInformation(const RadioControlEnsemble &ens) { m_ensembleName = ens.label; }
    void onEnsembleCSV(const QString &csvString);
    void onServiceComponentsList(const QList<RadioControlServiceComponent> &scList);
    void onSubchannelClicked(int subchId, bool isSelected);
    void enableEnsembleInfoUpload();
    void setEnsembleInfoUploaded(bool newEnsembleInfoUploaded);
    void setAudioParameters(const AudioParameters &params);
signals:
    void recordingStart(QWidget *widgetParent, int timeoutSec);
    void recordingStop();
    void requestEnsembleConfiguration();
    void requestEnsembleCSV();
    void requestUploadCVS();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    struct Subchannel
    {
        int id;       // Subchannel number
        int startCU;  // Starting position (inclusive)
        int numCU;    // Length in number of CU
        DabProtection protection;
        int bitrate;
        enum
        {
            AAC,
            MP2,
            Data
        } content;
        QStringList services;
    };
    QHash<int, Subchannel> m_subchannelMap;
    QLabel *m_subChServicesLabel[2];

    Ui::EnsembleInfoDialog *ui;
    Settings *m_settings = nullptr;

    bool m_isRecordingActive = false;
    quint32 m_frequency;
    QString m_ensembleName;
    bool m_ensembleInfoUploaded;

    quint64 m_fibCounter = 0;
    quint64 m_fibErrorCounter = 0;
    quint64 m_crcCounter = 0;
    quint64 m_crcErrorCounter = 0;

    int m_serviceBitrate = 0;
    float m_serviceBitrateNet = 0;

    void onRecordingButtonClicked();
    void fibFrameContextMenu(const QPoint &pos);
    void clearServiceInfo();
    void clearSignalInfo();
    void clearFreqInfo();
    void clearSubchInfo();
    void showRecordingStat(bool ena);
};

#endif  // ENSEMBLEINFODIALOG_H
