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

private:
    Ui::EnsembleInfoDialog *ui;
};

#endif // ENSEMBLEINFODIALOG_H
