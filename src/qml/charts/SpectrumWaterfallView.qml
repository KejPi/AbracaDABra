/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

import abracaComponents

// Combined spectrum + waterfall panel.
// The two sub-panes are separated by a thin hairline SplitView handle that
// blends into the chart background — no visible bar, just a 1 px line.
SplitView {
    id: root
    Accessible.role: Accessible.Chart
    Accessible.name: qsTr("Signal spectrum")

    property bool showWaterfall: false
    property var splitterState: null

    // ---- Public API (mirrors ChartView / LineChartItem for callers) ----------
    property alias spectrumChart: spectrumChart
    property alias chart: spectrumChart.chart
    property alias waterfall: waterfallView
    property alias labelTextColor: spectrumChart.labelTextColor
    property alias backgroundColor: spectrumChart.backgroundColor

    function saveInnerState() {
        return root.saveState()
    }

    // -------------------------------------------------------------------------
    orientation: Qt.Vertical

    Component.onCompleted: {
        if (splitterState) {
            root.restoreState(splitterState)
        }
    }

    // ---- Hairline handle ----------------------------------------------------
    handle: Item {
        implicitWidth: root.width
        implicitHeight: 6

        // Thin horizontal line — the only visible chrome
        Rectangle {
            width: parent.width
            height: 2
            anchors.verticalCenter: parent.verticalCenter
            color: SplitHandle.pressed ? spectrumChart.axisLineColor
                 : SplitHandle.hovered ? spectrumChart.axisTickColor
                 : spectrumChart.gridMajorColor
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        // Widen the hit-area cursor so users can find and grab it easily
        HoverHandler {
            cursorShape: Qt.SizeVerCursor
        }
    }

    // ---- Spectrum (top) -----------------------------------------------------
    ChartView {
        id: spectrumChart

        SplitView.fillWidth: true
        SplitView.fillHeight: true
        SplitView.minimumHeight: root.height / 3
    }

    // ---- Waterfall (bottom) -------------------------------------------------
    WaterfallItem {
        id: waterfallView

        SplitView.fillWidth: true
        SplitView.preferredHeight: 150
        SplitView.minimumHeight: root.height / 4

        visible: root.showWaterfall

        // Sync visible frequency range and colour scale with spectrum chart
        xMin: spectrumChart.chart.xMin
        xMax: spectrumChart.chart.xMax
        yMin: spectrumChart.chart.yMin
        yMax: spectrumChart.chart.yMax

        // Match plot margins so frequency columns align pixel-perfectly
        plotLeftMargin: spectrumChart.chart.plotLeftMargin
        plotRightMargin: spectrumChart.chart.plotRightMargin
        plotTopMargin: 4
        plotBottomMargin: 4

        backgroundColor: spectrumChart.backgroundColor
        numRows: 200
        rowHeight: 3
    }
}
