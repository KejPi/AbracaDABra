#include "favoritelabel.h"

FavoriteLabel::FavoriteLabel(QWidget *parent) : QLabel(parent)
{
    setStyleSheet("color:rgb(127, 127 , 127)");
    setText(QChar(0x2606));
}

void FavoriteLabel::setActive(bool ena)
{
    if (ena)
    {
        setStyleSheet("color:rgb(0,0,0)");
        setText(QChar(0x2605));
    }
    else
    {
        setStyleSheet("color:rgb(127,127,127)");
        setText(QChar(0x2606));
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

