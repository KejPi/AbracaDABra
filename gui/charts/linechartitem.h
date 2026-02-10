#ifndef LINECHARTITEM_H
#define LINECHARTITEM_H

#include <qqmlintegration.h>

#include <QBrush>
#include <QColor>
#include <QPen>
#include <QQmlListProperty>
#include <QQuickItem>
#include <QSGNode>

#include "lineseries.h"
#include "markerline.h"

class QSGGeometryNode;
class QSGClipNode;

/**
 * \class LineChartItem
 * \brief High-performance Qt Quick line chart with scene graph rendering
 */
class LineChartItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double xMin READ xMin WRITE setXMin NOTIFY rangeChanged)
    Q_PROPERTY(double xMax READ xMax WRITE setXMax NOTIFY rangeChanged)
    Q_PROPERTY(double yMin READ yMin WRITE setYMin NOTIFY rangeChanged)
    Q_PROPERTY(double yMax READ yMax WRITE setYMax NOTIFY rangeChanged)
    Q_PROPERTY(int historyCapacity READ historyCapacity WRITE setHistoryCapacity NOTIFY historyCapacityChanged)
    Q_PROPERTY(bool decimationEnabled READ decimationEnabled WRITE setDecimationEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(qreal targetPixelsPerPoint READ targetPixelsPerPoint WRITE setTargetPixelsPerPoint NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridMajorColor READ gridMajorColor WRITE setGridMajorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridMinorColor READ gridMinorColor WRITE setGridMinorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisLineColor READ axisLineColor WRITE setAxisLineColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisTickColor READ axisTickColor WRITE setAxisTickColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor labelTextColor READ labelTextColor WRITE setLabelTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisSelectionColor READ axisSelectionColor WRITE setAxisSelectionColor NOTIFY axisSelectionColorChanged FINAL)
    Q_PROPERTY(int plotLeftMargin READ plotLeftMargin WRITE setPlotLeftMargin NOTIFY appearanceChanged)
    Q_PROPERTY(int plotBottomMargin READ plotBottomMargin WRITE setPlotBottomMargin NOTIFY appearanceChanged)
    Q_PROPERTY(int plotTopMargin READ plotTopMargin WRITE setPlotTopMargin NOTIFY appearanceChanged)
    Q_PROPERTY(int plotRightMargin READ plotRightMargin WRITE setPlotRightMargin NOTIFY appearanceChanged)
    Q_PROPERTY(QString xAxisTitle READ xAxisTitle WRITE setXAxisTitle NOTIFY xAxisTitleChanged)
    Q_PROPERTY(QString yAxisTitle READ yAxisTitle WRITE setYAxisTitle NOTIFY yAxisTitleChanged)
    Q_PROPERTY(double majorTickStepX READ majorTickStepX WRITE setMajorTickStepX NOTIFY appearanceChanged)
    Q_PROPERTY(double minorTickStepX READ minorTickStepX WRITE setMinorTickStepX NOTIFY appearanceChanged)
    Q_PROPERTY(double majorTickStepY READ majorTickStepY WRITE setMajorTickStepY NOTIFY appearanceChanged)
    Q_PROPERTY(double minorTickStepY READ minorTickStepY WRITE setMinorTickStepY NOTIFY appearanceChanged)
    Q_PROPERTY(int minorSectionsX READ minorSectionsX WRITE setMinorSectionsX NOTIFY appearanceChanged)
    Q_PROPERTY(int minorSectionsY READ minorSectionsY WRITE setMinorSectionsY NOTIFY appearanceChanged)
    Q_PROPERTY(int xLabelDecimals READ xLabelDecimals WRITE setXLabelDecimals NOTIFY appearanceChanged)
    Q_PROPERTY(int yLabelDecimals READ yLabelDecimals WRITE setYLabelDecimals NOTIFY appearanceChanged)
    Q_PROPERTY(double minXSpan READ minXSpan WRITE setMinXSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double maxXSpan READ maxXSpan WRITE setMaxXSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double minYSpan READ minYSpan WRITE setMinYSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double maxYSpan READ maxYSpan WRITE setMaxYSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultXSpan READ defaultXSpan WRITE setDefaultXSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultYSpan READ defaultYSpan WRITE setDefaultYSpan NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultXMin READ defaultXMin WRITE setDefaultXMin NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultXMax READ defaultXMax WRITE setDefaultXMax NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultYMin READ defaultYMin WRITE setDefaultYMin NOTIFY appearanceChanged)
    Q_PROPERTY(double defaultYMax READ defaultYMax WRITE setDefaultYMax NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledXTicks READ labeledXTicksVariant WRITE setLabeledXTicksVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledYTicks READ labeledYTicksVariant WRITE setLabeledYTicksVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledXMinorTicks READ labeledXMinorTicksVariant WRITE setLabeledXMinorTicksVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledYMinorTicks READ labeledYMinorTicksVariant WRITE setLabeledYMinorTicksVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledXTickLabels READ labeledXTickLabelsVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList labeledYTickLabels READ labeledYTickLabelsVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList xLabelPx READ xLabelPxVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList yLabelPx READ yLabelPxVariant NOTIFY appearanceChanged)
    Q_PROPERTY(QQmlListProperty<LineSeries> series READ series)
    Q_PROPERTY(double baselineY READ baselineY WRITE setBaselineY NOTIFY appearanceChanged)
    Q_PROPERTY(bool resetActive READ resetActive NOTIFY resetActiveChanged)
    Q_PROPERTY(bool autoRangeX READ autoRangeX WRITE setAutoRangeX NOTIFY autoRangeXChanged)
    Q_PROPERTY(bool autoRangeY READ autoRangeY WRITE setAutoRangeY NOTIFY autoRangeYChanged)
    Q_PROPERTY(bool userModifiedX READ userModifiedX NOTIFY userModifiedXChanged)
    Q_PROPERTY(bool userModifiedY READ userModifiedY NOTIFY userModifiedYChanged)
    Q_PROPERTY(bool followTail READ followTail WRITE setFollowTail NOTIFY followTailChanged)
    Q_PROPERTY(QString xLabelFormat READ xLabelFormat WRITE setXLabelFormat NOTIFY xLabelFormatChanged)
    Q_PROPERTY(QString yLabelFormat READ yLabelFormat WRITE setYLabelFormat NOTIFY yLabelFormatChanged)
    Q_PROPERTY(QString xLabelUnit READ xLabelUnit WRITE setXLabelUnit NOTIFY xLabelUnitChanged)
    Q_PROPERTY(QString yLabelUnit READ yLabelUnit WRITE setYLabelUnit NOTIFY yLabelUnitChanged)
    Q_PROPERTY(QString xLabelPrefix READ xLabelPrefix WRITE setXLabelPrefix NOTIFY xLabelPrefixChanged)
    Q_PROPERTY(QString yLabelPrefix READ yLabelPrefix WRITE setYLabelPrefix NOTIFY yLabelPrefixChanged)
    Q_PROPERTY(QString xLabelSuffix READ xLabelSuffix WRITE setXLabelSuffix NOTIFY xLabelSuffixChanged)
    Q_PROPERTY(QString yLabelSuffix READ yLabelSuffix WRITE setYLabelSuffix NOTIFY yLabelSuffixChanged)
    Q_PROPERTY(QQmlListProperty<MarkerLine> markerLines READ markerLines)

public:
    explicit LineChartItem(QQuickItem *parent = nullptr);
    ~LineChartItem() override;

    // Range accessors
    double xMin() const { return m_xMin; }
    double xMax() const { return m_xMax; }
    double yMin() const { return m_yMin; }
    double yMax() const { return m_yMax; }
    void setXMin(double v);
    void setXMax(double v);
    void setYMin(double v);
    void setYMax(double v);

    // Zoom/pan operations (moved from QML)
    Q_INVOKABLE void applyWheel(double angleDelta, double mouseX, double mouseY, const QString &zoomAxis);
    Q_INVOKABLE void applyPinch(double scale, double centroidX, double centroidY, const QString &zoomAxis);
    Q_INVOKABLE void applyDrag(double deltaX, double deltaY);
    Q_INVOKABLE bool computeResetActive() const;
    bool resetActive() const { return m_resetActive; }

    int historyCapacity() const { return m_historyCapacity; }
    void setHistoryCapacity(int cap);

    bool decimationEnabled() const { return m_decimationEnabled; }
    void setDecimationEnabled(bool e);
    qreal targetPixelsPerPoint() const { return m_targetPixelsPerPoint; }
    void setTargetPixelsPerPoint(qreal v);

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor &c);
    QColor gridMajorColor() const { return m_gridMajorColor; }
    void setGridMajorColor(const QColor &c);
    QColor gridMinorColor() const { return m_gridMinorColor; }
    void setGridMinorColor(const QColor &c);
    QColor axisLineColor() const { return m_axisLineColor; }
    void setAxisLineColor(const QColor &c);
    QColor axisTickColor() const { return m_axisTickColor; }
    void setAxisTickColor(const QColor &c);
    QColor labelTextColor() const { return m_labelTextColor; }
    void setLabelTextColor(const QColor &c);
    QColor axisSelectionColor() const { return m_axisSelectionColor; }
    void setAxisSelectionColor(const QColor &c);

    int plotLeftMargin() const { return m_plotLeftMargin; }
    void setPlotLeftMargin(int v);
    int plotBottomMargin() const { return m_plotBottomMargin; }
    int plotTopMargin() const { return m_plotTopMargin; }
    int plotRightMargin() const { return m_plotRightMargin; }
    QString xAxisTitle() const { return m_xAxisTitle; }
    QString yAxisTitle() const { return m_yAxisTitle; }
    void setPlotBottomMargin(int v);
    void setPlotTopMargin(int v);
    void setPlotRightMargin(int v);
    void setXAxisTitle(const QString &t);
    void setYAxisTitle(const QString &t);

    double majorTickStepX() const { return m_majorTickStepX; }
    void setMajorTickStepX(double v);
    double minorTickStepX() const { return m_minorTickStepX; }
    void setMinorTickStepX(double v);
    double majorTickStepY() const { return m_majorTickStepY; }
    void setMajorTickStepY(double v);
    double minorTickStepY() const { return m_minorTickStepY; }
    void setMinorTickStepY(double v);
    int minorSectionsX() const { return m_minorSectionsX; }
    void setMinorSectionsX(int v);
    int minorSectionsY() const { return m_minorSectionsY; }
    void setMinorSectionsY(int v);

    double minXSpan() const { return m_minXSpan; }
    void setMinXSpan(double v)
    {
        m_minXSpan = v;
        emit appearanceChanged();
    }
    double maxXSpan() const { return m_maxXSpan; }
    void setMaxXSpan(double v)
    {
        m_maxXSpan = v;
        emit appearanceChanged();
    }
    double minYSpan() const { return m_minYSpan; }
    void setMinYSpan(double v)
    {
        m_minYSpan = v;
        emit appearanceChanged();
    }
    double maxYSpan() const { return m_maxYSpan; }
    void setMaxYSpan(double v)
    {
        m_maxYSpan = v;
        emit appearanceChanged();
    }
    double defaultXSpan() const { return m_defaultXSpan; }
    void setDefaultXSpan(double v)
    {
        m_defaultXSpan = v;
        emit appearanceChanged();
    }
    double defaultYSpan() const { return m_defaultYSpan; }
    void setDefaultYSpan(double v)
    {
        m_defaultYSpan = v;
        emit appearanceChanged();
    }
    double defaultXMin() const { return m_defaultXMin; }
    void setDefaultXMin(double v)
    {
        m_defaultXMin = v;
        emit appearanceChanged();
    }
    double defaultXMax() const { return m_defaultXMax; }
    void setDefaultXMax(double v)
    {
        m_defaultXMax = v;
        emit appearanceChanged();
    }
    double defaultYMin() const { return m_defaultYMin; }
    void setDefaultYMin(double v)
    {
        m_defaultYMin = v;
        emit appearanceChanged();
    }
    double defaultYMax() const { return m_defaultYMax; }
    void setDefaultYMax(double v)
    {
        m_defaultYMax = v;
        emit appearanceChanged();
    }
    bool followTail() const { return m_followTail; }
    void setFollowTail(bool enabled);

    QVariantList labeledXTicksVariant() const;
    void setLabeledXTicksVariant(const QVariantList &v);
    QVariantList labeledYTicksVariant() const;
    void setLabeledYTicksVariant(const QVariantList &v);
    QVariantList labeledXMinorTicksVariant() const;
    void setLabeledXMinorTicksVariant(const QVariantList &v);
    QVariantList labeledYMinorTicksVariant() const;
    void setLabeledYMinorTicksVariant(const QVariantList &v);
    QVariantList labeledXTickLabelsVariant() const;
    QVariantList labeledYTickLabelsVariant() const;
    QVariantList xLabelPxVariant() const;
    QVariantList yLabelPxVariant() const;

    int xLabelDecimals() const { return m_xLabelDecimals; }
    void setXLabelDecimals(int v);
    int yLabelDecimals() const { return m_yLabelDecimals; }
    void setYLabelDecimals(int v);

    QQmlListProperty<LineSeries> series();
    bool autoRangeX() const { return m_autoRangeX; }
    void setAutoRangeX(bool enabled);
    bool autoRangeY() const { return m_autoRangeY; }
    void setAutoRangeY(bool enabled);

    bool userModifiedX() const { return m_userModifiedX; }
    bool userModifiedY() const { return m_userModifiedY; }

    QString xLabelFormat() const { return m_xLabelFormat; }
    void setXLabelFormat(const QString &format);
    QString yLabelFormat() const { return m_yLabelFormat; }
    void setYLabelFormat(const QString &format);

    QString xLabelUnit() const { return m_xLabelUnit; }
    void setXLabelUnit(const QString &unit);
    QString yLabelUnit() const { return m_yLabelUnit; }
    void setYLabelUnit(const QString &unit);

    QString xLabelPrefix() const { return m_xLabelPrefix; }
    void setXLabelPrefix(const QString &prefix);
    QString yLabelPrefix() const { return m_yLabelPrefix; }
    void setYLabelPrefix(const QString &prefix);

    QString xLabelSuffix() const { return m_xLabelSuffix; }
    void setXLabelSuffix(const QString &suffix);
    QString yLabelSuffix() const { return m_yLabelSuffix; }
    void setYLabelSuffix(const QString &suffix);

    // C++ only: custom formatter functions
    using LabelFormatterFunc = std::function<QString(double)>;
    void setXLabelFormatter(LabelFormatterFunc formatter);
    void setYLabelFormatter(LabelFormatterFunc formatter);

    Q_INVOKABLE int addSeries(const QString &name, const QColor &color, qreal width);
    Q_INVOKABLE void removeSeries(int seriesId);

    Q_INVOKABLE void appendPoints(int seriesId, const QVector<QPointF> &pts);
    Q_INVOKABLE void replaceBuffer(int seriesId, const QVector<QPointF> &pts);
    Q_INVOKABLE void clear(int seriesId);
    Q_INVOKABLE double bufferValueAt(int seriesId, int index);
    Q_INVOKABLE void setSeriesColor(int seriesId, const QColor &c);
    Q_INVOKABLE void setSeriesFillColor(int seriesId, const QColor &c);
    Q_INVOKABLE void setSeriesWidth(int seriesId, qreal w);
    Q_INVOKABLE void setSeriesBaseline(int seriesId, double baselineY);
    Q_INVOKABLE void clearSeriesBaseline(int seriesId);
    Q_INVOKABLE void setSeriesStyle(int seriesId, int style);
    Q_INVOKABLE void setSeriesPen(int seriesId, const QPen &pen);
    Q_INVOKABLE void setSeriesBrush(int seriesId, const QBrush &brush);
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void resetZoomX();
    Q_INVOKABLE void resetZoomY();

    // Programmatic range updates (don't trigger userModified flag)
    Q_INVOKABLE void setProgrammaticXRange(double xMin, double xMax);
    Q_INVOKABLE void setProgrammaticYRange(double yMin, double yMax);

    // Reset to autorange mode
    Q_INVOKABLE void resetToAutoRange();
    Q_INVOKABLE void resetToAutoRangeX();
    Q_INVOKABLE void resetToAutoRangeY();

    // Jump to show latest data immediately
    Q_INVOKABLE void enableFollowTail();
    Q_INVOKABLE void jumpToLatestData();

    // Marker line management API
    QQmlListProperty<MarkerLine> markerLines();

    Q_INVOKABLE int addMarkerLine(bool isVertical, double position, const QString &name = QString(), const QColor &color = QColor(255, 255, 0, 180),
                                  qreal width = 1.0);
    Q_INVOKABLE void removeMarkerLine(int lineId);
    Q_INVOKABLE void setMarkerLinePosition(int lineId, double position);
    Q_INVOKABLE void setMarkerLineColor(int lineId, const QColor &color);
    Q_INVOKABLE void setMarkerLineWidth(int lineId, qreal width);
    Q_INVOKABLE void setMarkerLineStyle(int lineId, int style);
    Q_INVOKABLE void setMarkerLinePen(int lineId, const QPen &pen);
    Q_INVOKABLE void setMarkerLineVisible(int lineId, bool visible);
    Q_INVOKABLE void clearAllMarkerLines();

    double baselineY() const { return m_baselineY; }
    void setBaselineY(double v);

signals:
    void rangeChanged();
    void historyCapacityChanged();
    void appearanceChanged();
    void xAxisTitleChanged();
    void yAxisTitleChanged();
    void seriesUpdated(int seriesId, int count);
    void resetActiveChanged();
    void autoRangeXChanged();
    void autoRangeYChanged();
    void userModifiedXChanged();
    void userModifiedYChanged();
    void followTailChanged();
    void xLabelFormatChanged();
    void yLabelFormatChanged();
    void xLabelUnitChanged();
    void yLabelUnitChanged();
    void xLabelPrefixChanged();
    void yLabelPrefixChanged();
    void xLabelSuffixChanged();
    void yLabelSuffixChanged();
    void markerLinesChanged();
    void axisSelectionColorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void releaseResources() override;
    void componentComplete() override;

private:
    QVector<QPointF> filterVisibleRange(const QVector<QPointF> &data, double xMin, double xMax) const;
    QVector<QPointF> decimateEnvelope(const QVector<QPointF> &data, int widthPx, double xMin, double xMax) const;
    void recomputeTicks();
    void updateResetActive();
    void batchUpdateBegin();
    void batchUpdateEnd();
    LineSeries *findSeries(int seriesId);

    double clampXSpan(double span) const;
    double clampYSpan(double span) const;
    void enforceXSpanConstraints();
    void enforceYSpanConstraints();

    QString formatXLabel(double value) const;
    QString formatYLabel(double value) const;

    static void series_append(QQmlListProperty<LineSeries> *prop, LineSeries *s);
    static qsizetype series_count(QQmlListProperty<LineSeries> *prop);
    static LineSeries *series_at(QQmlListProperty<LineSeries> *prop, qsizetype idx);
    static void series_clear(QQmlListProperty<LineSeries> *prop);

    // Core range
    double m_xMin = 0.0, m_xMax = 10.0;
    double m_yMin = -1.0, m_yMax = 1.0;
    int m_historyCapacity = 4096;
    bool m_decimationEnabled = true;
    qreal m_targetPixelsPerPoint = 1.0;
    bool m_resetActive = false;

    // Appearance
    QColor m_backgroundColor = QColor(0x101014);
    QColor m_gridMajorColor = QColor(0x2a2a2e);
    QColor m_gridMinorColor = QColor(0x1c1c20);
    QColor m_axisLineColor = QColor(0xc0c0c8);
    QColor m_axisTickColor = QColor(0xa0a0a8);
    QColor m_labelTextColor = QColor(0xe0e0e8);
    int m_plotLeftMargin = 40;
    int m_plotBottomMargin = 24;
    int m_plotTopMargin = 20;
    int m_plotRightMargin = 20;
    QString m_xAxisTitle;
    QString m_yAxisTitle;

    // Ticks
    double m_majorTickStepX = 0.0;
    double m_minorTickStepX = 0.0;
    double m_majorTickStepY = 0.0;
    double m_minorTickStepY = 0.0;
    int m_minorSectionsX = 0;
    int m_minorSectionsY = 0;
    int m_xLabelDecimals = 2;
    int m_yLabelDecimals = 2;

    // Zoom constraints
    double m_minXSpan = 0.0;
    double m_maxXSpan = 0.0;
    double m_minYSpan = 0.0;
    double m_maxYSpan = 0.0;
    double m_defaultXSpan = 0.0;
    double m_defaultYSpan = 0.0;
    double m_defaultXMin = 0.0;
    double m_defaultXMax = 0.0;
    double m_defaultYMin = 0.0;
    double m_defaultYMax = 0.0;

    // Labeled ticks
    QVector<double> m_labeledXTicks;
    QVector<double> m_labeledYTicks;
    QVector<double> m_labeledXMinorTicks;
    QVector<double> m_labeledYMinorTicks;
    QVector<QString> m_labeledXTickLabels;
    QVector<QString> m_labeledYTickLabels;
    QVector<float> m_xLabelPx;
    QVector<float> m_yLabelPx;

    // Auto-range and user modification flags
    bool m_autoRangeX = false;
    bool m_autoRangeY = false;
    bool m_userModifiedX = false;
    bool m_userModifiedY = false;
    bool m_programmaticUpdate = false;

    QString m_xLabelFormat = "fixed";
    QString m_yLabelFormat = "fixed";
    QString m_xLabelUnit;
    QString m_yLabelUnit;
    QString m_xLabelPrefix;
    QString m_yLabelPrefix;
    QString m_xLabelSuffix;
    QString m_yLabelSuffix;
    LabelFormatterFunc m_xLabelFormatter = nullptr;
    LabelFormatterFunc m_yLabelFormatter = nullptr;

    // Series data
    QList<LineSeries *> m_series;
    struct SeriesBuffer
    {
        QVector<QPointF> data;
        int generation = 0;
    };
    QHash<int, SeriesBuffer> m_buffers;

    QList<MarkerLine *> m_markerLines;
    int m_nextMarkerLineId = 0;

    struct RenderCache
    {
        QVector<QPointF> decimated;
        double xMin = 0.0;
        double xMax = 0.0;
        int plotW = 0;
        int dataSize = 0;
        int generation = -1;
    };
    QHash<int, RenderCache> m_renderCache;

    // Scene graph nodes
    QSGGeometryNode *m_gridMinorNode = nullptr;
    QSGGeometryNode *m_gridNode = nullptr;
    QSGGeometryNode *m_axesNode = nullptr;
    QSGNode *m_markerLinesNode = nullptr;
    QSGNode *m_seriesRoot = nullptr;
    QSGNode *m_seriesStrokeRoot = nullptr;
    double m_baselineY = 0.0;
    QHash<int, QSGGeometryNode *> m_seriesStrokeNodes;

    QQuickWindow *m_window = nullptr;
    bool m_rebuild = false;

    // Cache for optimization
    float m_lastPlotX0 = -1.0f;
    float m_lastPlotY0 = -1.0f;
    float m_lastPlotW = -1.0f;
    float m_lastPlotH = -1.0f;
    double m_lastXMin = std::numeric_limits<double>::quiet_NaN();
    double m_lastXMax = std::numeric_limits<double>::quiet_NaN();
    double m_lastYMin = std::numeric_limits<double>::quiet_NaN();
    double m_lastYMax = std::numeric_limits<double>::quiet_NaN();

    // Batch update flag
    int m_batchUpdateDepth = 0;
    bool m_pendingUpdate = false;

    bool m_followTail = true;  // Enabled by default for streaming use cases

    // Built-in formatters
    static QString formatFixed(double value, int decimals);
    static QString formatScientific(double value, int decimals);
    static QString formatTimeMMSS(double seconds);
    static QString formatTimeHHMMSS(double seconds);
    static QString formatTimeHHMM(double seconds);
    static QString formatPercentage(double value, int decimals);
    static QString formatAngleDegrees(double radians, int decimals);
    static QString formatAngleRadians(double radians, int decimals);
    static QString formatDecibel(double value, int decimals);
    static QString formatEngineering(double value, int decimals, const QString &unit);
    QColor m_axisSelectionColor;

    MarkerLine *findMarkerLine(int lineId);  // NEW

    static void markerLines_append(QQmlListProperty<MarkerLine> *prop, MarkerLine *line);  // NEW
    static qsizetype markerLines_count(QQmlListProperty<MarkerLine> *prop);                // NEW
    static MarkerLine *markerLines_at(QQmlListProperty<MarkerLine> *prop, qsizetype idx);  // NEW
    static void markerLines_clear(QQmlListProperty<MarkerLine> *prop);
};

#endif  // LINECHARTITEM_H
