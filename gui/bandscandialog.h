#ifndef BANDSCANDIALOG_H
#define BANDSCANDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace Ui {
class BandScanDialog;
}

class BandScanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BandScanDialog(QWidget *parent = nullptr);
    ~BandScanDialog();

private:
    Ui::BandScanDialog *ui;
};

#endif // BANDSCANDIALOG_H
