#include <QDebug>
#include <QPainter>
#include "clickablelabel.h"

ClickableLabel::ClickableLabel(QWidget *parent)
    : QLabel(parent)
{
  setCursor(Qt::PointingHandCursor);
  setScaledContents(false);

  m_pic = new QPixmap(18,18);
  m_pic->fill(Qt::transparent);
  m_picChecked = new QPixmap(18,18);
  m_picChecked->fill(Qt::transparent);
}

ClickableLabel::~ClickableLabel()
{
    delete m_pic;
    delete m_picChecked;
    delete m_tooltip;
    delete m_tooltipChecked;
}

// only mouse click emits signals
void ClickableLabel::mouseReleaseEvent(QMouseEvent *)
{    
    if (m_checkable)
    {  // toggling state
        setChecked(!m_checked);
        emit toggled(m_checked);
    }
    else
    {   // showing menu if set, otherwise only sending signal
        if (nullptr != m_menu)
        {
            m_menu->popup(mapToGlobal(QPoint(0, height())));
        }
        // emit with current state
        emit clicked();
    }
}

bool ClickableLabel::setIcon(const QString & file, bool checked)
{
    QPixmap * pic = new QPixmap();

    if (!pic->load(file))
    {   // do nothing
        qDebug() << Q_FUNC_INFO << "Unable to load icon from file:"<< file;
        delete pic;
        return false;
    }

    // this could be used to calculate transparent picture
//    QPainter p;
//    p.begin(pic);
//    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
//    p.fillRect(pic->rect(), QColor(0, 0, 0, 180));
//    p.end();

    // we have picture loaded here
    if (checked)
    {
        delete m_picChecked;
        m_picChecked = pic;
    }
    else
    {
        delete m_pic;
        m_pic = pic;
    }
    update();

    return true;
}

void ClickableLabel::setTooltip(const QString & text, bool checked)
{
    QString * tooltip = new QString(text);
    if (checked)
    {
        delete m_tooltipChecked;
        m_tooltipChecked = tooltip;
    }
    else
    {
        delete m_tooltip;
        m_tooltip = tooltip;
    }
    update();
}

// this does not emit the signal
void ClickableLabel::setChecked(bool checked)
{
    if (!m_checkable)
    {
        return;
    }

    if (checked != m_checked)
    {  // changing state
        m_checked = checked;
        update();
    }
}

void ClickableLabel::update()
{
    if (m_checked)
    {
        setPixmap(*m_picChecked);
        if (nullptr != m_tooltipChecked)
        {
            QLabel::setToolTip(*m_tooltipChecked);
        }
    }
    else
    {
        setPixmap(*m_pic);
        if (nullptr != m_tooltip)
        {
            QLabel::setToolTip(*m_tooltip);
        }
    }
}
