#include "linechartitem.h"

#include <QBrush>
#include <QPen>
#include <QQuickWindow>
#include <QSGClipNode>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <cmath>

LineChartItem::LineChartItem(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::widthChanged, this, &LineChartItem::recomputeTicks);
    connect(this, &QQuickItem::heightChanged, this, &LineChartItem::recomputeTicks);
}

LineChartItem::~LineChartItem()
{
    if (m_seriesStrokeRoot)
    {
        while (QSGNode *n = m_seriesStrokeRoot->firstChild())
        {
            m_seriesStrokeRoot->removeChildNode(n);
            delete n;
        }
    }
    m_seriesStrokeNodes.clear();

    for (auto *s : std::as_const(m_series))
    {
        if (s)
        {
            s->deleteLater();
        }
    }
    m_series.clear();

    for (auto *line : std::as_const(m_markerLines))
    {
        if (line)
        {
            line->deleteLater();
        }
    }
    m_markerLines.clear();
}

void LineChartItem::componentComplete()
{
    QQuickItem::componentComplete();
    recomputeTicks();
}

// ============================================================================
// OPTIMIZATION: Batch updates to reduce signal spam
// ============================================================================
void LineChartItem::batchUpdateBegin()
{
    ++m_batchUpdateDepth;
}

void LineChartItem::batchUpdateEnd()
{
    if (--m_batchUpdateDepth == 0 && m_pendingUpdate)
    {
        m_pendingUpdate = false;
        update();
    }
}

// ============================================================================
// FIXED: Wheel Zoom with Span Constraints
// ============================================================================

void LineChartItem::applyWheel(double angleDelta, double mouseX, double mouseY, const QString &zoomAxis)
{
    // User zooming disables followTail (they want manual control)
    if (m_followTail && (zoomAxis == "x" || zoomAxis == "both" || zoomAxis == "none"))
    {
        m_followTail = false;
        emit followTailChanged();
        // qDebug() << "FollowTail disabled by wheel zoom";
    }

    // Disable autorange on user interaction
    if (zoomAxis == "y" || zoomAxis == "both" || zoomAxis == "none")
    {
        if (m_autoRangeY)
        {
            m_autoRangeY = false;
            m_userModifiedY = true;
            emit autoRangeYChanged();
            emit userModifiedYChanged();
        }
    }

    if (zoomAxis == "x" || zoomAxis == "both" || zoomAxis == "none")
    {
        if (m_autoRangeX)
        {
            m_autoRangeX = false;
            m_userModifiedX = true;
            emit autoRangeXChanged();
            emit userModifiedXChanged();
        }
    }

    // Existing zoom code
    batchUpdateBegin();

    const double zoom = std::pow(1.1, angleDelta / 120.0);
    const double xSpan = m_xMax - m_xMin;
    const double ySpan = m_yMax - m_yMin;
    const double plotX0 = m_plotLeftMargin + m_plotRightMargin;
    const double plotW = width() - m_plotLeftMargin - m_plotRightMargin;
    const double plotY0 = m_plotTopMargin;
    const double plotH = height() - m_plotTopMargin - m_plotBottomMargin;

    const double px = std::max(0.0, std::min(1.0, (mouseX - plotX0) / plotW));
    const double py = std::max(0.0, std::min(1.0, 1.0 - (mouseY - plotY0) / plotH));
    const double cx = m_xMin + xSpan * px;
    const double cy = m_yMin + ySpan * py;

    double nxSpan = xSpan / zoom;
    double nySpan = ySpan / zoom;

    nxSpan = clampXSpan(nxSpan);
    nySpan = clampYSpan(nySpan);

    if (zoomAxis == "y")
    {
        setYMin(cy - nySpan * py);
        setYMax(m_yMin + nySpan);
    }
    else if (zoomAxis == "x")
    {
        setXMin(cx - nxSpan * px);
        setXMax(m_xMin + nxSpan);
    }
    else
    {
        setXMin(cx - nxSpan * px);
        setXMax(m_xMin + nxSpan);
        setYMin(cy - nySpan * py);
        setYMax(m_yMin + nySpan);
    }

    batchUpdateEnd();
}

void LineChartItem::applyPinch(double scale, double centroidX, double centroidY, const QString &zoomAxis)
{
    if (scale <= 0)
    {
        return;
    }

    // Disable followTail on pinch
    if (m_followTail && (zoomAxis == "x" || zoomAxis == "both" || zoomAxis == "none"))
    {
        m_followTail = false;
        emit followTailChanged();
        // qDebug() << "FollowTail disabled by pinch zoom";
    }

    // Disable autorange on pinch
    if (zoomAxis == "y" || zoomAxis == "both" || zoomAxis == "none")
    {
        if (m_autoRangeY)
        {
            m_autoRangeY = false;
            m_userModifiedY = true;
            emit autoRangeYChanged();
            emit userModifiedYChanged();
        }
    }

    if (zoomAxis == "x" || zoomAxis == "both" || zoomAxis == "none")
    {
        if (m_autoRangeX)
        {
            m_autoRangeX = false;
            m_userModifiedX = true;
            emit autoRangeXChanged();
            emit userModifiedXChanged();
        }
    }

    // Existing pinch zoom code
    batchUpdateBegin();

    const double xSpan = m_xMax - m_xMin;
    const double ySpan = m_yMax - m_yMin;
    const double plotX0 = 0;  // m_plotLeftMargin + m_plotRightMargin;
    const double plotW = width() - m_plotLeftMargin - m_plotRightMargin;
    const double plotY0 = 0;  // m_plotTopMargin;
    const double plotH = height() - m_plotTopMargin - m_plotBottomMargin;

    const double px = std::max(0.0, std::min(1.0, (centroidX - plotX0) / plotW));
    const double py = std::max(0.0, std::min(1.0, 1.0 - (centroidY - plotY0) / plotH));
    const double cx = m_xMin + xSpan * px;
    const double cy = m_yMin + ySpan * py;

    // qDebug() << "Pinch zoom scale:" << scale << "centroidX:" << centroidX << "centroidY:" << centroidY << "px:" << px << "py:" << py
    //          << m_plotLeftMargin;

    double nxSpan = xSpan / scale;
    double nySpan = ySpan / scale;

    nxSpan = clampXSpan(nxSpan);
    nySpan = clampYSpan(nySpan);

    if (zoomAxis == "y")
    {
        setYMin(cy - nySpan * py);
        setYMax(m_yMin + nySpan);
    }
    else if (zoomAxis == "x")
    {
        setXMin(cx - nxSpan * px);
        setXMax(m_xMin + nxSpan);
    }
    else
    {
        setXMin(cx - nxSpan * px);
        setXMax(m_xMin + nxSpan);
        setYMin(cy - nySpan * py);
        setYMax(m_yMin + nySpan);
    }

    batchUpdateEnd();
}

void LineChartItem::applyDrag(double deltaX, double deltaY)
{
    // User drag disables followTail
    if (m_followTail)
    {
        m_followTail = false;
        emit followTailChanged();
        // qDebug() << "FollowTail disabled by drag";
    }

    // Disable autorange on drag
    if (m_autoRangeY)
    {
        m_autoRangeY = false;
        m_userModifiedY = true;
        emit autoRangeYChanged();
        emit userModifiedYChanged();
    }

    if (m_autoRangeX)
    {
        m_autoRangeX = false;
        m_userModifiedX = true;
        emit autoRangeXChanged();
        emit userModifiedXChanged();
    }

    // Existing drag code
    batchUpdateBegin();

    const double xSpan = m_xMax - m_xMin;
    const double ySpan = m_yMax - m_yMin;
    const double dx = deltaX / width() * xSpan;
    const double dy = deltaY / height() * ySpan;

    setXMin(m_xMin - dx);
    setXMax(m_xMin + xSpan);
    setYMin(m_yMin + dy);
    setYMax(m_yMin + ySpan);

    batchUpdateEnd();
}

// ============================================================================
// OPTIMIZATION: Move computeResetActive from QML to C++
// ============================================================================
bool LineChartItem::computeResetActive() const
{
    // If autorange is fully enabled, reset button should be inactive
    // (user should only see reset when they've manually modified the view)
    if (m_autoRangeX && m_autoRangeY)
    {
        return false;
    }

    constexpr double eps = 1e-6;
    const double xSpan = m_xMax - m_xMin;
    const double ySpan = m_yMax - m_yMin;

    bool xDirty = false;
    if (m_defaultXMax > m_defaultXMin)
    {
        xDirty = std::abs(m_xMin - m_defaultXMin) > eps || std::abs(m_xMax - m_defaultXMax) > eps;
    }
    else if (m_defaultXSpan > 0)
    {
        xDirty = std::abs(xSpan - m_defaultXSpan) > eps;
    }

    bool yDirty = false;
    if (m_defaultYMax > m_defaultYMin)
    {
        yDirty = std::abs(m_yMin - m_defaultYMin) > eps || std::abs(m_yMax - m_defaultYMax) > eps;
    }
    else if (m_defaultYSpan > 0)
    {
        yDirty = std::abs(ySpan - m_defaultYSpan) > eps;
    }

    return xDirty || yDirty;
}

void LineChartItem::updateResetActive()
{
    bool newActive = computeResetActive();
    if (m_resetActive != newActive)
    {
        m_resetActive = newActive;
        emit resetActiveChanged();
    }
}

// ============================================================================
// OPTIMIZATION: Reduce redundant updates in setters
// ============================================================================
void LineChartItem::setXMin(double v)
{
    if (std::abs(m_xMin - v) < 1e-12)
    {
        return;
    }

    // Track user modification if this is not a programmatic update
    if (!m_programmaticUpdate && m_autoRangeX)
    {
        m_userModifiedX = true;
        m_autoRangeX = false;
        emit userModifiedXChanged();
        emit autoRangeXChanged();
    }

    m_xMin = v;

    // ✅ FIX: Enforce span constraints BEFORE other logic
    enforceXSpanConstraints();

    // Existing default bounds enforcement
    if (m_defaultXMax > m_defaultXMin)
    {
        const double defSpan = m_defaultXMax - m_defaultXMin;
        // Update maxXSpan from defaults (if not explicitly set)
        if (m_maxXSpan <= 0)
        {
            m_maxXSpan = defSpan;
        }

        double curSpan = m_xMax - m_xMin;
        if (curSpan > defSpan)
        {
            double mid = (m_xMin + m_xMax) * 0.5;
            m_xMin = mid - defSpan * 0.5;
            m_xMax = mid + defSpan * 0.5;
        }
        if (m_xMin < m_defaultXMin)
        {
            double shift = m_defaultXMin - m_xMin;
            m_xMin += shift;
            m_xMax += shift;
        }
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    if (m_batchUpdateDepth > 0)
    {
        m_pendingUpdate = true;
    }
    else
    {
        update();
    }
}

void LineChartItem::setXMax(double v)
{
    if (std::abs(m_xMax - v) < 1e-12)
    {
        return;
    }

    if (!m_programmaticUpdate && m_autoRangeX)
    {
        m_userModifiedX = true;
        m_autoRangeX = false;
        emit userModifiedXChanged();
        emit autoRangeXChanged();
    }

    m_xMax = v;

    // ✅ FIX: Enforce span constraints BEFORE other logic
    enforceXSpanConstraints();

    if (m_defaultXMax > m_defaultXMin)
    {
        const double defSpan = m_defaultXMax - m_defaultXMin;
        if (m_maxXSpan <= 0)
        {
            m_maxXSpan = defSpan;
        }

        double curSpan = m_xMax - m_xMin;
        if (curSpan > defSpan)
        {
            double mid = (m_xMin + m_xMax) * 0.5;
            m_xMin = mid - defSpan * 0.5;
            m_xMax = mid + defSpan * 0.5;
        }
        if (m_xMax > m_defaultXMax)
        {
            double shift = m_xMax - m_defaultXMax;
            m_xMin -= shift;
            m_xMax -= shift;
        }
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    if (m_batchUpdateDepth > 0)
    {
        m_pendingUpdate = true;
    }
    else
    {
        update();
    }
}

void LineChartItem::setYMin(double v)
{
    if (std::abs(m_yMin - v) < 1e-12)
    {
        return;
    }

    if (!m_programmaticUpdate && m_autoRangeY)
    {
        m_userModifiedY = true;
        m_autoRangeY = false;
        emit userModifiedYChanged();
        emit autoRangeYChanged();
    }

    m_yMin = v;
    if (m_yMax < m_yMin)
    {
        std::swap(m_yMax, m_yMin);
    }

    // ✅ FIX: Add span constraint enforcement for Y axis
    enforceYSpanConstraints();

    // ✅ FIX: Add default bounds enforcement for Y axis (was missing!)
    if (m_defaultYMax > m_defaultYMin)
    {
        const double defSpan = m_defaultYMax - m_defaultYMin;
        if (m_maxYSpan <= 0)
        {
            m_maxYSpan = defSpan;
        }

        double curSpan = m_yMax - m_yMin;
        if (curSpan > defSpan)
        {
            double mid = (m_yMin + m_yMax) * 0.5;
            m_yMin = mid - defSpan * 0.5;
            m_yMax = mid + defSpan * 0.5;
        }
        if (m_yMin < m_defaultYMin)
        {
            double shift = m_defaultYMin - m_yMin;
            m_yMin += shift;
            m_yMax += shift;
        }
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    if (m_batchUpdateDepth > 0)
    {
        m_pendingUpdate = true;
    }
    else
    {
        update();
    }
}

void LineChartItem::setYMax(double v)
{
    if (std::abs(m_yMax - v) < 1e-12)
    {
        return;
    }

    if (!m_programmaticUpdate && m_autoRangeY)
    {
        m_userModifiedY = true;
        m_autoRangeY = false;
        emit userModifiedYChanged();
        emit autoRangeYChanged();
    }

    m_yMax = v;
    if (m_yMax < m_yMin)
    {
        std::swap(m_yMax, m_yMin);
    }

    // ✅ FIX: Add span constraint enforcement for Y axis
    enforceYSpanConstraints();

    // ✅ FIX: Add default bounds enforcement for Y axis (was missing!)
    if (m_defaultYMax > m_defaultYMin)
    {
        const double defSpan = m_defaultYMax - m_defaultYMin;
        if (m_maxYSpan <= 0)
        {
            m_maxYSpan = defSpan;
        }

        double curSpan = m_yMax - m_yMin;
        if (curSpan > defSpan)
        {
            double mid = (m_yMin + m_yMax) * 0.5;
            m_yMin = mid - defSpan * 0.5;
            m_yMax = mid + defSpan * 0.5;
        }
        if (m_yMax > m_defaultYMax)
        {
            double shift = m_yMax - m_defaultYMax;
            m_yMin -= shift;
            m_yMax -= shift;
        }
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    if (m_batchUpdateDepth > 0)
    {
        m_pendingUpdate = true;
    }
    else
    {
        update();
    }
}

// ============================================================================
// OPTIMIZATION: Consolidate color setters to reduce boilerplate
// ============================================================================
void LineChartItem::setBackgroundColor(const QColor &c)
{
    if (m_backgroundColor == c)
    {
        return;
    }
    m_backgroundColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setGridMajorColor(const QColor &c)
{
    if (m_gridMajorColor == c)
    {
        return;
    }
    m_gridMajorColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setGridMinorColor(const QColor &c)
{
    if (m_gridMinorColor == c)
    {
        return;
    }
    m_gridMinorColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setAxisLineColor(const QColor &c)
{
    if (m_axisLineColor == c)
    {
        return;
    }
    m_axisLineColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setAxisTickColor(const QColor &c)
{
    if (m_axisTickColor == c)
    {
        return;
    }
    m_axisTickColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setLabelTextColor(const QColor &c)
{
    if (m_labelTextColor == c)
    {
        return;
    }
    m_labelTextColor = c;
    emit appearanceChanged();
    update();
}

void LineChartItem::setAxisSelectionColor(const QColor &c)
{
    if (m_axisSelectionColor == c)
    {
        return;
    }
    m_axisSelectionColor = c;
    emit axisSelectionColorChanged();
    update();
}

void LineChartItem::setDecimationEnabled(bool e)
{
    if (m_decimationEnabled == e)
    {
        return;
    }
    m_decimationEnabled = e;
    emit appearanceChanged();
    update();
}

void LineChartItem::setTargetPixelsPerPoint(qreal v)
{
    if (std::abs(m_targetPixelsPerPoint - v) < 1e-6)
    {
        return;
    }
    m_targetPixelsPerPoint = v;
    emit appearanceChanged();
    update();
}

// ============================================================================
// Margin setters
// ============================================================================
void LineChartItem::setPlotLeftMargin(int v)
{
    if (m_plotLeftMargin == v)
    {
        return;
    }
    m_plotLeftMargin = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setPlotBottomMargin(int v)
{
    if (m_plotBottomMargin == v)
    {
        return;
    }
    m_plotBottomMargin = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setPlotTopMargin(int v)
{
    if (m_plotTopMargin == v)
    {
        return;
    }
    m_plotTopMargin = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setPlotRightMargin(int v)
{
    if (m_plotRightMargin == v)
    {
        return;
    }
    m_plotRightMargin = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setXAxisTitle(const QString &t)
{
    if (m_xAxisTitle == t)
    {
        return;
    }
    m_xAxisTitle = t;
    emit xAxisTitleChanged();
    update();
}

void LineChartItem::setYAxisTitle(const QString &t)
{
    if (m_yAxisTitle == t)
    {
        return;
    }
    m_yAxisTitle = t;
    emit yAxisTitleChanged();
    update();
}

// ============================================================================
// Tick setters
// ============================================================================
void LineChartItem::setMajorTickStepX(double v)
{
    if (std::abs(m_majorTickStepX - v) < 1e-12)
    {
        return;
    }
    m_majorTickStepX = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setMinorTickStepX(double v)
{
    if (std::abs(m_minorTickStepX - v) < 1e-12)
    {
        return;
    }
    m_minorTickStepX = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setMajorTickStepY(double v)
{
    if (std::abs(m_majorTickStepY - v) < 1e-12)
    {
        return;
    }
    m_majorTickStepY = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setMinorTickStepY(double v)
{
    if (std::abs(m_minorTickStepY - v) < 1e-12)
    {
        return;
    }
    m_minorTickStepY = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setMinorSectionsX(int v)
{
    if (m_minorSectionsX == v)
    {
        return;
    }
    m_minorSectionsX = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setMinorSectionsY(int v)
{
    if (m_minorSectionsY == v)
    {
        return;
    }
    m_minorSectionsY = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setXLabelDecimals(int v)
{
    if (m_xLabelDecimals == v)
    {
        return;
    }
    m_xLabelDecimals = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setYLabelDecimals(int v)
{
    if (m_yLabelDecimals == v)
    {
        return;
    }
    m_yLabelDecimals = v;
    recomputeTicks();
    emit appearanceChanged();
    update();
}

void LineChartItem::setBaselineY(double v)
{
    if (std::abs(m_baselineY - v) < 1e-12)
    {
        return;
    }
    m_baselineY = v;
    emit appearanceChanged();
    update();
}

// Continued in next artifact due to length...

// Continuation of LineChartItem.cpp - Data APIs and Rendering

void LineChartItem::setHistoryCapacity(int cap)
{
    if (m_historyCapacity == cap)
    {
        return;
    }
    m_historyCapacity = cap;
    emit historyCapacityChanged();
}

// ============================================================================
// Variant conversion helpers (optimized with move semantics)
// ============================================================================
QVariantList LineChartItem::labeledXTicksVariant() const
{
    QVariantList v;
    v.reserve(m_labeledXTicks.size());
    for (double d : m_labeledXTicks)
    {
        v.append(d);
    }
    return v;
}

void LineChartItem::setLabeledXTicksVariant(const QVariantList &v)
{
    m_labeledXTicks.clear();
    m_labeledXTicks.reserve(v.size());
    for (const auto &e : v)
    {
        m_labeledXTicks.append(e.toDouble());
    }
    recomputeTicks();
    update();
}

QVariantList LineChartItem::labeledYTicksVariant() const
{
    QVariantList v;
    v.reserve(m_labeledYTicks.size());
    for (double d : m_labeledYTicks)
    {
        v.append(d);
    }
    return v;
}

void LineChartItem::setLabeledYTicksVariant(const QVariantList &v)
{
    m_labeledYTicks.clear();
    m_labeledYTicks.reserve(v.size());
    for (const auto &e : v)
    {
        m_labeledYTicks.append(e.toDouble());
    }
    recomputeTicks();
    update();
}

QVariantList LineChartItem::labeledXMinorTicksVariant() const
{
    QVariantList v;
    v.reserve(m_labeledXMinorTicks.size());
    for (double d : m_labeledXMinorTicks)
    {
        v.append(d);
    }
    return v;
}

void LineChartItem::setLabeledXMinorTicksVariant(const QVariantList &v)
{
    m_labeledXMinorTicks.clear();
    m_labeledXMinorTicks.reserve(v.size());
    for (const auto &e : v)
    {
        m_labeledXMinorTicks.append(e.toDouble());
    }
    recomputeTicks();
    update();
}

QVariantList LineChartItem::labeledYMinorTicksVariant() const
{
    QVariantList v;
    v.reserve(m_labeledYMinorTicks.size());
    for (double d : m_labeledYMinorTicks)
    {
        v.append(d);
    }
    return v;
}

void LineChartItem::setLabeledYMinorTicksVariant(const QVariantList &v)
{
    m_labeledYMinorTicks.clear();
    m_labeledYMinorTicks.reserve(v.size());
    for (const auto &e : v)
    {
        m_labeledYMinorTicks.append(e.toDouble());
    }
    recomputeTicks();
    update();
}

QVariantList LineChartItem::labeledXTickLabelsVariant() const
{
    QVariantList v;
    v.reserve(m_labeledXTickLabels.size());
    for (const auto &s : m_labeledXTickLabels)
    {
        v.append(s);
    }
    return v;
}

QVariantList LineChartItem::labeledYTickLabelsVariant() const
{
    QVariantList v;
    v.reserve(m_labeledYTickLabels.size());
    for (const auto &s : m_labeledYTickLabels)
    {
        v.append(s);
    }
    return v;
}

QVariantList LineChartItem::xLabelPxVariant() const
{
    QVariantList v;
    v.reserve(m_xLabelPx.size());
    for (float p : m_xLabelPx)
    {
        v.append(p);
    }
    return v;
}

QVariantList LineChartItem::yLabelPxVariant() const
{
    QVariantList v;
    v.reserve(m_yLabelPx.size());
    for (float p : m_yLabelPx)
    {
        v.append(p);
    }
    return v;
}

// ============================================================================
// Series Management
// ============================================================================
QQmlListProperty<LineSeries> LineChartItem::series()
{
    return QQmlListProperty<LineSeries>(this, &m_series, &LineChartItem::series_append, &LineChartItem::series_count, &LineChartItem::series_at,
                                        &LineChartItem::series_clear);
}

int LineChartItem::addSeries(const QString &name, const QColor &color, qreal width)
{
    auto *s = new LineSeries(this);
    s->setSeriesId(m_series.size());
    s->setName(name);
    s->setColor(color);
    s->setWidth(width);
    m_series.append(s);
    m_buffers[s->seriesId()] = SeriesBuffer{};
    emit seriesUpdated(s->seriesId(), 0);
    update();
    return s->seriesId();
}

void LineChartItem::removeSeries(int seriesId)
{
    for (int i = 0; i < m_series.size(); ++i)
    {
        if (m_series[i]->seriesId() == seriesId)
        {
            LineSeries *s = m_series.takeAt(i);
            m_buffers.remove(seriesId);
            m_renderCache.remove(seriesId);

            // Clean up stroke overlay node
            if (m_seriesStrokeNodes.contains(seriesId))
            {
                QSGGeometryNode *stroke = m_seriesStrokeNodes.take(seriesId);
                if (stroke && m_seriesStrokeRoot)
                {
                    m_seriesStrokeRoot->removeChildNode(stroke);
                    delete stroke;
                }
            }

            if (s)
            {
                s->deleteLater();
            }
            update();
            return;
        }
    }
}

LineSeries *LineChartItem::findSeries(int seriesId)
{
    for (auto *s : std::as_const(m_series))
    {
        if (s->seriesId() == seriesId)
        {
            return s;
        }
    }
    return nullptr;
}

// QQmlListProperty helpers
void LineChartItem::series_append(QQmlListProperty<LineSeries> *prop, LineSeries *s)
{
    auto *self = static_cast<LineChartItem *>(prop->object);
    s->setSeriesId(self->m_series.size());
    self->m_series.append(s);
}

qsizetype LineChartItem::series_count(QQmlListProperty<LineSeries> *prop)
{
    return static_cast<LineChartItem *>(prop->object)->m_series.size();
}

LineSeries *LineChartItem::series_at(QQmlListProperty<LineSeries> *prop, qsizetype i)
{
    return static_cast<LineChartItem *>(prop->object)->m_series.at(i);
}

void LineChartItem::series_clear(QQmlListProperty<LineSeries> *prop)
{
    auto *self = static_cast<LineChartItem *>(prop->object);
    for (auto *s : std::as_const(self->m_series))
    {
        if (s)
        {
            s->deleteLater();
        }
    }
    self->m_series.clear();

    // Clean up all stroke nodes
    if (self->m_seriesStrokeRoot)
    {
        while (QSGNode *n = self->m_seriesStrokeRoot->firstChild())
        {
            self->m_seriesStrokeRoot->removeChildNode(n);
            delete n;
        }
    }
    self->m_seriesStrokeNodes.clear();
}

// ============================================================================
// Data APIs with optimizations
// ============================================================================
void LineChartItem::appendPoints(int seriesId, const QVector<QPointF> &pts)
{
    if (pts.isEmpty())
    {
        return;
    }

    auto &buf = m_buffers[seriesId];

    // Existing capacity management
    if (m_historyCapacity > 0 && buf.data.size() + pts.size() > m_historyCapacity)
    {
        int drop = buf.data.size() + pts.size() - m_historyCapacity;
        if (drop > 0 && drop < buf.data.size())
        {
            buf.data.erase(buf.data.begin(), buf.data.begin() + drop);
        }
        else if (drop >= buf.data.size())
        {
            buf.data.clear();
        }
    }

    buf.data += pts;
    buf.generation++;

    // ✅ NEW: FollowTail auto-scroll logic
    if (m_followTail)
    {
        // Find the maximum X value in the newly added points
        double maxX = -std::numeric_limits<double>::infinity();
        for (const auto &pt : pts)
        {
            maxX = std::max(maxX, pt.x());
        }

        // If latest data exceeds current view, scroll to show it
        if (std::isfinite(maxX) && maxX > m_xMax)
        {
            double currentSpan = m_xMax - m_xMin;

            // Use programmatic update to avoid disabling autorange/followTail
            m_programmaticUpdate = true;
            batchUpdateBegin();
            setXMax(maxX);
            setXMin(maxX - currentSpan);
            batchUpdateEnd();
            m_programmaticUpdate = false;
        }
    }

    emit seriesUpdated(seriesId, pts.size());
    update();
}

void LineChartItem::replaceBuffer(int seriesId, const QVector<QPointF> &pts)
{
    auto &b = m_buffers[seriesId];
    b.data = pts;
    b.generation++;
    emit seriesUpdated(seriesId, pts.size());

    // OPTIMIZATION: Enforce X clamp for replace mode in one place
    const QVariant dm = this->property("dataModeProp");
    if (dm.isValid() && dm.toString() == QStringLiteral("replace"))
    {
        double defMin = m_defaultXMin;
        double defMax = m_defaultXMax;
        double defSpan = (defMax > defMin) ? (defMax - defMin) : m_defaultXSpan;

        if (defSpan > 0.0)
        {
            m_maxXSpan = defSpan;

            if (defMax > defMin)
            {
                double currentSpan = m_xMax - m_xMin;
                if (currentSpan <= 0.0)
                {
                    currentSpan = defSpan;
                }

                if (m_xMin < defMin || m_xMax > defMax)
                {
                    m_xMin = defMin;
                    m_xMax = defMin + std::min(currentSpan, defSpan);
                }
            }
        }
    }

    update();
}

void LineChartItem::clear(int seriesId)
{
    auto &b = m_buffers[seriesId];
    b.data.clear();
    b.generation++;
    emit seriesUpdated(seriesId, 0);
    update();
}

double LineChartItem::bufferValueAt(int seriesId, int index)
{
    auto it = m_buffers.find(seriesId);
    if (it == m_buffers.end())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto &buf = it.value();
    if (index < 0 || index >= buf.data.size())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return buf.data[index].y();
}

// ============================================================================
// Series property setters
// ============================================================================
void LineChartItem::setSeriesColor(int seriesId, const QColor &c)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setColor(c);
        update();
    }
}

void LineChartItem::setSeriesWidth(int seriesId, qreal w)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setWidth(w);
        update();
    }
}

void LineChartItem::setSeriesStyle(int seriesId, int st)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setStyle(st);
        update();
    }
}

void LineChartItem::setSeriesFillColor(int seriesId, const QColor &c)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setFillColor(c);
        update();
    }
}

void LineChartItem::setSeriesBaseline(int seriesId, double y)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setBaselineY(y);
        update();
    }
}

void LineChartItem::clearSeriesBaseline(int seriesId)
{
    if (auto *s = findSeries(seriesId))
    {
        s->setBaselineY(std::numeric_limits<double>::quiet_NaN());
        update();
    }
}

void LineChartItem::setSeriesPen(int seriesId, const QPen &pen)
{
    setSeriesColor(seriesId, pen.color());
    setSeriesWidth(seriesId, pen.widthF() > 0 ? pen.widthF() : (double)pen.width());
}

void LineChartItem::setSeriesBrush(int seriesId, const QBrush &brush)
{
    setSeriesFillColor(seriesId, brush.color());
}

// ============================================================================
// Reset zoom functions
// ============================================================================
void LineChartItem::resetZoom()
{
    batchUpdateBegin();

    if (m_defaultXMax > m_defaultXMin)
    {
        m_xMin = m_defaultXMin;
        m_xMax = m_defaultXMax;
    }
    else if (m_defaultXSpan > 0.0)
    {
        double midX = (m_xMin + m_xMax) * 0.5;
        m_xMin = midX - m_defaultXSpan * 0.5;
        m_xMax = midX + m_defaultXSpan * 0.5;
    }

    if (m_defaultYMax > m_defaultYMin)
    {
        m_yMin = m_defaultYMin;
        m_yMax = m_defaultYMax;
    }
    else if (m_defaultYSpan > 0.0)
    {
        double midY = (m_yMin + m_yMax) * 0.5;
        m_yMin = midY - m_defaultYSpan * 0.5;
        m_yMax = midY + m_defaultYSpan * 0.5;
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    batchUpdateEnd();

    // Restore autorange mode
    resetToAutoRange();
}

void LineChartItem::resetZoomX()
{
    batchUpdateBegin();

    if (m_defaultXMax > m_defaultXMin)
    {
        m_xMin = m_defaultXMin;
        m_xMax = m_defaultXMax;
    }
    else if (m_defaultXSpan > 0.0)
    {
        double midX = (m_xMin + m_xMax) * 0.5;
        m_xMin = midX - m_defaultXSpan * 0.5;
        m_xMax = midX + m_defaultXSpan * 0.5;
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    batchUpdateEnd();

    // Restore X autorange
    resetToAutoRangeX();
}

void LineChartItem::resetZoomY()
{
    batchUpdateBegin();

    if (m_defaultYMax > m_defaultYMin)
    {
        m_yMin = m_defaultYMin;
        m_yMax = m_defaultYMax;
    }
    else if (m_defaultYSpan > 0.0)
    {
        double midY = (m_yMin + m_yMax) * 0.5;
        m_yMin = midY - m_defaultYSpan * 0.5;
        m_yMax = midY + m_defaultYSpan * 0.5;
    }

    recomputeTicks();
    updateResetActive();
    emit rangeChanged();

    batchUpdateEnd();

    // Restore Y autorange
    resetToAutoRangeY();
}

QSGNode *LineChartItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (m_rebuild)
    {
        delete oldNode;
        oldNode = nullptr;
        m_rebuild = false;

        // CRITICAL FIX: Clear ALL cached node pointers on rebuild
        // These belong to the old scene graph context and must not be reused
        m_gridMinorNode = nullptr;
        m_gridNode = nullptr;
        m_axesNode = nullptr;
        m_seriesRoot = nullptr;        // ← MUST clear this
        m_seriesStrokeRoot = nullptr;  // ← MUST clear this
        m_seriesStrokeNodes.clear();   // ← MUST clear this
    }

    auto *root = static_cast<QSGNode *>(oldNode);
    if (!root)
    {
        root = new QSGNode();
    }

    // Background quad
    QSGGeometryNode *bg = nullptr;
    if (root->firstChild())
    {
        bg = static_cast<QSGGeometryNode *>(root->firstChild());
    }
    if (!bg)
    {
        bg = new QSGGeometryNode();
        auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
        g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        bg->setGeometry(g);
        bg->setFlag(QSGNode::OwnsGeometry);
        auto *m = new QSGFlatColorMaterial();
        m->setColor(m_backgroundColor);
        bg->setMaterial(m);
        bg->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(bg);
    }
    {
        auto *g = bg->geometry();
        auto *v = g->vertexDataAsPoint2D();
        v[0].x = 0;
        v[0].y = 0;
        v[1].x = width();
        v[1].y = 0;
        v[2].x = 0;
        v[2].y = height();
        v[3].x = width();
        v[3].y = height();
        auto *mat = static_cast<QSGFlatColorMaterial *>(bg->material());
        if (mat->color() != m_backgroundColor)
        {
            mat->setColor(m_backgroundColor);
            bg->markDirty(QSGNode::DirtyMaterial);
        }
        bg->markDirty(QSGNode::DirtyGeometry);
    }

    // Compute plot rect (inside margins)
    const float plotX0 = float(m_plotLeftMargin);
    const float plotY0 = float(m_plotTopMargin);
    const float plotW = float(width() - (m_plotLeftMargin + m_plotRightMargin));
    const float plotH = float(height() - (m_plotTopMargin + m_plotBottomMargin));
    if (plotW <= 1 || plotH <= 1)
    {
        return root;
    }

    // Clip node for strict clipping
    QSGClipNode *clip = nullptr;
    if (bg->nextSibling())
    {
        clip = static_cast<QSGClipNode *>(bg->nextSibling());
    }
    if (!clip)
    {
        clip = new QSGClipNode();
        root->appendChildNode(clip);
    }
    clip->setIsRectangular(true);
    const QRectF newClip(plotX0, plotY0, plotW, plotH);
    if (clip->clipRect() != newClip)
    {
        clip->setClipRect(newClip);
    }

    // CRITICAL FIX: Don't try to find existing seriesRoot from clip children
    // After a window change, m_seriesRoot is invalid (points to old context)
    // Always check m_seriesRoot validity and recreate if needed

    QSGNode *seriesRoot = nullptr;

    // Only try to reuse m_seriesRoot if it's actually a child of current clip node
    if (m_seriesRoot)
    {
        // Verify m_seriesRoot is actually in the current scene graph
        bool found = false;
        for (QSGNode *n = clip->firstChild(); n; n = n->nextSibling())
        {
            if (n == m_seriesRoot)
            {
                found = true;
                seriesRoot = m_seriesRoot;
                break;
            }
        }

        if (!found)
        {
            // m_seriesRoot points to old scene graph - clear it
            m_seriesRoot = nullptr;
        }
    }

    if (!seriesRoot)
    {
        // Need to create new seriesRoot or find existing one in clip
        for (QSGNode *n = clip->firstChild(); n; n = n->nextSibling())
        {
            // Skip grid nodes to find series root
            if (n != m_gridNode && n != m_gridMinorNode)
            {
                seriesRoot = n;
                break;
            }
        }

        if (!seriesRoot)
        {
            seriesRoot = new QSGNode();
            clip->appendChildNode(seriesRoot);
        }

        m_seriesRoot = seriesRoot;
    }

    // Same fix for m_seriesStrokeRoot
    QSGNode *seriesStrokeRoot = nullptr;

    if (m_seriesStrokeRoot)
    {
        bool found = false;
        for (QSGNode *n = clip->firstChild(); n; n = n->nextSibling())
        {
            if (n == m_seriesStrokeRoot)
            {
                found = true;
                seriesStrokeRoot = m_seriesStrokeRoot;
                break;
            }
        }

        if (!found)
        {
            m_seriesStrokeRoot = nullptr;
            m_seriesStrokeNodes.clear();  // These are also invalid now
        }
    }

    if (!seriesStrokeRoot)
    {
        seriesStrokeRoot = new QSGNode();
        clip->appendChildNode(seriesStrokeRoot);
        m_seriesStrokeRoot = seriesStrokeRoot;
    }

    // Ensure minor and major grid nodes exist under clip before seriesRoot
    auto ensureGridNode = [&](QSGGeometryNode *&node, const QColor &color)
    {
        if (!node)
        {
            node = new QSGGeometryNode();
            auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            g->setDrawingMode(QSGGeometry::DrawLines);
            node->setGeometry(g);
            node->setFlag(QSGNode::OwnsGeometry);
            auto *mat = new QSGFlatColorMaterial();
            mat->setColor(color);
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial);
            clip->insertChildNodeBefore(node, seriesRoot);
        }
        // Do not reorder once inserted; keep scene graph stable
    };

    auto updateGridLines = [](QSGGeometryNode *node, const QVector<QPointF> &lines, const QColor &color)
    {
        if (!node)
        {
            return;
        }
        auto *g = node->geometry();
        const int count = lines.size();
        if (g->vertexCount() != count)
        {
            g->allocate(count);
        }
        auto *gv = g->vertexDataAsPoint2D();
        for (int i = 0; i < count; ++i)
        {
            gv[i].x = float(lines[i].x());
            gv[i].y = float(lines[i].y());
        }
        auto *mat = static_cast<QSGFlatColorMaterial *>(node->material());
        if (mat->color() != color)
        {
            mat->setColor(color);
            node->markDirty(QSGNode::DirtyMaterial);
        }
        node->markDirty(QSGNode::DirtyGeometry);
    };
    // Ensure minor/major grid nodes exist and are ordered under the clip
    ensureGridNode(m_gridMinorNode, m_gridMinorColor);
    ensureGridNode(m_gridNode, m_gridMajorColor);

    if (!m_markerLinesNode)
    {
        m_markerLinesNode = new QSGNode();
        clip->appendChildNode(m_markerLinesNode);
    }

    // Enforce desired order once: minor -> major -> seriesRoot
    auto enforceOrder = [&]()
    {
        // Find positions
        bool minorBeforeSeries = false;
        bool majorBeforeSeries = false;
        bool markersBeforeSeries = false;
        for (QSGNode *n = clip->firstChild(); n; n = n->nextSibling())
        {
            if (n == m_gridMinorNode)
            {
                minorBeforeSeries = true;
            }
            if (n == m_gridNode)
            {
                majorBeforeSeries = true;
            }
            if (n == m_markerLinesNode)
            {
                markersBeforeSeries = true;
            }
            if (n == seriesRoot)
            {
                // If minor/major not before seriesRoot, move them before
                if (m_gridMinorNode && !minorBeforeSeries)
                {
                    clip->removeChildNode(m_gridMinorNode);
                    clip->insertChildNodeBefore(m_gridMinorNode, seriesRoot);
                }
                if (m_gridNode && !majorBeforeSeries)
                {
                    clip->removeChildNode(m_gridNode);
                    clip->insertChildNodeBefore(m_gridNode, seriesRoot);
                }
                if (m_markerLinesNode && !markersBeforeSeries)
                {
                    clip->removeChildNode(m_markerLinesNode);
                    clip->insertChildNodeBefore(m_markerLinesNode, seriesRoot);
                }
                break;
            }
        }
        // If already ordered, skip any work
        if (minorBeforeSeries && majorBeforeSeries)
        {
            return;
        }
    };
    enforceOrder();

    // Early-out grid updates if plot rect and ranges unchanged to prevent flicker
    bool gridRectChanged = (m_lastPlotX0 != plotX0 || m_lastPlotY0 != plotY0 || m_lastPlotW != plotW || m_lastPlotH != plotH);
    bool gridRangeChanged = (m_lastXMin != m_xMin || m_lastXMax != m_xMax || m_lastYMin != m_yMin || m_lastYMax != m_yMax);
    bool rebuildGrid = gridRectChanged || gridRangeChanged;
    if (rebuildGrid)
    {
        m_lastPlotX0 = plotX0;
        m_lastPlotY0 = plotY0;
        m_lastPlotW = plotW;
        m_lastPlotH = plotH;
        m_lastXMin = m_xMin;
        m_lastXMax = m_xMax;
        m_lastYMin = m_yMin;
        m_lastYMax = m_yMax;
    }
    if (rebuildGrid)
    {
        QVector<QPointF> minorLines;
        QVector<QPointF> majorLines;
        auto appendMinorV = [&](float px)
        {
            minorLines.append(QPointF(plotX0 + px, plotY0));
            minorLines.append(QPointF(plotX0 + px, plotY0 + plotH));
        };
        auto appendMajorV = [&](float px)
        {
            majorLines.append(QPointF(plotX0 + px, plotY0));
            majorLines.append(QPointF(plotX0 + px, plotY0 + plotH));
        };
        auto appendMinorH = [&](float py)
        {
            minorLines.append(QPointF(plotX0, plotY0 + py));
            minorLines.append(QPointF(plotX0 + plotW, plotY0 + py));
        };
        auto appendMajorH = [&](float py)
        {
            majorLines.append(QPointF(plotX0, plotY0 + py));
            majorLines.append(QPointF(plotX0 + plotW, plotY0 + py));
        };

        // Compute majors from labeled ticks if available, otherwise from step
        QVector<double> xMajors;
        QVector<double> yMajors;
        if (!m_labeledXTicks.isEmpty())
        {
            xMajors = m_labeledXTicks;
        }
        else if (m_majorTickStepX > 0)
        {
            // for (double x = start; x <= m_xMax + 1e-9; x += m_majorTickStepX)
            double x = std::ceil(m_xMin / m_majorTickStepX) * m_majorTickStepX;
            while (x <= m_xMax + 1e-9)
            {
                xMajors.append(x);
                x += m_majorTickStepX;
            }
        }
        if (!m_labeledYTicks.isEmpty())
        {
            yMajors = m_labeledYTicks;
        }
        else if (m_majorTickStepY > 0)
        {
            // for (double y = start; y <= m_yMax + 1e-9; y += m_majorTickStepY)
            double y = std::ceil(m_yMin / m_majorTickStepY) * m_majorTickStepY;
            while (y <= m_yMax + 1e-9)
            {
                yMajors.append(y);
                y += m_majorTickStepY;
            }
        }
        for (double x : std::as_const(xMajors))
        {
            if (x < m_xMin || x > m_xMax)
            {
                continue;
            }
            float px = float((x - m_xMin) / (m_xMax - m_xMin) * plotW);
            appendMajorV(px);
        }
        for (double y : std::as_const(yMajors))
        {
            if (y < m_yMin || y > m_yMax)
            {
                continue;
            }
            float py = float(plotH - (y - m_yMin) / (m_yMax - m_yMin) * plotH);
            appendMajorH(py);
        }

        // Compute minors: labeled minors, explicit step, or subdivision count
        if (!m_labeledXMinorTicks.isEmpty())
        {
            for (double x : std::as_const(m_labeledXMinorTicks))
            {
                if (x < m_xMin || x > m_xMax)
                {
                    continue;
                }
                float px = float((x - m_xMin) / (m_xMax - m_xMin) * plotW);
                appendMinorV(px);
            }
        }
        else if (m_minorTickStepX > 0)
        {
            // for (double x = start; x <= m_xMax + 1e-9; x += m_minorTickStepX)
            double x = std::ceil(m_xMin / m_minorTickStepX) * m_minorTickStepX;
            while (x <= m_xMax + 1e-9)
            {
                float px = float((x - m_xMin) / (m_xMax - m_xMin) * plotW);
                appendMinorV(px);
                x += m_minorTickStepX;
            }
        }
        else if (!xMajors.isEmpty())
        {
            int sections = (m_minorSectionsX > 0 ? m_minorSectionsX : 5);
            for (int i = 0; i < xMajors.size() - 1; ++i)
            {
                double a = xMajors[i], b = xMajors[i + 1];
                double stepMinor = (b - a) / sections;
                for (int k = 1; k < sections; ++k)
                {
                    double xm = a + k * stepMinor;
                    if (xm < m_xMin || xm > m_xMax)
                    {
                        continue;
                    }
                    float px = float((xm - m_xMin) / (m_xMax - m_xMin) * plotW);
                    appendMinorV(px);
                }
            }
        }
        if (!m_labeledYMinorTicks.isEmpty())
        {
            for (double y : std::as_const(m_labeledYMinorTicks))
            {
                if (y < m_yMin || y > m_yMax)
                {
                    continue;
                }
                float py = float(plotH - (y - m_yMin) / (m_yMax - m_yMin) * plotH);
                appendMinorH(py);
            }
        }
        else if (m_minorTickStepY > 0)
        {
            // for (double y = start; y <= m_yMax + 1e-9; y += m_minorTickStepY)
            double y = std::ceil(m_yMin / m_minorTickStepY) * m_minorTickStepY;
            while (y <= m_yMax + 1e-9)
            {
                float py = float(plotH - (y - m_yMin) / (m_yMax - m_yMin) * plotH);
                appendMinorH(py);
                y += m_minorTickStepY;
            }
        }
        else if (!yMajors.isEmpty())
        {
            int sections = (m_minorSectionsY > 0 ? m_minorSectionsY : 5);
            for (int i = 0; i < yMajors.size() - 1; ++i)
            {
                double a = yMajors[i], b = yMajors[i + 1];
                double stepMinor = (b - a) / sections;
                for (int k = 1; k < sections; ++k)
                {
                    double ym = a + k * stepMinor;
                    if (ym < m_yMin || ym > m_yMax)
                    {
                        continue;
                    }
                    float py = float(plotH - (ym - m_yMin) / (m_yMax - m_yMin) * plotH);
                    appendMinorH(py);
                }
            }
        }
        // Update grid nodes
        updateGridLines(m_gridMinorNode, minorLines, m_gridMinorColor);
        updateGridLines(m_gridNode, majorLines, m_gridMajorColor);
    }

    // Axes rectangle
    if (!m_axesNode)
    {
        m_axesNode = new QSGGeometryNode();
        auto *a = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 8);
        a->setDrawingMode(QSGGeometry::DrawLines);
        m_axesNode->setGeometry(a);
        m_axesNode->setFlag(QSGNode::OwnsGeometry);
        auto *mat = new QSGFlatColorMaterial();
        mat->setColor(m_axisLineColor);
        m_axesNode->setMaterial(mat);
        m_axesNode->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(m_axesNode);
    }
    {
        // Early-out axes when rect and color unchanged
        static float lastAxX0 = -1.0f, lastAxY0 = -1.0f, lastAxW = -1.0f, lastAxH = -1.0f;
        static QColor lastAxColor;
        bool axesRectChanged = (lastAxX0 != plotX0 || lastAxY0 != plotY0 || lastAxW != plotW || lastAxH != plotH);
        bool axesColorChanged = (lastAxColor != m_axisLineColor);
        if (!(axesRectChanged || axesColorChanged))
        {
            // nothing to update
        }
        else
        {
            lastAxX0 = plotX0;
            lastAxY0 = plotY0;
            lastAxW = plotW;
            lastAxH = plotH;
            lastAxColor = m_axisLineColor;
            auto *a = m_axesNode->geometry();
            if (a->vertexCount() != 8)
            {
                a->allocate(8);
            }
            auto *axv = a->vertexDataAsPoint2D();
            axv[0] = {plotX0, plotY0};
            axv[1] = {plotX0 + plotW, plotY0};
            axv[2] = {plotX0 + plotW, plotY0};
            axv[3] = {plotX0 + plotW, plotY0 + plotH};
            axv[4] = {plotX0 + plotW, plotY0 + plotH};
            axv[5] = {plotX0, plotY0 + plotH};
            axv[6] = {plotX0, plotY0 + plotH};
            axv[7] = {plotX0, plotY0};
            auto *mat = static_cast<QSGFlatColorMaterial *>(m_axesNode->material());
            if (mat->color() != m_axisLineColor)
            {
                mat->setColor(m_axisLineColor);
                m_axesNode->markDirty(QSGNode::DirtyMaterial);
            }
            m_axesNode->markDirty(QSGNode::DirtyGeometry);
        }
    }

    // ========================================================================
    // Render Marker Lines (above grid, below series)
    // ========================================================================
    {
        // Clean up existing marker line nodes
        while (QSGNode *n = m_markerLinesNode->firstChild())
        {
            m_markerLinesNode->removeChildNode(n);
            delete n;
        }

        // Render each visible marker line
        for (const auto *marker : std::as_const(m_markerLines))
        {
            if (!marker->visible())
            {
                continue;
            }

            const double pos = marker->position();

            // Check if line is within visible range
            if (marker->isVertical())
            {
                if (pos < m_xMin || pos > m_xMax)
                {
                    continue;
                }
            }
            else
            {
                if (pos < m_yMin || pos > m_yMax)
                {
                    continue;
                }
            }

            const float lineWidth = qMax(0.5f, static_cast<float>(marker->width()));
            const Qt::PenStyle penStyle = static_cast<Qt::PenStyle>(marker->style());
            const QColor lineColor = marker->color();

            // Determine line position and dimensions
            float x1, y1, x2, y2;
            if (marker->isVertical())
            {
                // Vertical line at X position
                const float px = plotX0 + float((pos - m_xMin) / (m_xMax - m_xMin) * plotW);
                x1 = px;
                y1 = plotY0;
                x2 = px;
                y2 = plotY0 + plotH;
            }
            else
            {
                // Horizontal line at Y position
                const float py = plotY0 + float(plotH - (pos - m_yMin) / (m_yMax - m_yMin) * plotH);
                x1 = plotX0;
                y1 = py;
                x2 = plotX0 + plotW;
                y2 = py;
            }

            // For solid lines, draw as a single filled rectangle (or optimized line for 1px)
            if (penStyle == Qt::SolidLine)
            {
                // Optimize: for 1px solid lines, use simple line drawing (2 vertices vs 4)
                if (lineWidth <= 1.0f)
                {
                    auto *node = new QSGGeometryNode();
                    auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 2);
                    geom->setDrawingMode(QSGGeometry::DrawLines);
                    geom->setLineWidth(1.0f);
                    node->setGeometry(geom);
                    node->setFlag(QSGNode::OwnsGeometry);

                    auto *mat = new QSGFlatColorMaterial();
                    mat->setColor(lineColor);
                    node->setMaterial(mat);
                    node->setFlag(QSGNode::OwnsMaterial);

                    auto *v = geom->vertexDataAsPoint2D();
                    v[0].set(x1, y1);
                    v[1].set(x2, y2);

                    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    m_markerLinesNode->appendChildNode(node);
                }
                else
                {
                    // Draw as a filled rectangle (triangle strip) for width > 1px
                    auto *node = new QSGGeometryNode();
                    auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                    geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                    node->setGeometry(geom);
                    node->setFlag(QSGNode::OwnsGeometry);

                    auto *mat = new QSGFlatColorMaterial();
                    mat->setColor(lineColor);
                    node->setMaterial(mat);
                    node->setFlag(QSGNode::OwnsMaterial);

                    auto *v = geom->vertexDataAsPoint2D();

                    if (marker->isVertical())
                    {
                        // Vertical rectangle
                        const float halfWidth = lineWidth * 0.5f;
                        v[0].set(x1 - halfWidth, y1);
                        v[1].set(x1 + halfWidth, y1);
                        v[2].set(x1 - halfWidth, y2);
                        v[3].set(x1 + halfWidth, y2);
                    }
                    else
                    {
                        // Horizontal rectangle
                        const float halfWidth = lineWidth * 0.5f;
                        v[0].set(x1, y1 - halfWidth);
                        v[1].set(x2, y1 - halfWidth);
                        v[2].set(x1, y1 + halfWidth);
                        v[3].set(x2, y1 + halfWidth);
                    }

                    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    m_markerLinesNode->appendChildNode(node);
                }
            }
            else
            {
                // For dashed/dotted styles with thin lines, tessellate the pattern
                const float lineLength = marker->isVertical() ? (y2 - y1) : (x2 - x1);

                // Define dash patterns (in pixels)
                float dashLength = 5.0f;
                float gapLength = 5.0f;

                switch (penStyle)
                {
                    case Qt::DashLine:
                        dashLength = 10.0f;
                        gapLength = 5.0f;
                        break;
                    case Qt::DotLine:
                        dashLength = 2.0f;
                        gapLength = 3.0f;
                        break;
                    case Qt::DashDotLine:
                        // We'll alternate between dash and dot
                        dashLength = 10.0f;
                        gapLength = 3.0f;
                        break;
                    case Qt::DashDotDotLine:
                        dashLength = 10.0f;
                        gapLength = 3.0f;
                        break;
                    default:
                        // Fallback to solid
                        dashLength = lineLength;
                        gapLength = 0.0f;
                        break;
                }

                // Calculate dash segments
                QVector<QPair<float, float>> segments;
                float pos = 0.0f;
                bool isDash = true;
                bool isDashDotPattern = (penStyle == Qt::DashDotLine || penStyle == Qt::DashDotDotLine);
                int dotCount = 0;

                while (pos < lineLength)
                {
                    if (isDash)
                    {
                        float segLength = dashLength;

                        // For DashDot patterns, alternate between dash and dots
                        if (isDashDotPattern && dotCount > 0)
                        {
                            segLength = 2.0f;  // dot length
                            dotCount--;
                            if (dotCount == 0)
                            {
                                isDash = true;  // Back to dash
                            }
                        }

                        const float endPos = qMin(pos + segLength, lineLength);
                        segments.append(qMakePair(pos, endPos));
                        pos = endPos;
                        isDash = false;

                        // Set up dot sequence for DashDot patterns
                        if (isDashDotPattern && dotCount == 0)
                        {
                            dotCount = (penStyle == Qt::DashDotDotLine) ? 2 : 1;
                            isDash = false;  // Next will be dots
                        }
                    }
                    else
                    {
                        pos += gapLength;
                        isDash = true;
                    }
                }

                // Draw each dash segment as a thin rectangle
                for (const auto &seg : segments)
                {
                    auto *node = new QSGGeometryNode();
                    auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
                    geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
                    node->setGeometry(geom);
                    node->setFlag(QSGNode::OwnsGeometry);

                    auto *mat = new QSGFlatColorMaterial();
                    mat->setColor(lineColor);
                    node->setMaterial(mat);
                    node->setFlag(QSGNode::OwnsMaterial);

                    auto *v = geom->vertexDataAsPoint2D();

                    if (marker->isVertical())
                    {
                        const float halfWidth = lineWidth * 0.5f;
                        const float segY1 = y1 + seg.first;
                        const float segY2 = y1 + seg.second;
                        v[0].set(x1 - halfWidth, segY1);
                        v[1].set(x1 + halfWidth, segY1);
                        v[2].set(x1 - halfWidth, segY2);
                        v[3].set(x1 + halfWidth, segY2);
                    }
                    else
                    {
                        const float halfWidth = lineWidth * 0.5f;
                        const float segX1 = x1 + seg.first;
                        const float segX2 = x1 + seg.second;
                        v[0].set(segX1, y1 - halfWidth);
                        v[1].set(segX2, y1 - halfWidth);
                        v[2].set(segX1, y1 + halfWidth);
                        v[3].set(segX2, y1 + halfWidth);
                    }

                    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                    m_markerLinesNode->appendChildNode(node);
                }
            }
        }
    }

    // Ensure one geometry node per series
    int existing = 0;
    for (QSGNode *n = seriesRoot->firstChild(); n; n = n->nextSibling())
    {
        existing++;
    }
    int needed = 0;
    for (auto *s : std::as_const(m_series))
    {
        if (s->visible())
        {
            needed++;
        }
    }
    while (existing < needed)
    {
        auto *gn = new QSGGeometryNode();
        auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        g->setDrawingMode(QSGGeometry::DrawTriangles);
        gn->setGeometry(g);
        gn->setFlag(QSGNode::OwnsGeometry);
        auto *m = new QSGFlatColorMaterial();
        gn->setMaterial(m);
        gn->setFlag(QSGNode::OwnsMaterial);
        seriesRoot->appendChildNode(gn);
        existing++;
    }
    while (existing > needed)
    {
        QSGNode *last = seriesRoot->lastChild();
        seriesRoot->removeChildNode(last);
        delete last;
        existing--;
    }

    // Rendering
    const double xSpan = (m_xMax - m_xMin);
    const double ySpan = (m_yMax - m_yMin);
    int si = 0;
#ifdef ENABLE_MEM_INSTRUMENT
    int totalVertices = 0;
    int totalSeries = 0;
#endif
    for (auto *s : std::as_const(m_series))
    {
        if (!s->visible())
        {
            continue;
        }
        QSGGeometryNode *node = static_cast<QSGGeometryNode *>(seriesRoot->childAtIndex(si));
        auto it = m_buffers.find(s->seriesId());
        const auto &data = (it != m_buffers.end()) ? it.value().data : QVector<QPointF>{};
        if (data.size() < 2 || xSpan <= 0.0 || ySpan <= 0.0)
        {
            node->geometry()->allocate(0);
            node->markDirty(QSGNode::DirtyGeometry);
            si++;
            continue;
        }
        QVector<QPointF> use;
        // Skip decimation for Impulse style - each bar is independent and must be preserved
        if (m_decimationEnabled && s->style() != LineSeries::Impulse)
        {
            auto &rc = m_renderCache[s->seriesId()];
            int gen = m_buffers[s->seriesId()].generation;
            if (rc.xMin != m_xMin || rc.xMax != m_xMax || rc.plotW != int(plotW) || rc.dataSize != data.size() || rc.generation != gen)
            {
                rc.decimated = decimateEnvelope(data, int(plotW), m_xMin, m_xMax);
                rc.xMin = m_xMin;
                rc.xMax = m_xMax;
                rc.plotW = int(plotW);
                rc.dataSize = data.size();
                rc.generation = gen;
            }
            use = rc.decimated;
        }
        else
        {
            // Apply boundary-aware filtering for non-decimated or Impulse series
            use = filterVisibleRange(data, m_xMin, m_xMax);
        }

        auto *geom = node->geometry();
        auto *mat = static_cast<QSGFlatColorMaterial *>(node->material());

        const auto mapX = [&](double x)
        {
            return plotX0 + float((x - m_xMin) / xSpan * plotW);
        };
        const auto mapY = [&](double y)
        {
            return plotY0 + float(plotH - (y - m_yMin) / ySpan * plotH);
        };

        if (s->style() == LineSeries::Impulse)
        {
            const double by = (std::isnan(s->baselineY()) ? m_baselineY : s->baselineY());
            const float yZero = mapY(by);
            const float halfW = float(std::max<qreal>(1.0, s->width()) * 0.5);
            geom->setDrawingMode(QSGGeometry::DrawTriangles);
            {
                const int count = use.size() * 6;
                if (geom->vertexCount() != count)
                {
                    geom->allocate(count);
                }
            }
            auto *v = geom->vertexDataAsPoint2D();
            int j = 0;
            for (const auto &p : std::as_const(use))
            {
                float x = mapX(p.x());
                float y = mapY(p.y());
                float xl = x - halfW;
                float xr = x + halfW;
                v[j++] = {xl, y};
                v[j++] = {xr, y};
                v[j++] = {xl, yZero};
                v[j++] = {xl, yZero};
                v[j++] = {xr, y};
                v[j++] = {xr, yZero};
            }
            if (mat->color() != s->color())
            {
                mat->setColor(s->color());
                node->markDirty(QSGNode::DirtyMaterial);
            }
        }
        else if (s->style() == LineSeries::Filled || s->style() == LineSeries::FilledWithStroke)
        {
            const double by = (std::isnan(s->baselineY()) ? m_baselineY : s->baselineY());
            const float yZero = mapY(by);
            geom->setDrawingMode(QSGGeometry::DrawTriangles);
            {
                const int count = (use.size() - 1) * 6;
                if (geom->vertexCount() != count)
                {
                    geom->allocate(count);
                }
            }
            auto *v = geom->vertexDataAsPoint2D();
            int j = 0;
            for (int i = 0; i < use.size() - 1; ++i)
            {
                const QPointF &pa = use[i];
                const QPointF &pb = use[i + 1];
                float ax = mapX(pa.x());
                float ay = mapY(pa.y());
                float bx = mapX(pb.x());
                float bys = mapY(pb.y());
                v[j++] = {ax, ay};
                v[j++] = {bx, bys};
                v[j++] = {ax, yZero};
                v[j++] = {ax, yZero};
                v[j++] = {bx, bys};
                v[j++] = {bx, yZero};
            }
            {
                const QColor fc = (s->fillColor().isValid() ? s->fillColor() : s->color());
                if (mat->color() != fc)
                {
                    mat->setColor(fc);
                    node->markDirty(QSGNode::DirtyMaterial);
                }
            }
            // If style requests stroke overlay, render an additional node above
            if (s->style() == LineSeries::FilledWithStroke)
            {
                QSGGeometryNode *strokeNode = m_seriesStrokeNodes.value(s->seriesId(), nullptr);
                if (!strokeNode)
                {
                    // Create stroke node parented under the dedicated clipped stroke root
                    if (m_seriesStrokeRoot)
                    {
                        strokeNode = new QSGGeometryNode();
                        auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
                        g->setLineWidth(std::max<qreal>(1.0, s->width()));
                        g->setVertexDataPattern(QSGGeometry::DynamicPattern);
                        strokeNode->setGeometry(g);
                        auto *m = new QSGFlatColorMaterial();
                        strokeNode->setMaterial(m);
                        m_seriesStrokeNodes.insert(s->seriesId(), strokeNode);
                        m_seriesStrokeRoot->appendChildNode(strokeNode);
                    }
                    else
                    {
                        // Series root not ready yet; skip stroke rendering this frame
                    }
                }
                auto *sGeom = strokeNode->geometry();
                auto *sMat = static_cast<QSGFlatColorMaterial *>(strokeNode->material());
                if (s->width() <= 1.0)
                {
                    sGeom->setDrawingMode(QSGGeometry::DrawLineStrip);
                    const int count = use.size();
                    if (sGeom->vertexCount() != count)
                    {
                        sGeom->allocate(count);
                    }
                    auto *sv = sGeom->vertexDataAsPoint2D();
                    int sj = 0;
                    for (const auto &p : std::as_const(use))
                    {
                        sv[sj++] = {mapX(p.x()), mapY(p.y())};
                    }
                }
                else
                {
                    // Thick stroke via quads, similar to branch below
                    QVector<QPointF> pts;
                    pts.reserve(use.size());
                    for (const auto &p : std::as_const(use))
                    {
                        pts.append(QPointF(mapX(p.x()), mapY(p.y())));
                    }
                    int segCount = 0;
                    for (int i = 0; i < pts.size() - 1; ++i)
                    {
                        QPointF a = pts[i], b = pts[i + 1];
                        float dx = b.x() - a.x(), dy = b.y() - a.y();
                        float len = std::sqrt(dx * dx + dy * dy);
                        if (len > 1e-3f)
                        {
                            segCount++;
                        }
                    }
                    sGeom->setDrawingMode(QSGGeometry::DrawTriangles);
                    const int count = segCount * 6;
                    if (sGeom->vertexCount() != count)
                    {
                        sGeom->allocate(count);
                    }
                    auto *sv = sGeom->vertexDataAsPoint2D();
                    const float halfW = float(s->width() * 0.5f);
                    int sj = 0;
                    auto perp = [&](float dx, float dy)
                    {
                        float len = std::sqrt(dx * dx + dy * dy);
                        if (len <= 1e-3f)
                        {
                            return QPointF(0, 0);
                        }
                        return QPointF(-dy / len * halfW, dx / len * halfW);
                    };
                    for (int i = 0; i < pts.size() - 1; ++i)
                    {
                        QPointF a = pts[i], b = pts[i + 1];
                        float dx = b.x() - a.x(), dy = b.y() - a.y();
                        float len = std::sqrt(dx * dx + dy * dy);
                        if (len <= 1e-3f)
                        {
                            continue;
                        }
                        QPointF n = perp(dx, dy);
                        QPointF a1(a.x() + n.x(), a.y() + n.y()), a2(a.x() - n.x(), a.y() - n.y());
                        QPointF b1(b.x() + n.x(), b.y() + n.y()), b2(b.x() - n.x(), b.y() - n.y());
                        sv[sj++] = {float(a1.x()), float(a1.y())};
                        sv[sj++] = {float(b1.x()), float(b1.y())};
                        sv[sj++] = {float(a2.x()), float(a2.y())};
                        sv[sj++] = {float(a2.x()), float(a2.y())};
                        sv[sj++] = {float(b1.x()), float(b1.y())};
                        sv[sj++] = {float(b2.x()), float(b2.y())};
                    }
                }
                if (strokeNode)
                {
                    if (sMat->color() != s->color())
                    {
                        sMat->setColor(s->color());
                        strokeNode->markDirty(QSGNode::DirtyMaterial);
                    }
                    strokeNode->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
                }
            }
        }
        else if (s->width() <= 1.0)
        {
            geom->setDrawingMode(QSGGeometry::DrawLineStrip);
            {
                const int count = use.size();
                if (geom->vertexCount() != count)
                {
                    geom->allocate(count);
                }
            }
            auto *v = geom->vertexDataAsPoint2D();
            int j = 0;
            for (const auto &p : std::as_const(use))
            {
                v[j++] = {mapX(p.x()), mapY(p.y())};
            }
            if (mat->color() != s->color())
            {
                mat->setColor(s->color());
                node->markDirty(QSGNode::DirtyMaterial);
            }
        }
        else
        {
            // Thick stroke via quads
            QVector<QPointF> pts;
            pts.reserve(use.size());
            for (const auto &p : std::as_const(use))
            {
                pts.append(QPointF(mapX(p.x()), mapY(p.y())));
            }
            int segCount = 0;
            for (int i = 0; i < pts.size() - 1; ++i)
            {
                QPointF a = pts[i], b = pts[i + 1];
                float dx = b.x() - a.x(), dy = b.y() - a.y();
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 1e-3f)
                {
                    segCount++;
                }
            }
            geom->setDrawingMode(QSGGeometry::DrawTriangles);
            {
                const int count = segCount * 6;
                if (geom->vertexCount() != count)
                {
                    geom->allocate(count);
                }
            }
            auto *v = geom->vertexDataAsPoint2D();
            const float halfW = float(s->width() * 0.5f);
            int j = 0;
            auto perp = [&](float dx, float dy)
            {
                float len = std::sqrt(dx * dx + dy * dy);
                if (len <= 1e-3f)
                {
                    return QPointF(0, 0);
                }
                return QPointF(-dy / len * halfW, dx / len * halfW);
            };
            for (int i = 0; i < pts.size() - 1; ++i)
            {
                QPointF a = pts[i], b = pts[i + 1];
                float dx = b.x() - a.x(), dy = b.y() - a.y();
                float len = std::sqrt(dx * dx + dy * dy);
                if (len <= 1e-3f)
                {
                    continue;
                }
                QPointF n = perp(dx, dy);
                QPointF a1(a.x() + n.x(), a.y() + n.y()), a2(a.x() - n.x(), a.y() - n.y());
                QPointF b1(b.x() + n.x(), b.y() + n.y()), b2(b.x() - n.x(), b.y() - n.y());
                v[j++] = {float(a1.x()), float(a1.y())};
                v[j++] = {float(b1.x()), float(b1.y())};
                v[j++] = {float(a2.x()), float(a2.y())};
                v[j++] = {float(a2.x()), float(a2.y())};
                v[j++] = {float(b1.x()), float(b1.y())};
                v[j++] = {float(b2.x()), float(b2.y())};
            }
            if (mat->color() != s->color())
            {
                mat->setColor(s->color());
                node->markDirty(QSGNode::DirtyMaterial);
            }
        }
        // Geometry updated above; only mark geometry dirty here.
        node->markDirty(QSGNode::DirtyGeometry);
#ifdef ENABLE_MEM_INSTRUMENT
        totalVertices += node->geometry()->vertexCount();
        totalSeries++;
#endif
        si++;
    }

#ifdef ENABLE_MEM_INSTRUMENT
    qInfo().nospace() << "LineChartItem frame: series=" << totalSeries << " vertices=" << totalVertices << " plotW=" << int(width())
                      << " plotH=" << int(height());
#endif

    return root;
}

// ============================================================================
// OPTIMIZATION: Filter data for visible range with boundary points
// ============================================================================
QVector<QPointF> LineChartItem::filterVisibleRange(const QVector<QPointF> &data, double xMin, double xMax) const
{
    if (data.size() < 2)
    {
        return data;
    }

    // Find first point at or before visible range
    int start = 0;
    while (start < data.size() && data[start].x() < xMin)
    {
        start++;
    }

    // Back up one point to include boundary connection
    if (start > 0)
    {
        start--;
    }

    // Find last point at or after visible range
    int end = start;
    while (end < data.size() && data[end].x() <= xMax)
    {
        end++;
    }

    // Include one point beyond xMax for boundary connection
    if (end < data.size())
    {
        end++;
    }

    if (start >= end)
    {
        return data;
    }

    return data.mid(start, end - start);
}

// ============================================================================
// OPTIMIZATION: Enhanced decimation with envelope preservation
// ============================================================================
QVector<QPointF> LineChartItem::decimateEnvelope(const QVector<QPointF> &data, int widthPx, double xMin, double xMax) const
{
    if (widthPx <= 0 || data.size() < 2)
    {
        return data;
    }

    const double xSpan = xMax - xMin;
    if (xSpan <= 0)
    {
        return data;
    }

    // OPTIMIZATION: Pre-allocate with estimated size
    QVector<QPointF> out;
    out.reserve(widthPx * 2);

    // Find first point before or at visible range start
    // CRITICAL: Include one point before xMin to ensure line extends to left edge
    int i = 0;
    while (i < data.size() && data[i].x() < xMin)
    {
        i++;
    }

    // Back up one point if available to include boundary connection
    if (i > 0)
    {
        i--;
    }

    int lastPixel = -1;
    double minY = 0, maxY = 0;
    double minX = 0, maxX = 0;
    bool have = false;

    for (; i < data.size(); ++i)
    {
        const auto &p = data[i];

        // CRITICAL: Don't break immediately when exceeding xMax
        // Process one more point to ensure line extends to right edge
        const bool beyondMax = (p.x() > xMax);

        int px = int(((p.x() - xMin) / xSpan) * widthPx);

        if (px != lastPixel)
        {
            if (have)
            {
                // Push min then max to preserve spikes
                out.append(QPointF(minX, minY));
                if (maxX != minX || maxY != minY)
                {
                    out.append(QPointF(maxX, maxY));
                }
            }
            lastPixel = px;
            minY = maxY = p.y();
            minX = maxX = p.x();
            have = true;

            // Break after processing first point beyond xMax
            if (beyondMax)
            {
                break;
            }
        }
        else
        {
            if (p.y() < minY)
            {
                minY = p.y();
                minX = p.x();
            }
            if (p.y() > maxY)
            {
                maxY = p.y();
                maxX = p.x();
            }

            // Break after processing first point beyond xMax
            if (beyondMax)
            {
                break;
            }
        }
    }

    if (have)
    {
        out.append(QPointF(minX, minY));
        if (maxX != minX || maxY != minY)
        {
            out.append(QPointF(maxX, maxY));
        }
    }

    return out.isEmpty() ? data : out;
}

// ============================================================================
// Scene change handler - invalidate cached nodes
// ============================================================================
void LineChartItem::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemSceneChange)
    {
        if (m_window)
        {
            disconnect(m_window, nullptr, this, nullptr);
        }

        m_window = value.window;

        if (m_window)
        {
            connect(
                m_window, &QQuickWindow::sceneGraphInvalidated, this,
                [this]()
                {
                    m_rebuild = true;
                    releaseResources();
                },
                Qt::DirectConnection);
        }

        // Mark for full rebuild
        m_rebuild = true;
        releaseResources();

        // Request update in new window context
        if (m_window)
        {
            update();
        }
    }
    // else if (change == ItemVisibleHasChanged)
    // {
    //     if (isVisible())
    //     {
    //         m_rebuild = true;
    //         releaseResources();
    //         update();
    //     }
    // }
    QQuickItem::itemChange(change, value);
}

void LineChartItem::releaseResources()
{
    // This is called when the item is removed from a window
    // or when the scene graph is invalidated
    m_rebuild = true;
    m_gridMinorNode = nullptr;
    m_gridNode = nullptr;
    m_markerLinesNode = nullptr;
    m_axesNode = nullptr;
    m_seriesRoot = nullptr;
    m_seriesStrokeRoot = nullptr;
    m_seriesStrokeNodes.clear();
    m_renderCache.clear();

    // Reset cache
    m_lastPlotX0 = -1.0f;
    m_lastPlotY0 = -1.0f;
    m_lastPlotW = -1.0f;
    m_lastPlotH = -1.0f;
    m_lastXMin = std::numeric_limits<double>::quiet_NaN();
    m_lastXMax = std::numeric_limits<double>::quiet_NaN();
    m_lastYMin = std::numeric_limits<double>::quiet_NaN();
    m_lastYMax = std::numeric_limits<double>::quiet_NaN();
}

// Continued in next artifact with recomputeTicks() and updatePaintNode()...

// OPTIMIZATION: Enhanced recomputeTicks with pre-allocation and caching
void LineChartItem::recomputeTicks()
{
    // Use sorted ranges to avoid transient inversions
    const double x0 = std::min(m_xMin, m_xMax);
    const double x1 = std::max(m_xMin, m_xMax);
    const double y0 = std::min(m_yMin, m_yMax);
    const double y1 = std::max(m_yMin, m_yMax);

    // OPTIMIZATION: Pre-allocate vectors
    const int estimatedXTicks = (m_majorTickStepX > 0) ? int((x1 - x0) / m_majorTickStepX) + 2 : 10;
    const int estimatedYTicks = (m_majorTickStepY > 0) ? int((y1 - y0) / m_majorTickStepY) + 2 : 10;

    m_labeledXTicks.clear();
    m_labeledXTicks.reserve(estimatedXTicks);
    m_labeledYTicks.clear();
    m_labeledYTicks.reserve(estimatedYTicks);
    m_labeledXMinorTicks.clear();
    m_labeledYMinorTicks.clear();
    m_labeledXTickLabels.clear();
    m_labeledYTickLabels.clear();
    m_xLabelPx.clear();
    m_yLabelPx.clear();

    // ========================================================================
    // X axis major ticks
    // ========================================================================
    if (m_majorTickStepX > 0.0)
    {
        const double step = m_majorTickStepX;
        double v = std::floor(x0 / step) * step;
        while (v <= x1 + 1e-12)
        {
            if (v >= x0 - 1e-12)
            {
                m_labeledXTicks.append(v);
            }
            v += step;
        }
    }

    // ========================================================================
    // Y axis major ticks
    // ========================================================================
    if (m_majorTickStepY > 0.0)
    {
        const double step = m_majorTickStepY;
        double v = std::floor(y0 / step) * step;
        while (v <= y1 + 1e-12)
        {
            if (v >= y0 - 1e-12)
            {
                m_labeledYTicks.append(v);
            }
            v += step;
        }
    }

    // ========================================================================
    // X axis minor ticks
    // ========================================================================
    if (m_minorTickStepX > 0.0)
    {
        const double step = m_minorTickStepX;
        double v = std::floor(x0 / step) * step;
        while (v <= x1 + 1e-12)
        {
            if (v >= x0 - 1e-12)
            {
                m_labeledXMinorTicks.append(v);
            }
            v += step;
        }
    }
    else if (m_minorSectionsX > 0 && m_majorTickStepX > 0.0)
    {
        const int sections = m_minorSectionsX;
        const double majorStep = m_majorTickStepX;
        QVector<double> majors = m_labeledXTicks;

        if (majors.isEmpty())
        {
            double v = std::floor(x0 / majorStep) * majorStep;
            // for (double v = start; v <= x1 + 1e-12; v += majorStep)
            while (v <= x1 + 1e-12)
            {
                if (v >= x0 - 1e-12)
                {
                    majors.append(v);
                }
                v += majorStep;
            }
        }

        for (int i = 0; i + 1 < majors.size(); ++i)
        {
            const double a = majors[i];
            const double b = majors[i + 1];
            const double interval = (b - a) / sections;
            for (int s = 1; s < sections; ++s)
            {
                const double v = a + s * interval;
                if (v >= x0 - 1e-12 && v <= x1 + 1e-12)
                {
                    m_labeledXMinorTicks.append(v);
                }
            }
        }
    }

    // ========================================================================
    // Y axis minor ticks
    // ========================================================================
    if (m_minorTickStepY > 0.0)
    {
        const double step = m_minorTickStepY;
        double v = std::floor(y0 / step) * step;
        while (v <= y1 + 1e-12)
        {
            if (v >= y0 - 1e-12)
            {
                m_labeledYMinorTicks.append(v);
            }
            v += step;
        }
    }
    else if (m_minorSectionsY > 0 && m_majorTickStepY > 0.0)
    {
        const int sections = m_minorSectionsY;
        const double majorStep = m_majorTickStepY;
        QVector<double> majors = m_labeledYTicks;

        if (majors.isEmpty())
        {
            double v = std::floor(y0 / majorStep) * majorStep;
            while (v <= y1 + 1e-12)
            {
                if (v >= y0 - 1e-12)
                {
                    majors.append(v);
                }
                v += majorStep;
            }
        }

        for (int i = 0; i + 1 < majors.size(); ++i)
        {
            const double a = majors[i];
            const double b = majors[i + 1];
            const double interval = (b - a) / sections;
            for (int s = 1; s < sections; ++s)
            {
                const double v = a + s * interval;
                if (v >= y0 - 1e-12 && v <= y1 + 1e-12)
                {
                    m_labeledYMinorTicks.append(v);
                }
            }
        }
    }

    // ========================================================================
    // OPTIMIZATION: Pre-format all labels
    // ========================================================================
    QVector<QString> xLabelsAll;
    xLabelsAll.reserve(m_labeledXTicks.size());
    for (double x : std::as_const(m_labeledXTicks))
    {
        xLabelsAll.append(formatXLabel(x));
    }

    QVector<QString> yLabelsAll;
    yLabelsAll.reserve(m_labeledYTicks.size());
    for (double y : std::as_const(m_labeledYTicks))
    {
        yLabelsAll.append(formatYLabel(y));
    }

    // ========================================================================
    // Compute pixel positions and cull for spacing
    // ========================================================================
    const float plotX0 = float(m_plotLeftMargin);
    const float plotY0 = float(m_plotTopMargin);
    const float plotW = float(width() - (m_plotLeftMargin + m_plotRightMargin));
    const float plotH = float(height() - (m_plotTopMargin + m_plotBottomMargin));

    if (plotW > 1 && plotH > 1 && (x1 > x0) && (y1 > y0))
    {
        const float minXSpacing = 20.0f + 4 * m_xLabelDecimals;
        const float minYSpacing = 18.0f;
        constexpr qsizetype maxLabels = 64;

        // ====================================================================
        // X labels with zero-aware alternating selection
        // ====================================================================
        float lastPx = -1e9f;
        int used = 0;
        QVector<QString> keptXLabels;
        keptXLabels.reserve(std::min(maxLabels, m_labeledXTicks.size()));

        auto mapXPx = [&](double xx)
        {
            return plotX0 + float((xx - x0) / (x1 - x0) * plotW);
        };
        auto withinPlotX = [&](float px)
        {
            return !(px < plotX0 - 1 || px > plotX0 + plotW + 1);
        };

        // Find zero index
        int zeroIdxX = -1;
        for (int i = 0; i < m_labeledXTicks.size(); ++i)
        {
            if (std::fabs(m_labeledXTicks[i]) <= 1e-12)
            {
                zeroIdxX = i;
                break;
            }
        }

        auto tryAppendIdxX = [&](int i) -> bool
        {
            if (i < 0 || i >= m_labeledXTicks.size() || i >= xLabelsAll.size())
            {
                return false;
            }
            double xx = m_labeledXTicks[i];
            if (xx < x0 || xx > x1)
            {
                return false;
            }
            float px = mapXPx(xx);
            if (!withinPlotX(px))
            {
                return false;
            }
            if (px - lastPx < minXSpacing)
            {
                return false;
            }
            m_xLabelPx.append(px);
            keptXLabels.append(xLabelsAll[i]);
            lastPx = px;
            used++;
            return true;
        };

        if (zeroIdxX >= 0)
        {
            // Add 0.0 first
            tryAppendIdxX(zeroIdxX);
            float lastPxPos = lastPx;
            float lastPxNeg = lastPx;

            auto tryAppendIdxPosX = [&](int i) -> bool
            {
                if (i < 0 || i >= m_labeledXTicks.size() || i >= xLabelsAll.size())
                {
                    return false;
                }
                double xx = m_labeledXTicks[i];
                if (xx <= 0.0 || xx < x0 || xx > x1)
                {
                    return false;
                }
                float px = mapXPx(xx);
                if (!withinPlotX(px) || px - lastPxPos < minXSpacing)
                {
                    return false;
                }
                m_xLabelPx.append(px);
                keptXLabels.append(xLabelsAll[i]);
                lastPxPos = px;
                used++;
                return true;
            };

            auto tryAppendIdxNegX = [&](int i) -> bool
            {
                if (i < 0 || i >= m_labeledXTicks.size() || i >= xLabelsAll.size())
                {
                    return false;
                }
                double xx = m_labeledXTicks[i];
                if (xx >= 0.0 || xx < x0 || xx > x1)
                {
                    return false;
                }
                float px = mapXPx(xx);
                if (!withinPlotX(px) || lastPxNeg - px < minXSpacing)
                {
                    return false;
                }
                m_xLabelPx.append(px);
                keptXLabels.append(xLabelsAll[i]);
                lastPxNeg = px;
                used++;
                return true;
            };

            int delta = 1;
            while (used < maxLabels && (zeroIdxX - delta >= 0 || zeroIdxX + delta < m_labeledXTicks.size()))
            {
                if (zeroIdxX + delta < m_labeledXTicks.size())
                {
                    tryAppendIdxPosX(zeroIdxX + delta);
                }
                if (used >= maxLabels)
                {
                    break;
                }
                if (zeroIdxX - delta >= 0)
                {
                    tryAppendIdxNegX(zeroIdxX - delta);
                }
                delta++;
            }
        }
        else
        {
            // Fallback: simple sequential
            for (int i = 0; i < m_labeledXTicks.size() && i < xLabelsAll.size(); ++i)
            {
                if (tryAppendIdxX(i) && used >= maxLabels)
                {
                    break;
                }
            }
        }
        m_labeledXTickLabels.swap(keptXLabels);

        // ====================================================================
        // Y labels with zero-aware alternating selection
        // ====================================================================
        float lastPy = 1e9f;
        used = 0;
        QVector<QString> keptYLabels;
        keptYLabels.reserve(std::min(maxLabels, m_labeledYTicks.size()));

        auto mapYPx = [&](double yy)
        {
            return plotY0 + float(plotH - (yy - y0) / (y1 - y0) * plotH);
        };
        auto withinPlotY = [&](float py)
        {
            return !(py < plotY0 - 1 || py > plotY0 + plotH + 1);
        };

        int zeroIdxY = -1;
        for (int i = 0; i < m_labeledYTicks.size(); ++i)
        {
            if (std::fabs(m_labeledYTicks[i]) <= 1e-12)
            {
                zeroIdxY = i;
                break;
            }
        }

        auto tryAppendIdxY = [&](int i) -> bool
        {
            if (i < 0 || i >= m_labeledYTicks.size() || i >= yLabelsAll.size())
            {
                return false;
            }
            double yy = m_labeledYTicks[i];
            if (yy < y0 || yy > y1)
            {
                return false;
            }
            float py = mapYPx(yy);
            if (!withinPlotY(py) || std::fabs(py - lastPy) < minYSpacing)
            {
                return false;
            }
            m_yLabelPx.append(py);
            keptYLabels.append(yLabelsAll[i]);
            lastPy = py;
            used++;
            return true;
        };

        if (zeroIdxY >= 0)
        {
            tryAppendIdxY(zeroIdxY);
            float lastPyPos = lastPy;
            float lastPyNeg = lastPy;

            auto tryAppendIdxPosY = [&](int i) -> bool
            {
                if (i < 0 || i >= m_labeledYTicks.size() || i >= yLabelsAll.size())
                {
                    return false;
                }
                double yy = m_labeledYTicks[i];
                if (yy <= 0.0 || yy < y0 || yy > y1)
                {
                    return false;
                }
                float py = mapYPx(yy);
                if (!withinPlotY(py) || std::fabs(py - lastPyPos) < minYSpacing)
                {
                    return false;
                }
                m_yLabelPx.append(py);
                keptYLabels.append(yLabelsAll[i]);
                lastPyPos = py;
                used++;
                return true;
            };

            auto tryAppendIdxNegY = [&](int i) -> bool
            {
                if (i < 0 || i >= m_labeledYTicks.size() || i >= yLabelsAll.size())
                {
                    return false;
                }
                double yy = m_labeledYTicks[i];
                if (yy >= 0.0 || yy < y0 || yy > y1)
                {
                    return false;
                }
                float py = mapYPx(yy);
                if (!withinPlotY(py) || std::fabs(py - lastPyNeg) < minYSpacing)
                {
                    return false;
                }
                m_yLabelPx.append(py);
                keptYLabels.append(yLabelsAll[i]);
                lastPyNeg = py;
                used++;
                return true;
            };

            int delta = 1;
            while (used < maxLabels && (zeroIdxY - delta >= 0 || zeroIdxY + delta < m_labeledYTicks.size()))
            {
                if (zeroIdxY + delta < m_labeledYTicks.size())
                {
                    tryAppendIdxPosY(zeroIdxY + delta);
                }
                if (used >= maxLabels)
                {
                    break;
                }
                if (zeroIdxY - delta >= 0)
                {
                    tryAppendIdxNegY(zeroIdxY - delta);
                }
                delta++;
            }
        }
        else
        {
            for (int i = 0; i < m_labeledYTicks.size() && i < yLabelsAll.size() && used < maxLabels; ++i)
            {
                tryAppendIdxY(i);
            }
        }
        m_labeledYTickLabels.swap(keptYLabels);
    }
    else
    {
        m_xLabelPx.clear();
        m_yLabelPx.clear();
        m_labeledXTickLabels.clear();
        m_labeledYTickLabels.clear();
    }

    emit appearanceChanged();
    update();
}

// ============================================================================
// Marker Line Management
// ============================================================================

QQmlListProperty<MarkerLine> LineChartItem::markerLines()
{
    return QQmlListProperty<MarkerLine>(this, &m_markerLines, &LineChartItem::markerLines_append, &LineChartItem::markerLines_count,
                                        &LineChartItem::markerLines_at, &LineChartItem::markerLines_clear);
}

int LineChartItem::addMarkerLine(bool isVertical, double position, const QString &name, const QColor &color, qreal width)
{
    auto *line = new MarkerLine(this);
    line->setLineId(m_nextMarkerLineId++);
    line->setIsVertical(isVertical);
    line->setPosition(position);
    line->setName(name);
    line->setColor(color);
    line->setWidth(width);

    m_markerLines.append(line);

    // Connect signals to trigger redraws
    connect(line, &MarkerLine::positionChanged, this, [this]() { update(); });
    connect(line, &MarkerLine::colorChanged, this, [this]() { update(); });
    connect(line, &MarkerLine::widthChanged, this, [this]() { update(); });
    connect(line, &MarkerLine::styleChanged, this, [this]() { update(); });
    connect(line, &MarkerLine::visibleChanged, this, [this]() { update(); });

    emit markerLinesChanged();
    update();
    return line->lineId();
}

void LineChartItem::removeMarkerLine(int lineId)
{
    for (int i = 0; i < m_markerLines.size(); ++i)
    {
        if (m_markerLines[i]->lineId() == lineId)
        {
            MarkerLine *line = m_markerLines.takeAt(i);
            if (line)
            {
                line->deleteLater();
            }
            emit markerLinesChanged();
            update();
            return;
        }
    }
}

MarkerLine *LineChartItem::findMarkerLine(int lineId)
{
    for (auto *line : std::as_const(m_markerLines))
    {
        if (line->lineId() == lineId)
        {
            return line;
        }
    }
    return nullptr;
}

void LineChartItem::setMarkerLinePosition(int lineId, double position)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setPosition(position);
    }
}

void LineChartItem::setMarkerLineColor(int lineId, const QColor &color)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setColor(color);
    }
}

void LineChartItem::setMarkerLineWidth(int lineId, qreal width)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setWidth(width);
    }
}

void LineChartItem::setMarkerLineStyle(int lineId, int style)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setStyle(style);
    }
}

void LineChartItem::setMarkerLinePen(int lineId, const QPen &pen)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setPen(pen);
    }
}

void LineChartItem::setMarkerLineVisible(int lineId, bool visible)
{
    if (auto *line = findMarkerLine(lineId))
    {
        line->setVisible(visible);
    }
}

void LineChartItem::clearAllMarkerLines()
{
    for (auto *line : std::as_const(m_markerLines))
    {
        if (line)
        {
            line->deleteLater();
        }
    }
    m_markerLines.clear();
    emit markerLinesChanged();
    update();
}

void LineChartItem::markerLines_append(QQmlListProperty<MarkerLine> *prop, MarkerLine *line)
{
    auto *self = static_cast<LineChartItem *>(prop->object);
    line->setLineId(self->m_nextMarkerLineId++);
    self->m_markerLines.append(line);
}

qsizetype LineChartItem::markerLines_count(QQmlListProperty<MarkerLine> *prop)
{
    return static_cast<LineChartItem *>(prop->object)->m_markerLines.size();
}

MarkerLine *LineChartItem::markerLines_at(QQmlListProperty<MarkerLine> *prop, qsizetype i)
{
    return static_cast<LineChartItem *>(prop->object)->m_markerLines.at(i);
}

void LineChartItem::markerLines_clear(QQmlListProperty<MarkerLine> *prop)
{
    auto *self = static_cast<LineChartItem *>(prop->object);
    for (auto *line : std::as_const(self->m_markerLines))
    {
        if (line)
        {
            line->deleteLater();
        }
    }
    self->m_markerLines.clear();
}

double LineChartItem::clampXSpan(double span) const
{
    if (m_minXSpan > 0 && span < m_minXSpan)
    {
        return m_minXSpan;
    }
    if (m_maxXSpan > 0 && span > m_maxXSpan)
    {
        return m_maxXSpan;
    }
    return span;
}

double LineChartItem::clampYSpan(double span) const
{
    if (m_minYSpan > 0 && span < m_minYSpan)
    {
        return m_minYSpan;
    }
    if (m_maxYSpan > 0 && span > m_maxYSpan)
    {
        return m_maxYSpan;
    }
    return span;
}

void LineChartItem::enforceXSpanConstraints()
{
    double curSpan = m_xMax - m_xMin;
    double newSpan = clampXSpan(curSpan);

    if (std::abs(newSpan - curSpan) > 1e-9)
    {
        // Maintain center point, adjust span
        double center = (m_xMin + m_xMax) * 0.5;
        m_xMin = center - newSpan * 0.5;
        m_xMax = center + newSpan * 0.5;
    }
}

void LineChartItem::enforceYSpanConstraints()
{
    double curSpan = m_yMax - m_yMin;
    double newSpan = clampYSpan(curSpan);

    if (std::abs(newSpan - curSpan) > 1e-9)
    {
        double center = (m_yMin + m_yMax) * 0.5;
        m_yMin = center - newSpan * 0.5;
        m_yMax = center + newSpan * 0.5;
    }
}

void LineChartItem::setAutoRangeX(bool enabled)
{
    if (m_autoRangeX == enabled)
    {
        return;
    }
    m_autoRangeX = enabled;

    if (enabled)
    {
        // Enabling autorange clears user modification flag
        m_userModifiedX = false;
        emit userModifiedXChanged();
    }

    updateResetActive();
    emit autoRangeXChanged();
}

void LineChartItem::setAutoRangeY(bool enabled)
{
    if (m_autoRangeY == enabled)
    {
        return;
    }
    m_autoRangeY = enabled;

    if (enabled)
    {
        m_userModifiedY = false;
        emit userModifiedYChanged();
    }

    updateResetActive();
    emit autoRangeYChanged();
}

void LineChartItem::setProgrammaticXRange(double xMin, double xMax)
{
    // Mark as programmatic update to prevent userModified flag
    m_programmaticUpdate = true;

    batchUpdateBegin();
    setXMin(xMin);
    setXMax(xMax);
    batchUpdateEnd();

    m_programmaticUpdate = false;
}

void LineChartItem::setProgrammaticYRange(double yMin, double yMax)
{
    m_programmaticUpdate = true;

    batchUpdateBegin();
    setYMin(yMin);
    setYMax(yMax);
    batchUpdateEnd();

    m_programmaticUpdate = false;
}

void LineChartItem::resetToAutoRange()
{
    resetToAutoRangeX();
    resetToAutoRangeY();
}

void LineChartItem::resetToAutoRangeX()
{
    m_userModifiedX = false;
    m_autoRangeX = true;
    emit userModifiedXChanged();
    updateResetActive();
    emit autoRangeXChanged();
}

void LineChartItem::resetToAutoRangeY()
{
    m_userModifiedY = false;
    m_autoRangeY = true;
    emit userModifiedYChanged();
    updateResetActive();
    emit autoRangeYChanged();
}

void LineChartItem::setFollowTail(bool enabled)
{
    if (m_followTail == enabled)
    {
        return;
    }
    m_followTail = enabled;
    emit followTailChanged();

    // When re-enabling, immediately jump to latest data
    if (enabled)
    {
        jumpToLatestData();
    }
}

void LineChartItem::enableFollowTail()
{
    setFollowTail(true);
}

void LineChartItem::jumpToLatestData()
{
    // Find the maximum X value across all visible series
    double maxX = -std::numeric_limits<double>::infinity();
    bool foundData = false;

    for (const auto *series : std::as_const(m_series))
    {
        if (!series->visible())
        {
            continue;
        }

        auto it = m_buffers.find(series->seriesId());
        if (it != m_buffers.end() && !it.value().data.isEmpty())
        {
            const auto &buf = it.value();
            // Check last point (assumes data is roughly time-ordered)
            maxX = std::max(maxX, buf.data.last().x());
            foundData = true;
        }
    }

    if (foundData && std::isfinite(maxX))
    {
        double currentSpan = m_xMax - m_xMin;

        // Use programmatic update to avoid triggering userModified
        m_programmaticUpdate = true;
        batchUpdateBegin();
        setXMax(maxX);
        setXMin(maxX - currentSpan);
        batchUpdateEnd();
        m_programmaticUpdate = false;

        // qDebug() << "Jumped to latest data: xMax =" << maxX;
    }
}

void LineChartItem::setXLabelFormat(const QString &format)
{
    if (m_xLabelFormat == format)
    {
        return;
    }
    m_xLabelFormat = format;
    recomputeTicks();
    emit xLabelFormatChanged();
}

void LineChartItem::setYLabelFormat(const QString &format)
{
    if (m_yLabelFormat == format)
    {
        return;
    }
    m_yLabelFormat = format;
    recomputeTicks();
    emit yLabelFormatChanged();
}

void LineChartItem::setXLabelUnit(const QString &unit)
{
    if (m_xLabelUnit == unit)
    {
        return;
    }
    m_xLabelUnit = unit;
    recomputeTicks();
    emit xLabelUnitChanged();
}

void LineChartItem::setYLabelUnit(const QString &unit)
{
    if (m_yLabelUnit == unit)
    {
        return;
    }
    m_yLabelUnit = unit;
    recomputeTicks();
    emit yLabelUnitChanged();
}

void LineChartItem::setXLabelPrefix(const QString &prefix)
{
    if (m_xLabelPrefix == prefix)
    {
        return;
    }
    m_xLabelPrefix = prefix;
    recomputeTicks();
    emit xLabelPrefixChanged();
}

void LineChartItem::setYLabelPrefix(const QString &prefix)
{
    if (m_yLabelPrefix == prefix)
    {
        return;
    }
    m_yLabelPrefix = prefix;
    recomputeTicks();
    emit yLabelPrefixChanged();
}

void LineChartItem::setXLabelSuffix(const QString &suffix)
{
    if (m_xLabelSuffix == suffix)
    {
        return;
    }
    m_xLabelSuffix = suffix;
    recomputeTicks();
    emit xLabelSuffixChanged();
}

void LineChartItem::setYLabelSuffix(const QString &suffix)
{
    if (m_yLabelSuffix == suffix)
    {
        return;
    }
    m_yLabelSuffix = suffix;
    recomputeTicks();
    emit yLabelSuffixChanged();
}

void LineChartItem::setXLabelFormatter(LabelFormatterFunc formatter)
{
    m_xLabelFormatter = formatter;
    recomputeTicks();
}

void LineChartItem::setYLabelFormatter(LabelFormatterFunc formatter)
{
    m_yLabelFormatter = formatter;
    recomputeTicks();
}

// ============================================================================
// Built-in Format Functions
// ============================================================================

QString LineChartItem::formatFixed(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

QString LineChartItem::formatScientific(double value, int decimals)
{
    return QString::number(value, 'e', decimals);
}

QString LineChartItem::formatTimeMMSS(double seconds)
{
    int totalSec = static_cast<int>(std::round(seconds));
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

QString LineChartItem::formatTimeHHMMSS(double seconds)
{
    int totalSec = static_cast<int>(std::round(seconds));
    int hours = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    int secs = totalSec % 60;
    return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

QString LineChartItem::formatTimeHHMM(double seconds)
{
    int totalSec = static_cast<int>(std::round(seconds));
    int hours = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    return QString("%1:%2").arg(hours, 2, 10, QChar('0')).arg(mins, 2, 10, QChar('0'));
}

QString LineChartItem::formatPercentage(double value, int decimals)
{
    return QString::number(value * 100.0, 'f', decimals) + "%";
}

QString LineChartItem::formatAngleDegrees(double radians, int decimals)
{
    double degrees = radians * 180.0 / M_PI;
    return QString::number(degrees, 'f', decimals) + "°";
}

QString LineChartItem::formatAngleRadians(double radians, int decimals)
{
    return QString::number(radians, 'f', decimals) + " rad";
}

QString LineChartItem::formatDecibel(double value, int decimals)
{
    return QString::number(value, 'f', decimals) + " dB";
}

QString LineChartItem::formatEngineering(double value, int decimals, const QString &unit)
{
    struct Prefix
    {
        double scale;
        QString symbol;
    };

    static const QVector<Prefix> prefixes = {{1e-12, "p"}, {1e-9, "n"}, {1e-6, "µ"}, {1e-3, "m"}, {1.0, ""},
                                             {1e3, "k"},   {1e6, "M"},  {1e9, "G"},  {1e12, "T"}};

    double absValue = std::abs(value);

    // Handle zero specially
    if (absValue < 1e-15)
    {
        return QString::number(0, 'f', decimals) + " " + unit;
    }

    // Find appropriate prefix
    for (int i = prefixes.size() - 1; i >= 0; --i)
    {
        if (absValue >= prefixes[i].scale * 0.9999)  // Small tolerance for rounding
        {
            double scaled = value / prefixes[i].scale;
            QString prefix = prefixes[i].symbol;
            if (prefix.isEmpty())
            {
                return QString::number(scaled, 'f', decimals) + " " + unit;
            }
            else
            {
                return QString::number(scaled, 'f', decimals) + " " + prefix + unit;
            }
        }
    }

    // Fallback
    return QString::number(value, 'f', decimals) + " " + unit;
}

// ============================================================================
// Label Formatting Logic
// ============================================================================

QString LineChartItem::formatXLabel(double value) const
{
    // Priority 1: Custom formatter function
    if (m_xLabelFormatter)
    {
        QString formatted = m_xLabelFormatter(value);
        return m_xLabelPrefix + formatted + m_xLabelSuffix;
    }

    // Priority 2: Built-in format string
    QString formatted;

    if (m_xLabelFormat == "scientific")
    {
        formatted = formatScientific(value, m_xLabelDecimals);
    }
    else if (m_xLabelFormat == "time_mmss")
    {
        formatted = formatTimeMMSS(value);
    }
    else if (m_xLabelFormat == "time_hhmmss")
    {
        formatted = formatTimeHHMMSS(value);
    }
    else if (m_xLabelFormat == "time_hhmm")
    {
        formatted = formatTimeHHMM(value);
    }
    else if (m_xLabelFormat == "percentage")
    {
        formatted = formatPercentage(value, m_xLabelDecimals);
    }
    else if (m_xLabelFormat == "angle_deg")
    {
        formatted = formatAngleDegrees(value, m_xLabelDecimals);
    }
    else if (m_xLabelFormat == "angle_rad")
    {
        formatted = formatAngleRadians(value, m_xLabelDecimals);
    }
    else if (m_xLabelFormat == "decibel")
    {
        formatted = formatDecibel(value, m_xLabelDecimals);
    }
    else if (m_xLabelFormat == "engineering")
    {
        formatted = formatEngineering(value, m_xLabelDecimals, m_xLabelUnit);
    }
    else  // Default: "fixed"
    {
        formatted = formatFixed(value, m_xLabelDecimals);
    }

    // Apply prefix/suffix (unless already included by formatter)
    if (m_xLabelFormat == "engineering")
    {
        return formatted;  // Engineering already includes unit
    }

    return m_xLabelPrefix + formatted + m_xLabelSuffix;
}

QString LineChartItem::formatYLabel(double value) const
{
    // Same logic for Y axis
    if (m_yLabelFormatter)
    {
        QString formatted = m_yLabelFormatter(value);
        return m_yLabelPrefix + formatted + m_yLabelSuffix;
    }

    QString formatted;

    if (m_yLabelFormat == "scientific")
    {
        formatted = formatScientific(value, m_yLabelDecimals);
    }
    else if (m_yLabelFormat == "time_mmss")
    {
        formatted = formatTimeMMSS(value);
    }
    else if (m_yLabelFormat == "time_hhmmss")
    {
        formatted = formatTimeHHMMSS(value);
    }
    else if (m_yLabelFormat == "time_hhmm")
    {
        formatted = formatTimeHHMM(value);
    }
    else if (m_yLabelFormat == "percentage")
    {
        formatted = formatPercentage(value, m_yLabelDecimals);
    }
    else if (m_yLabelFormat == "angle_deg")
    {
        formatted = formatAngleDegrees(value, m_yLabelDecimals);
    }
    else if (m_yLabelFormat == "angle_rad")
    {
        formatted = formatAngleRadians(value, m_yLabelDecimals);
    }
    else if (m_yLabelFormat == "decibel")
    {
        formatted = formatDecibel(value, m_yLabelDecimals);
    }
    else if (m_yLabelFormat == "engineering")
    {
        formatted = formatEngineering(value, m_yLabelDecimals, m_yLabelUnit);
    }
    else  // Default: "fixed"
    {
        formatted = formatFixed(value, m_yLabelDecimals);
    }

    if (m_yLabelFormat == "engineering")
    {
        return formatted;
    }

    return m_yLabelPrefix + formatted + m_yLabelSuffix;
}
