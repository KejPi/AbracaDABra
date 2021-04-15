#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QObject>
#include <QMenu>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(QWidget *parent=0);
    void setMenu(QMenu * menu) { m_menu = menu; }
signals:
    void clicked();    
protected:
    void mouseReleaseEvent(QMouseEvent*);

private:
    QMenu * m_menu = nullptr;

};

#endif // CLICKABLELABEL_H
