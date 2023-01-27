#ifndef ENSEMBLEINFODIALOG_H
#define ENSEMBLEINFODIALOG_H

#include <QDialog>
#include <QCloseEvent>
#include "radiocontrol.h"

namespace Ui {
class EnsembleInfoDialog;
}

class EnsembleInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnsembleInfoDialog(QWidget *parent = nullptr);
    ~EnsembleInfoDialog();
    void refreshEnsembleConfiguration(const QString & txt);
    void updateSnr(float snr);
    void updateFreqOffset(float offset);

    const QString &getDumpPath() const;
    void setDumpPath(const QString &newDumpPath);
    void enableRecording(bool ena);
    void onRecording(bool isActive);
    void updateRecordingStatus(uint64_t bytes, float ms);
    void updateAgcGain(float gain);
    void updateFIBstatus(int fibCount, int fibErrCount);
    void updateMSCstatus(int crcOkCount, int crcErrCount);
    void resetFibStat();
    void resetMscStat();
    void newFrequency(quint32 f);
    void serviceChanged(const RadioControlServiceComponent &s);

signals:
    //void recordingStart(const QString & filename);
    void recordingStart(QWidget * widgetParent);
    void recordingStop();
    void requestEnsembleConfiguration();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::EnsembleInfoDialog *ui;

    bool m_isRecordingActive = false;
    quint32 m_frequency;
    QString m_recordingPath;

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

#endif // ENSEMBLEINFODIALOG_H
