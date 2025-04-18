/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "ensemblebar.h"

#include <QApplication>
#include <QHelpEvent>
#include <QPaintEvent>
#include <QPainter>

// Define color palettes
const QList<QColor> EnsembleBar::m_palette = {
    QColor(0x42, 0x85, 0xF4),  // Blue
    QColor(0xEA, 0x43, 0x35),  // Red
    QColor(0xFB, 0xBC, 0x05),  // Yellow
    QColor(0x34, 0xA8, 0x53),  // Green
    QColor(0x9C, 0x27, 0xB0),  // Purple
    QColor(0xFF, 0x98, 0x00),  // Orange
    QColor(0x00, 0xBC, 0xD4),  // Cyan
    QColor(0x79, 0x55, 0x48)   // Brown
};

EnsembleBar::EnsembleBar(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(26);
    setMouseTracking(true);            // Enable mouse tracking for hover effects
    setAttribute(Qt::WA_Hover, true);  // Enable hover events
}

void EnsembleBar::setSubchannels(const QList<Subchannel> &subchannels)
{
    // Only copy non-empty segments (gaps will be recalculated)
    m_subchannels.clear();
    for (const auto &subch : subchannels)
    {
        if (subch.startCU < (subch.startCU + subch.numCU) && subch.startCU >= 0 && (subch.startCU + subch.numCU) <= TotalNumCU)
        {
            m_subchannels.append(subch);
        }
    }

    // Sort by start
    std::sort(m_subchannels.begin(), m_subchannels.end(), [](const Subchannel &a, const Subchannel &b) { return a.startCU < b.startCU; });

    // Assign colors to any segments that is not gap
    assignAutoColors();

    // Fill gaps between segments
    fillGaps();

    update();
}

void EnsembleBar::clearSubchannels()
{
    m_subchannels.clear();
    update();
}

void EnsembleBar::selectSubchannel(int id)
{
    for (const auto &subch : m_subchannels)
    {
        if (!subch.isEmpty && subch.id == id)
        {
            if (!subch.isSelected)
            {
                toggleSelected(id);
            }
            return;
        }
    }
}

void EnsembleBar::assignAutoColors()
{
    int colorIdx = 0;
    for (auto &subch : m_subchannels)
    {
        if (!subch.isEmpty)  // && subch.color == Qt::transparent)
        {
            subch.color = m_palette[colorIdx % m_palette.size()];
            colorIdx++;
        }
    }
}

void EnsembleBar::toggleSelected(int id)
{
    for (auto it = m_subchannels.begin(); it != m_subchannels.end(); ++it)
    {
        if (!it->isEmpty)
        {
            if (id == it->id)
            {
                it->isSelected = !it->isSelected;
                emit subchannelSelected(id, it->isSelected);
            }
            else
            {
                it->isSelected = false;
            }
        }
    }

    // force repaint
    update();
}

void EnsembleBar::fillGaps()
{
    QVector<Subchannel> subchannelsWithGaps;

    // Add first gap if needed (from 0 to first segment start)
    if (m_subchannels.isEmpty())
    {
        // No segments, add a gap for the entire range
        Subchannel gap;  //(0, TotalNumCU, Qt::transparent, "Empty: 0-" + QString::number(TotalNumCU));
        gap.startCU = 0;
        gap.numCU = TotalNumCU;
        gap.color = Qt::transparent;
        gap.isEmpty = true;
        subchannelsWithGaps.append(gap);
    }
    else
    {
        if (m_subchannels.first().startCU > 0)
        {
            Subchannel gap;
            gap.startCU = 0;
            gap.numCU = m_subchannels.first().startCU;
            gap.color = Qt::transparent;
            gap.isEmpty = true;
            subchannelsWithGaps.append(gap);
        }

        // Add all segments and gaps between them
        for (int i = 0; i < m_subchannels.size(); i++)
        {
            // Add the current segment
            subchannelsWithGaps.append(m_subchannels[i]);

            // Add gap to next segment if there is one
            if (i < m_subchannels.size() - 1 && (m_subchannels[i].startCU + m_subchannels[i].numCU) < m_subchannels[i + 1].startCU)
            {
                int gapSize = m_subchannels[i + 1].startCU - (m_subchannels[i].startCU + m_subchannels[i].numCU);
                Subchannel gap;
                gap.startCU = m_subchannels[i].startCU + m_subchannels[i].numCU;
                gap.numCU = gapSize;
                gap.color = Qt::transparent;
                gap.isEmpty = true;
                subchannelsWithGaps.append(gap);
            }
        }

        // Add final gap if needed (from last segment end to total parts)
        if (m_subchannels.last().startCU + m_subchannels.last().numCU < TotalNumCU)
        {
            int gapSize = TotalNumCU - (m_subchannels.last().startCU + m_subchannels.last().numCU);
            Subchannel gap;
            gap.startCU = m_subchannels.last().startCU + m_subchannels.last().numCU;
            gap.numCU = gapSize;
            gap.color = Qt::transparent;
            gap.isEmpty = true;
            subchannelsWithGaps.append(gap);
        }
    }

    m_subchannels = subchannelsWithGaps;
}

void EnsembleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const int margin = 4;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int barWidth = width();
    const int barHeight = height();

    // Line parameters
    int lineY = barHeight / 2;
    int lineThickness = 4;

    // Draw baseline
    painter.setPen(QPen(Qt::darkGray, lineThickness));
#define COMPENSATE_BORDER 1
#if COMPENSATE_BORDER
    if (m_subchannels.empty())
    {
        painter.drawLine(0, lineY, barWidth, lineY);
    }
    else
    {  // this is to make ends of the line invisible (in theory it should be -1 but experiments show it must be -3)
        int start = 0;
        int end = barWidth;
        if (m_subchannels.first().isEmpty == false && m_subchannels.first().startCU == 0)
        {
            start += 3;
        }
        if (m_subchannels.last().isEmpty == false && (m_subchannels.last().startCU + m_subchannels.last().numCU) == TotalNumCU)
        {
            end -= 3;
        }
        painter.drawLine(start, lineY, end, lineY);
    }
#else
    painter.drawLine(0, lineY, barWidth, lineY);
#endif

    if (m_subchannels.isEmpty())
    {
        // Draw empty bar
        painter.fillRect(rect(), Qt::transparent);
        return;
    }

    // painter.setPen(QPen(Qt::black, 1));
    //  painter.setPen(Qt::NoPen);

    auto selected = m_subchannels.cend();
    for (auto it = m_subchannels.cbegin(); it != m_subchannels.cend(); ++it)
    {
        // Calculate segment position
#if COMPENSATE_BORDER
        int startX = qMax(qRound(static_cast<double>(it->startCU) / TotalNumCU * barWidth), 1);
        int endX = qMin(qRound(static_cast<double>(it->startCU + it->numCU) / TotalNumCU * barWidth), barWidth - 1);
#else
        int startX = qRound(static_cast<double>(it->startCU) / TotalNumCU * barWidth);
        int endX = qRound(static_cast<double>(it->startCU + it->numCU) / TotalNumCU * barWidth);
#endif

        int segmentWidth = endX - startX;

        // Draw the segment
        if (it->isEmpty)
        {
            painter.setPen(Qt::NoPen);
        }
        else
        {
            painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            if (it->isSelected)
            {
                selected = it;
            }
        }
        painter.setBrush(it->color);
        painter.drawRect(QRect(startX, 0 + margin, segmentWidth, barHeight - 2 * margin));  // No border
        if (!it->isEmpty && !it->isAudio)
        {
            painter.setBrush(QBrush(Qt::black, Qt::DiagCrossPattern));
            painter.drawRect(QRect(startX, 0 + margin, segmentWidth, barHeight - 2 * margin));  // No border
        }
    }

    // draw selected on top
    if (selected != m_subchannels.cend())
    {
        const int selectionLineWidthHalf = margin / 2;

        // Calculate segment position
        int startX = qMax(qRound(static_cast<double>(selected->startCU) / TotalNumCU * barWidth), selectionLineWidthHalf);
        int endX = qMin(qRound(static_cast<double>(selected->startCU + selected->numCU) / TotalNumCU * barWidth), barWidth - selectionLineWidthHalf);

        int segmentWidth = endX - startX;

        painter.setPen(QPen(Qt::white, 2 * selectionLineWidthHalf, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

        painter.setBrush(selected->color);
        painter.drawRect(QRect(startX, selectionLineWidthHalf, segmentWidth,
                               barHeight - 2 * selectionLineWidthHalf));  // No border
        if (!selected->isAudio)
        {
            painter.setBrush(QBrush(Qt::black, Qt::DiagCrossPattern));
            painter.drawRect(QRect(startX, selectionLineWidthHalf, segmentWidth,
                                   barHeight - 2 * selectionLineWidthHalf));  // No border
        }
    }
}

int EnsembleBar::findSubChAtPosition(const QPoint &pos) const
{
    if (m_subchannels.isEmpty())
    {
        return -1;
    }

    const int barWidth = width();

    for (int i = 0; i < m_subchannels.size(); ++i)
    {
        const Subchannel &subch = m_subchannels[i];
        int startX = qRound(static_cast<double>(subch.startCU) / TotalNumCU * barWidth);
        int endX = qRound(static_cast<double>(subch.startCU + subch.numCU) / TotalNumCU * barWidth);

        if (pos.x() >= startX && pos.x() < endX)
        {
            return i;
        }
    }

    return -1;
}

bool EnsembleBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        int subchIndex = findSubChAtPosition(helpEvent->pos());

        if (subchIndex >= 0 && subchIndex < m_subchannels.size())
        {
            const Subchannel &subch = m_subchannels[subchIndex];
            QString tooltip;

            if (subch.isEmpty)
            {
                tooltip = QString(tr("<p style='white-space:pre'>Unused<br>Capacity units: %1 CU [%2..%3]</p>"))
                              .arg(subch.numCU)
                              .arg(subch.startCU)
                              .arg(subch.startCU + subch.numCU - 1);
            }
            else
            {
                tooltip = QString(tr("<p style='white-space:pre'>SubChannel: %1 (%2)<br>Capacity units: %3 CU [%4..%5]</p>"))
                              .arg(subch.id)
                              .arg(subch.isAudio ? tr("audio") : tr("data"))
                              .arg(subch.numCU)
                              .arg(subch.startCU)
                              .arg(subch.startCU + subch.numCU - 1);
            }

            QToolTip::showText(helpEvent->globalPos(), tooltip, this);
            return true;
        }
        else
        {
            QToolTip::hideText();
        }
    }

    return QWidget::event(event);
}

void EnsembleBar::mouseMoveEvent(QMouseEvent *event)
{
    int subchIndex = findSubChAtPosition(event->pos());
    bool isSubch = false;
    if (subchIndex >= 0 && subchIndex < m_subchannels.size())
    {
        isSubch = !m_subchannels[subchIndex].isEmpty;
    }

    setCursor(isSubch ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void EnsembleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    int subchIndex = findSubChAtPosition(event->pos());
    if (subchIndex >= 0 && subchIndex < m_subchannels.size())
    {
        if (!m_subchannels[subchIndex].isEmpty)
        {
            toggleSelected(m_subchannels[subchIndex].id);
        }
    }
}
