#include <QDebug>
#include "clickablelabel.h"

ClickableLabel::ClickableLabel(QWidget *parent)
    : QLabel(parent)
{
  setCursor(Qt::PointingHandCursor);
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *)
{    
    if (nullptr != m_menu)
    {
        m_menu->popup(mapToGlobal(QPoint(0, height())));
    }

    // emit with current state
    emit clicked();
}
