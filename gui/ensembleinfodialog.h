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

    void enableDumpToFile(bool ena);
    void dumpToFileStateToggle(bool dumping);

signals:
    void dumpToFileStart(const QString & filename);
    void dumpToFileStop();
    void requestEnsembleConfiguration();

protected:
    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    Ui::EnsembleInfoDialog *ui;

    bool isDumping = false;

private slots:
    void on_dumpButton_clicked();

};

#endif // ENSEMBLEINFODIALOG_H
