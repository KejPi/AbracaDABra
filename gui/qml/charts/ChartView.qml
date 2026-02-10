import QtQuick
import abracaComponents

Item {
    id: root
    property alias chart: chart
    property string xAxisTitle: ""
    property string yAxisTitle: ""
    property double xMin: chart.xMin
    property double xMax: chart.xMax
    property double yMin: chart.yMin
    property double yMax: chart.yMax
    property double majorTickStepX: 0
    property int minorSectionsX: 5
    property double majorTickStepY: 0
    property int minorSectionsY: 5
    property int historyCapacity: 4096
    property color backgroundColor: "#101014"
    property color gridMajorColor: "#808080"
    property color gridMinorColor: "#404040"
    property color axisLineColor: "#c0c0c8"
    property color axisTickColor: "#a0a0a8"
    property color labelTextColor: "#e0e0e8"
    property color axisSelectionColor: "#80baff"
    property int axisLineWidth: 3
    property string zoomAxis: "none"
    property int axisHitThickness: 24
    property bool xAutoScroll: true
    property int yTickLabelWidth: 48
    property int xTickLabelHeight: 16
    property alias followTail: chart.followTail
    property string dataMode: "append"
    property int xTickCount: 0
    property int yTickCount: 0
    property bool showButton: false
    property bool decimationEnabled: chart.decimationEnabled

    width: 400
    height: 300

    // OPTIMIZATION: Tick labels rendered outside plot band
    Item {
        id: xTickLayer
        anchors.fill: parent
        z: 50
        visible: true
        Repeater {
            model: chart.xLabelPx
            delegate: Text {
                color: chart.labelTextColor
                font.pixelSize: 12
                text: chart.labeledXTickLabels[index]
                x: modelData - width / 2
                y: chart.height - chart.plotBottomMargin + 2
            }
        }
    }

    Item {
        id: yTickLayer
        anchors.fill: parent
        z: 50
        visible: true
        Repeater {
            model: chart.yLabelPx
            delegate: Text {
                color: chart.labelTextColor
                font.pixelSize: 12
                text: chart.labeledYTickLabels[index]
                x: chart.plotLeftMargin - 4 - width
                y: modelData - height / 2
            }
        }
    }

    // OPTIMIZATION: Update resetActive via signal instead of binding
    Connections {
        target: chart
        function onRangeChanged() {
        }
        function onResetActiveChanged() {
        }
    }

    LineChartItem {
        id: chart
        anchors.fill: parent
        clip: true
        historyCapacity: root.historyCapacity
        backgroundColor: root.backgroundColor
        gridMajorColor: root.gridMajorColor
        gridMinorColor: root.gridMinorColor
        axisLineColor: root.axisLineColor
        axisTickColor: root.axisTickColor
        labelTextColor: root.labelTextColor
        axisSelectionColor: root.axisSelectionColor
        xMin: root.xMin
        xMax: root.xMax
        yMin: root.yMin
        yMax: root.yMax
        plotTopMargin: 24
        plotRightMargin: 24
        majorTickStepX: root.majorTickStepX
        majorTickStepY: root.majorTickStepY
        xAxisTitle: root.xAxisTitle
        yAxisTitle: root.yAxisTitle

        // Expose follow/data mode to C++ via dynamic properties
        property bool followTailProp: root.followTail
        property string dataModeProp: root.dataMode
        readonly property int baseLeftMargin: 10
        readonly property int baseBottomMargin: 10
        readonly property int axisLineWidth: root.axisLineWidth

        plotLeftMargin: chart.baseLeftMargin + root.yTickLabelWidth + 8
        plotBottomMargin: chart.baseBottomMargin + root.xTickLabelHeight + (showButton ? 22 : 12)

        // Y-axis selection zones
        MouseArea {
            x: leftAxisLine.x - (root.yTickLabelWidth + 8)
            y: leftAxisLine.y
            width: (root.yTickLabelWidth) + root.axisHitThickness
            height: leftAxisLine.height
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            onClicked: root.zoomAxis = (root.zoomAxis === "y" ? "none" : "y")
        }

        Rectangle {
            id: leftAxisLine
            x: chart.plotLeftMargin - chart.axisLineWidth / 2
            y: chart.plotTopMargin - chart.axisLineWidth / 2
            width: chart.axisLineWidth
            height: chart.height - chart.plotTopMargin - chart.plotBottomMargin + chart.axisLineWidth
            color: root.zoomAxis === "y" ? chart.axisSelectionColor : chart.axisLineColor
            z: root.zoomAxis === "y" ? 200 : 20
        }

        MouseArea {
            x: rightAxisLine.x - root.axisHitThickness / 2
            y: rightAxisLine.y
            width: root.axisHitThickness
            height: rightAxisLine.height
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            onClicked: root.zoomAxis = (root.zoomAxis === "y" ? "none" : "y")
        }

        Rectangle {
            id: rightAxisLine
            x: chart.width - chart.plotRightMargin - chart.axisLineWidth / 2
            y: leftAxisLine.y
            width: chart.axisLineWidth
            height: leftAxisLine.height
            color: root.zoomAxis === "y" ? chart.axisSelectionColor : chart.axisLineColor
            z: root.zoomAxis === "y" ? 200 : 20
        }

        // X-axis selection zones
        MouseArea {
            x: bottomAxisLine.x
            y: bottomAxisLine.y - root.axisHitThickness / 2
            width: bottomAxisLine.width
            height: root.axisHitThickness + root.xTickLabelHeight + 6
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: root.zoomAxis = (root.zoomAxis === "x" ? "none" : "x")
            //onDoubleClicked: root.followTail = true
            onDoubleClicked: chart.enableFollowTail()
        }

        Rectangle {
            id: bottomAxisLine
            x: leftAxisLine.x
            y: leftAxisLine.y + leftAxisLine.height - chart.axisLineWidth // chart.plotBottomMargin
            width: rightAxisLine.x - leftAxisLine.x + chart.axisLineWidth // chart.width - chart.plotLeftMargin - chart.plotRightMargin + chart.axisLineWidth
            height: chart.axisLineWidth
            color: root.zoomAxis === "x" ? chart.axisSelectionColor : chart.axisLineColor
            z: root.zoomAxis === "x" ? 200 : 20
        }

        MouseArea {
            x: topAxisLine.x
            y: topAxisLine.y - root.axisHitThickness / 2
            width: topAxisLine.width
            height: root.axisHitThickness
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: root.zoomAxis = (root.zoomAxis === "x" ? "none" : "x")
        }

        Rectangle {
            id: topAxisLine
            x: leftAxisLine.x
            y: leftAxisLine.y
            width: bottomAxisLine.width
            height: chart.axisLineWidth
            color: root.zoomAxis === "x" ? chart.axisSelectionColor : chart.axisLineColor
            z: root.zoomAxis === "x" ? 200 : 20
        }

        Item {
            id: chartArea
            anchors.left: leftAxisLine.left
            anchors.right: rightAxisLine.right
            anchors.top: topAxisLine.top
            anchors.bottom: bottomAxisLine.bottom

            WheelHandler {
                // target: chartArea
                // acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"  || Qt.platform.pluginName === "xcb"
                                 ? PointerDevice.Mouse | PointerDevice.TouchPad
                                 : PointerDevice.Mouse
                onWheel: event => {
                    chart.applyWheel(event.angleDelta.y, event.x, event.y, root.zoomAxis);
                    event.accepted = true;
                }
            }
            PinchHandler {
                id: pinchHandler
                target: null
                // target: chart
                // grabPermissions: PointerHandler.CanTakeOverFromAnything
                onScaleChanged: (delta) => {
                    //console.log("Pinch scale:", delta, pinchHandler.centroid.position.x, pinchHandler.centroid.position.y, chartArea.x, chartArea.y);
                    chart.applyPinch(delta,
                                     pinchHandler.centroid.position.x,
                                     pinchHandler.centroid.position.y,
                                     root.zoomAxis);
                }
            }
            DragHandler {
                id: dragHandler
                target: null
                grabPermissions: PointerHandler.TakeOverForbidden // PointerHandler.CanTakeOverFromAnything
                property real startXMin: 0
                property real startXMax: 0
                property real startYMin: 0
                property real startYMax: 0

                onActiveChanged: {
                    if (active) {
                        startXMin = chart.xMin;
                        startXMax = chart.xMax;
                        startYMin = chart.yMin;
                        startYMax = chart.yMax;
                    }
                }

                onTranslationChanged: {
                    // OPTIMIZATION: Use stored start values and call C++ once
                    const xSpan = startXMax - startXMin;
                    const ySpan = startYMax - startYMin;
                    const dx = translation.x / chart.width * xSpan;
                    const dy = translation.y / chart.height * ySpan;

                    chart.xMin = startXMin - dx;
                    chart.xMax = chart.xMin + xSpan;
                    chart.yMin = startYMin + dy;
                    chart.yMax = chart.yMin + ySpan;

                    root.followTail = false;
                }
            }
        }

        // Axis titles
        Text {
            id: xTitle
            text: chart.xAxisTitle
            color: chart.labelTextColor
            font.pixelSize: 14
            y: bottomAxisLine.y + chart.axisLineWidth + root.xTickLabelHeight
            x: chart.plotLeftMargin + (chart.width - chart.plotLeftMargin - chart.plotRightMargin) / 2 - width / 2
            z: 10
        }

        // Control buttons
        Rectangle {
            id: resetView
            width: 64
            height: 22
            radius: 4
            color: chart.resetActive ? "#2e7d32" : "#555"
            border.color: chart.resetActive ? "#4caf50" : "#888"
            border.width: 1
            z: 100
            x: chart.plotLeftMargin + (chart.width - chart.plotLeftMargin - chart.plotRightMargin) - width
            y: chart.height - height - 6
            visible: showButton && root.dataMode === "replace"

            Text {
                anchors.centerIn: parent
                text: qsTr("Reset")
                color: "#fff"
                font.pixelSize: 12
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: if (chart.resetActive)
                    chart.resetZoom()
            }
        }

        Rectangle {
            id: liveToggle
            width: 56
            height: 22
            radius: 4
            color: root.followTail ? "#2e7d32" : "#555"
            border.color: root.followTail ? "#4caf50" : "#888"
            border.width: 1
            z: 100
            x: chart.plotLeftMargin + (chart.width - chart.plotLeftMargin - chart.plotRightMargin) - width
            y: chart.height - height - 6
            visible: showButton && root.dataMode === "append"

            Text {
                anchors.centerIn: parent
                text: chart.followTail ? qsTr("Live") : qsTr("Paused")
                color: "#fff"
                font.pixelSize: 12
            }

            MouseArea {
                anchors.fill: parent
                onClicked: chart.followTail = !chart.followTail
            }
        }

        Text {
            id: yTitle
            text: chart.yAxisTitle
            color: chart.labelTextColor
            font.pixelSize: 14
            x: -yTitle.implicitWidth / 2 + chart.baseLeftMargin + 16
            y: chart.plotTopMargin + (chart.height - chart.plotBottomMargin - chart.plotTopMargin) / 2 - height / 2
            rotation: -90
            transformOrigin: Item.Center
            z: 10
        }
    }
}
