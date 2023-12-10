#ifndef EPGDIALOG_H
#define EPGDIALOG_H

#include <QDialog>
#include "metadatamanager.h"

namespace Ui {
class EPGDialog;
}

class EPGDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EPGDialog(MetadataManager *metadataManager, QWidget *parent = nullptr);
    ~EPGDialog();

private:
    Ui::EPGDialog *ui;

    MetadataManager * m_metadataManager;
};

#endif // EPGDIALOG_H
