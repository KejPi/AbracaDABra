#ifndef EPGDIALOG_H
#define EPGDIALOG_H

#include <QDialog>
#include "metadatamanager.h"
#include "slmodel.h"

namespace Ui {
class EPGDialog;
}

class EPGDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged FINAL)

public:
    explicit EPGDialog(SLModel *serviceListModel, MetadataManager *metadataManager, QWidget *parent = nullptr);
    ~EPGDialog();

    bool isVisible() const;
    void setIsVisible(bool newIsVisible);

signals:
    void isVisibleChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::EPGDialog *ui;

    MetadataManager * m_metadataManager;
    SLModel * m_serviceListModel;
    bool m_isVisible;
};

#endif // EPGDIALOG_H
