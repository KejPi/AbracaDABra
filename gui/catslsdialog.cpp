#include <QDebug>
#include "catslsdialog.h"
#include "ui_catslsdialog.h"

CatSLSDialog::CatSLSDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CatSLSDialog)
{
    ui->setupUi(this);

    connect(ui->fwdButton, &QPushButton::clicked, this, &CatSLSDialog::onFwdButtonClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &CatSLSDialog::onBackButtonClicked);
    connect(ui->categoryView, &QListView::clicked, this, &CatSLSDialog::onCategoryViewClicked);

    ui->slideCountLabel->setMinimumWidth(ui->slideCountLabel->fontMetrics().boundingRect("88 / 88").width());
    ui->slsView->setMinimumSize(QSize(322, 242));
    ui->categoryView->setModel(new QStandardItemModel());

    reset();

    resize(minimumSizeHint());
}

CatSLSDialog::~CatSLSDialog()
{
    delete ui;
}

void CatSLSDialog::showEvent(QShowEvent *event)
{
    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr != scene)
    {
        ui->slsView->fitInViewTight(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    //event->accept();
    QDialog::showEvent(event);
}

void CatSLSDialog::resizeEvent(QResizeEvent *event)
{
    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr != scene)
    {
        ui->slsView->fitInViewTight(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
    }

    //event->accept();
    QDialog::resizeEvent(event);
}

void CatSLSDialog::reset()
{
    ui->slideCountLabel->setText("");
    ui->fwdButton->setEnabled(false);
    ui->backButton->setEnabled(false);
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    model->clear();

    QPixmap pic;
    if (pic.load(":/resources/sls_logo.png"))
    {
        QGraphicsScene * scene = ui->slsView->scene();
        if (nullptr == scene)
        {
            scene = new QGraphicsScene(this);
            slsPixmapItem = scene->addPixmap(pic);

            ui->slsView->setScene(scene);
        }
        else
        {
            slsPixmapItem->setPixmap(pic);
        }
        scene->setSceneRect(pic.rect());
        scene->setBackgroundBrush(Qt::white);
        ui->slsView->fitInViewTight(pic.rect(), Qt::KeepAspectRatio);
    }
    else
    {
        qDebug() << "Unable to load :/resources/sls_logo.png";
    }
}

void CatSLSDialog::onCategoryUpdate(int catId, const QString & title)
{
    qDebug() << Q_FUNC_INFO << "Category" << title << "["<< catId <<  "] updated";

    // find category
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    QModelIndexList list = model->match(model->index(0, 0), Qt::UserRole, catId);

    if (title.isEmpty())
    {   // remove category
        if (!list.empty())
        {  // found
            model->removeRow(list.first().row());
        }
        else
        { /* category not found */ }
    }
    else
    {  // add category
        if (list.empty())
        {   // not found - add new category
            QStandardItem * item = new QStandardItem(title);
            item->setData(catId, Qt::UserRole);
            model->appendRow(item);
            model->sort(0);
        }
        else
        {   // found - rename category
            model->itemFromIndex(list.first())->setText(title);
        }
    }

    if (!ui->categoryView->currentIndex().isValid())
    {   // // if index is not valid - select first item
        ui->categoryView->selectionModel()->setCurrentIndex(model->index(0,0), QItemSelectionModel::Clear | QItemSelectionModel::Select | QItemSelectionModel::Current);

        // this triggers request for slide
        onCategoryViewClicked(model->index(0,0));
    }
    else
    { // current index is valid - ask for current slide
        emit getCurrentCatSlide(catId);
    }
}

void CatSLSDialog::onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides)
{
    qDebug() << Q_FUNC_INFO << "New slide"<< slide.getContentName() << "category"<< catId << slideIdx+1 << "/" << numSlides;

    if (ui->categoryView->currentIndex().data(Qt::UserRole).toUInt() != catId)
    {   // do nothing, category is not selected
        return;
    }

    ui->slideCountLabel->setText(QString("%1 / %2").arg(slideIdx+1).arg(numSlides));

    if (numSlides > 1)
    {
        ui->fwdButton->setEnabled(true);
        ui->backButton->setEnabled(true);
    }
    else
    {
        ui->fwdButton->setEnabled(false);
        ui->backButton->setEnabled(false);
    }

    QGraphicsScene * scene = ui->slsView->scene();
    if (nullptr == scene)
    {
        //qDebug() << Q_FUNC_INFO << "New graphisc scene";
        scene = new QGraphicsScene(this);
        slsPixmapItem = scene->addPixmap(slide.getPixmap());

        ui->slsView->setScene(scene);
    }
    else
    {
        slsPixmapItem->setPixmap(slide.getPixmap());
    }

    scene->setSceneRect(slide.getPixmap().rect());
    scene->setBackgroundBrush(Qt::black);
    ui->slsView->fitInViewTight(slide.getPixmap().rect(), Qt::KeepAspectRatio);
}

void CatSLSDialog::onBackButtonClicked()
{
    emit getNextCatSlide(ui->categoryView->currentIndex().data(Qt::UserRole).toUInt(), false);
}


void CatSLSDialog::onFwdButtonClicked()
{
    emit getNextCatSlide(ui->categoryView->currentIndex().data(Qt::UserRole).toUInt(), true);
}


void CatSLSDialog::onCategoryViewClicked(const QModelIndex &index)
{
    QStandardItemModel * model = qobject_cast<QStandardItemModel*>(ui->categoryView->model());
    QStandardItem *item = model->itemFromIndex(index);

    emit getCurrentCatSlide(item->data(Qt::UserRole).toUInt());
}

