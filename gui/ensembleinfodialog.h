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
    void enableDumpToFile(bool ena);
    void dumpToFileStateToggle(bool dumping, int bytesPerSample);
    void updateDumpStatus(ssize_t bytes);
    void updateAgcGain(int gain10);
    void updateFIBstatus(int fibCount, int fibErrCount);

    void newFrequency(quint32 f) { frequency = f; }

signals:
    void dumpToFileStart(const QString & filename);
    void dumpToFileStop();
    void requestEnsembleConfiguration();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::EnsembleInfoDialog *ui;

    bool isDumping = false;
    ssize_t bytesDumped = 0;
    int bytesToTimeShiftFactor = 12;
    quint32 frequency;
    QString dumpPath;

    quint32 fibCounter;
    quint32 fibErrorCounter;

private slots:
    void on_dumpButton_clicked();
    void fibFrameContextMenu(const QPoint &pos);

};

#endif // ENSEMBLEINFODIALOG_H
