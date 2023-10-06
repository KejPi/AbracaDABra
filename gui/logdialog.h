#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>
#include <QAbstractItemModel>
#include <QStringListModel>
#include <QFontDatabase>

namespace Ui {
class LogDialog;
}

class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    LogModel(QObject *parent = nullptr) : m_isDarkMode(false), QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex & index, const QVariant & value, int role) override;
    bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex()) override;
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex()) override;
    void setupDarkMode(bool darkModeEna) { m_isDarkMode = darkModeEna; }
    Q_INVOKABLE void appendRow(const QString & rowTxt, int role);

private:
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    struct LogItem
    {
        QString msg;
        QtMsgType type = QtInfoMsg;
    };

    bool m_isDarkMode;
    QList<struct LogItem> m_msgList;
};


class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget *parent = nullptr);
    ~LogDialog();

    QAbstractItemModel * getModel() const;

    void setupDarkMode(bool darkModeEna);

private:
    Ui::LogDialog *ui;
    LogModel * m_dataModel;

    void saveLogToFile();
    void copyToClipboard();
};

#endif // LOGDIALOG_H
