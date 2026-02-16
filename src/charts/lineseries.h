#ifndef LINESERIES_H
#define LINESERIES_H

#include <QColor>
#include <QObject>
#include <limits>

class LineSeries : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int seriesId READ seriesId WRITE setSeriesId NOTIFY seriesIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor NOTIFY fillColorChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(double baselineY READ baselineY WRITE setBaselineY NOTIFY baselineYChanged)
public:
    explicit LineSeries(QObject* parent = nullptr) : QObject(parent) {}

    int seriesId() const { return m_seriesId; }
    void setSeriesId(int id)
    {
        if (m_seriesId == id)
        {
            return;
        }
        m_seriesId = id;
        emit seriesIdChanged();
    }

    const QString& name() const { return m_name; }
    void setName(const QString& n)
    {
        if (m_name == n)
        {
            return;
        }
        m_name = n;
        emit nameChanged();
    }

    QColor color() const { return m_color; }
    void setColor(const QColor& c)
    {
        if (m_color == c)
        {
            return;
        }
        m_color = c;
        emit colorChanged();
    }

    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor& c)
    {
        if (m_fillColor == c)
        {
            return;
        }
        m_fillColor = c;
        emit fillColorChanged();
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

    enum LineStyle
    {
        Normal = 0,
        Impulse = 1,
        Filled = 2,
        FilledWithStroke = 3
    };
    int style() const { return m_style; }
    void setStyle(int st)
    {
        if (m_style == st)
        {
            return;
        }
        m_style = st;
        emit styleChanged();
    }

    double baselineY() const { return m_baselineY; }
    void setBaselineY(double by)
    {
        if (m_baselineY == by)
        {
            return;
        }
        m_baselineY = by;
        emit baselineYChanged();
    }

signals:
    void seriesIdChanged();
    void nameChanged();
    void colorChanged();
    void widthChanged();
    void visibleChanged();
    void styleChanged();
    void baselineYChanged();
    void fillColorChanged();

private:
    int m_seriesId = -1;
    QString m_name;
    QColor m_color = Qt::white;
    QColor m_fillColor = QColor(255, 255, 255, 64);
    qreal m_width = 1.0;
    bool m_visible = true;
    int m_style = Normal;
    double m_baselineY = std::numeric_limits<double>::quiet_NaN();
};

#endif  // LINESERIES_H