#ifndef MARKERLINE_H
#define MARKERLINE_H

#include <QColor>
#include <QObject>
#include <QPen>

class MarkerLine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int lineId READ lineId WRITE setLineId NOTIFY lineIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool isVertical READ isVertical WRITE setIsVertical NOTIFY isVerticalChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)

public:
    explicit MarkerLine(QObject *parent = nullptr) : QObject(parent) {}

    enum LineStyle
    {
        SolidLine = Qt::SolidLine,
        DashLine = Qt::DashLine,
        DotLine = Qt::DotLine,
        DashDotLine = Qt::DashDotLine,
        DashDotDotLine = Qt::DashDotDotLine
    };

    int lineId() const { return m_lineId; }
    void setLineId(int id)
    {
        if (m_lineId == id)
        {
            return;
        }
        m_lineId = id;
        emit lineIdChanged();
    }

    const QString &name() const { return m_name; }
    void setName(const QString &n)
    {
        if (m_name == n)
        {
            return;
        }
        m_name = n;
        emit nameChanged();
    }

    bool isVertical() const { return m_isVertical; }
    void setIsVertical(bool v)
    {
        if (m_isVertical == v)
        {
            return;
        }
        m_isVertical = v;
        emit isVerticalChanged();
    }

    double position() const { return m_position; }
    void setPosition(double p)
    {
        if (m_position == p)
        {
            return;
        }
        m_position = p;
        emit positionChanged();
    }

    QColor color() const { return m_color; }
    void setColor(const QColor &c)
    {
        if (m_color == c)
        {
            return;
        }
        m_color = c;
        emit colorChanged();
    }

    qreal width() const { return m_width; }
    void setWidth(qreal w)
    {
        if (m_width == w)
        {
            return;
        }
        m_width = w;
        emit widthChanged();
    }

    int style() const { return m_style; }
    void setStyle(int s)
    {
        if (m_style == s)
        {
            return;
        }
        m_style = s;
        emit styleChanged();
    }

    bool visible() const { return m_visible; }
    void setVisible(bool v)
    {
        if (m_visible == v)
        {
            return;
        }
        m_visible = v;
        emit visibleChanged();
    }

    QPen pen() const
    {
        QPen p;
        p.setColor(m_color);
        p.setWidthF(m_width);
        p.setStyle(static_cast<Qt::PenStyle>(m_style));
        return p;
    }

    void setPen(const QPen &pen)
    {
        setColor(pen.color());
        setWidth(pen.widthF() > 0 ? pen.widthF() : (double)pen.width());
        setStyle(pen.style());
    }

signals:
    void lineIdChanged();
    void nameChanged();
    void isVerticalChanged();
    void positionChanged();
    void colorChanged();
    void widthChanged();
    void styleChanged();
    void visibleChanged();

private:
    int m_lineId = -1;
    QString m_name;
    bool m_isVertical = true;
    double m_position = 0.0;
    QColor m_color = QColor(255, 255, 0, 180);  // Semi-transparent yellow
    qreal m_width = 1.5;
    int m_style = SolidLine;
    bool m_visible = true;
};

#endif  // MARKERLINE_H