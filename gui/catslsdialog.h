#ifndef CATSLSDIALOG_H
#define CATSLSDIALOG_H

#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QShowEvent>

#include "userapplication.h"

namespace Ui {
class CatSLSDialog;
}

class CatSLSDialog : public QDialog
{
    Q_OBJECT

public:
    //! @ Constructor
    explicit CatSLSDialog(QWidget *parent = nullptr);

    //! @brief Destructor
    ~CatSLSDialog();

    //! @brief Reset UI (on service change)
    void reset();

    //! @brief NSlot for new category, removal of category (title is empty) or update of the category name
    void onCategoryUpdate(int catId, const QString & title);

    //! @brief Slot for new slide to come
    void onCatSlide(const Slide & slide, int catId, int slideIdx, int numSlides);

signals:
    //! @brief Signal to get current slide in category (when user changes category or when category is updated)
    void getCurrentCatSlide(int catId);

    //! @brief Signal to get next slide in category (when user presses back/fwd button)
    void getNextCatSlide(int catId, bool forward);

private:
    //! @brief User interface
    Ui::CatSLSDialog *ui;

private slots:
    //! @brief Slot when slide back button is clicked
    void onBackButtonClicked();

    //! @brief Slot when slide forward button is clicked
    void onFwdButtonClicked();

    //! @brief Slot when soem category is clicked in categoryView
    void onCategoryViewClicked(const QModelIndex &index);
};

#endif // CATSLSDIALOG_H
