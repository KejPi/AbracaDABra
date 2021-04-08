#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QObject>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(QWidget *parent=0);
signals:
    void clicked();
protected:
    void mouseReleaseEvent(QMouseEvent*);
    QPixmap picture;
};

#endif // CLICKABLELABEL_H
