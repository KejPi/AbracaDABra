#include <QDebug>
#include "catslsdialog.h"
#include "ui_catslsdialog.h"

CatSLSDialog::CatSLSDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CatSLSDialog)
{
    ui->setupUi(this);
}

CatSLSDialog::~CatSLSDialog()
{
    delete ui;
}


void CatSLSDialog::onCategoryUpdate(int catId, const QString & title)
{
    qDebug() << Q_FUNC_INFO << "Category" << title << "["<< catId <<  "] updated";
    emit getCurrentCatSlide(catId);
}

void CatSLSDialog::onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides)
{
    qDebug() << Q_FUNC_INFO << "New slide"<< slide.getContentName() << "category"<< catId << slideIdx+1 << "/" << numSlides;
}
