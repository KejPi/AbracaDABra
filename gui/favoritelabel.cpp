#include "favoritelabel.h"

FavoriteLabel::FavoriteLabel(QWidget *parent) : QLabel(parent)
{
    // SVG does not work correctly on Win (blurry)
    setScaledContents(false);
    if (!picActive.load(":/resources/star20.png"))
    {
        QPixmap picActive(20,20);
        picActive.fill(Qt::transparent);
    }
    if (!picNoactive.load(":/resources/starEmpty20.png"))
    {
        QPixmap picNoactive(20,20);
        picNoactive.fill(Qt::transparent);
    }
}

void FavoriteLabel::setActive(bool ena)
{
    if (ena)
    {
        setPixmap(picActive);
        setToolTip(QString("Click to remove service from favourites"));
    }
    else
    {
        setPixmap(picNoactive);
        setToolTip(QString("Click to add service to favourites"));
    }    
    active = ena;

}

void FavoriteLabel::mouseReleaseEvent(QMouseEvent*)
{
    // toggle state
    setActive(!active);

    // emit with current state
    emit toggled(active);
}

