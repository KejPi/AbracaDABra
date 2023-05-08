#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>
#include <QAbstractItemModel>
#include <QStringListModel>

namespace Ui {
class LogDialog;
}

class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget *parent = nullptr);
    ~LogDialog();

    QAbstractItemModel * getModel() const;

private:
    Ui::LogDialog *ui;
    QAbstractListModel * m_dataModel;

    void saveLogToFile();
    void copyToClipboard();

};

#endif // LOGDIALOG_H
