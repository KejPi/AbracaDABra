#ifndef CATSLSDIALOG_H
#define CATSLSDIALOG_H

#include <QDialog>
#include "userapplication.h"

namespace Ui {
class CatSLSDialog;
}

class CatSLSDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CatSLSDialog(QWidget *parent = nullptr);
    ~CatSLSDialog();

    void onCategoryUpdate(int catId, const QString & title);
    void onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides);

signals:
    void getCurrentCatSlide(int catId);

private:
    Ui::CatSLSDialog *ui;
};

#endif // CATSLSDIALOG_H
