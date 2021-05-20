#ifndef ENSEMBLEINFODIALOG_H
#define ENSEMBLEINFODIALOG_H

#include <QDialog>
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
    void setEnsStructText(const QString & txt);
    void updateSnr(float snr);
    void updateFreqOffset(float offset);

private:
    Ui::EnsembleInfoDialog *ui;
};

#endif // ENSEMBLEINFODIALOG_H
