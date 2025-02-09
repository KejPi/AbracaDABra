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

#include "radiocontrol.h"

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
    void updateFIBstatus(int fibCount, int fibErrCount);
    void updateMSCstatus(int crcOkCount, int crcErrCount);
    void resetFibStat();
    void resetMscStat();
    void newFrequency(quint32 f);
    void serviceChanged(const RadioControlServiceComponent &s);
    void onEnsembleInformation(const RadioControlEnsemble &ens) { m_ensembleName = ens.label; }
    void onEnsembleCSV(const QString &csvString);
    QString exportPath() const;
    void setExportPath(const QString &newExportPath);
    void setEnsembleInfoUploaded(bool newEnsembleInfoUploaded);

signals:
    void recordingStart(QWidget *widgetParent);
    void recordingStop();
    void requestEnsembleConfiguration();
    void requestEnsembleCSV();
    void requestUploadCVS();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::EnsembleInfoDialog *ui;

    bool m_isRecordingActive = false;
    quint32 m_frequency;
    QString m_exportPath;
    QString m_ensembleName;
    bool m_ensembleInfoUploaded;

    quint32 m_fibCounter;
    quint32 m_fibErrorCounter;
    quint32 m_crcCounter;
    quint32 m_crcErrorCounter;

    void onRecordingButtonClicked();
    void fibFrameContextMenu(const QPoint &pos);
    void clearServiceInfo();
    void clearSignalInfo();
    void clearFreqInfo();
    void showRecordingStat(bool ena);
};

#endif  // ENSEMBLEINFODIALOG_H
