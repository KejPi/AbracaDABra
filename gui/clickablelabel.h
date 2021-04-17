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
    ~ClickableLabel();
    void setMenu(QMenu * menu) { m_menu = menu; }
    bool setIcon(const QString & file, bool checked = false);
    void setTooltip(const QString & text, bool checked = false);
    void setCheckable(bool checkable) { m_checkable = checkable; m_checked = false; }
    bool isCheckable() const { return m_checkable; }
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
signals:
    void clicked();
    void toggled(bool checked);
protected:
    void mouseReleaseEvent(QMouseEvent*);

private:    
    QMenu * m_menu = nullptr;
    bool m_checkable = false;
    bool m_checked = false;

    QPixmap * m_pic = nullptr;
    QPixmap * m_picChecked = nullptr;

    QString * m_tooltip = nullptr;
    QString * m_tooltipChecked = nullptr;

    void update();
};

#endif // CLICKABLELABEL_H
