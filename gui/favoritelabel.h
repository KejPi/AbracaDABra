#ifndef FAVORITELABEL_H
#define FAVORITELABEL_H

#include <QLabel>
#include <QObject>

class FavoriteLabel : public QLabel
{
    Q_OBJECT
public:
    FavoriteLabel(QWidget *parent=0);
    ~FavoriteLabel() {};
    void setActive(bool ena);
signals:
    void toggled(bool);
protected:
    void mouseReleaseEvent(QMouseEvent*);
private:
    bool active = false;
};

#endif // FAVORITELABEL_H
