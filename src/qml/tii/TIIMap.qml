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

import QtCore
import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Layouts
import abracaComponents 1.0

Item {
    id: mainItem
    property var backend: undefined
    property bool dragEnabled: true
    property bool showTable: true

    readonly property int buttonsSize: UI.isAndroid ? UI.controlHeight : UI.controlHeight - 4

    Plugin {
        id: mapPlugin
        name: "osm"
        PluginParameter { name: "osm.mapping.copyright"; value: "&nbsp;&nbsp;© <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors" }
        PluginParameter { name: "osm.mapping.custom.host"; value: "https://tile.openstreetmap.org/"}
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: backend.mapCenter
        zoomLevel: backend.zoomLevel
        property geoCoordinate startCentroid

        activeMapType: map.supportedMapTypes[map.supportedMapTypes.length-1]
        onCopyrightLinkActivated: link => Qt.openUrlExternally(link)

        onZoomLevelChanged: {
            backend.zoomLevel = map.zoomLevel;
        }
        onCenterChanged: {
            if (backend.mapCenter !== map.center) {
                 backend.mapCenter = map.center;
            }
        }

        PinchHandler {
            id: pinch
            target: null
            onActiveChanged: if (active) {
                                 map.startCentroid = map.toCoordinate(pinch.centroid.position, false)
                             }
            onScaleChanged: (delta) => {
                                map.zoomLevel += Math.log2(delta)
                                map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
                            }
            // onRotationChanged: (delta) => {
            //                        map.bearing -= delta
            //                        map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
            //                    }
            grabPermissions: PointerHandler.TakeOverForbidden
        }
        WheelHandler {
            id: wheel
            // workaround for QTBUG-87646 / QTBUG-112394 / QTBUG-112432:
            // Magic Mouse pretends to be a trackpad but doesn't work with PinchHandler
            // and we don't yet distinguish mice and trackpads on Wayland either
            acceptedDevices: Qt.platform.pluginName == "cocoa" || Qt.platform.pluginName == "wayland"  || Qt.platform.pluginName == "xcb"
                             ? PointerDevice.Mouse | PointerDevice.TouchPad
                             : PointerDevice.Mouse
            rotationScale: 1/120
            property: "zoomLevel"
        }
        DragHandler {
            id: drag
            // target: map
            enabled: mainItem.dragEnabled
            onTranslationChanged: (delta) => {
                                      map.pan(-delta.x, -delta.y);
                                      backend.centerToCurrentPosition = false;
                                  }
        }
        TapHandler {
            id: tapHandler
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.WithinBounds
            onTapped: {
                // deselection of transmitter
                backend.selectTx(-1);
            }
        }
        Shortcut {
            enabled: map.zoomLevel < map.maximumZoomLevel
            sequence: StandardKey.ZoomIn
            onActivated: map.zoomLevel = Math.round(map.zoomLevel + 1)
        }
        Shortcut {
            enabled: map.zoomLevel > map.minimumZoomLevel
            sequence: StandardKey.ZoomOut
            onActivated: map.zoomLevel = Math.round(map.zoomLevel - 1)
        }

        MapQuickItem {
            id: currentPosition
            parent: map
            sourceItem: Rectangle { width: 14; height: 14; color: "#251ee4"; border.width: 2; border.color: "white"; smooth: true; radius: 7; opacity: 0.8 }
            coordinate: backend.currentPosition
            opacity: 1.0
            anchorPoint: Qt.point(sourceItem.width/2, sourceItem.height/2)
            visible: backend.positionValid
        }

        MapItemView {
            model: backend.tableModel
            delegate: TransmitterMarker {
                id: marker
                parent: map
                coordinate:  coordinates
                tiiCode: tiiString
                markerColor: levelColor
                isSelected: (index >= 0) ? selectedTx : false
                isTiiMode: backend.isTii
                z: isSelected ? 2 : 1

                TapHandler {
                    id: txTapHandler
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.WithinBounds
                    onTapped: {
                        backend.selectTx(index);
                    }
                }
            }
        }

        Rectangle {
            id: infoBox

            HoverHandler {
                id: infoHoverHandler
            }

            color: "white"
            opacity: infoHoverHandler.hovered ? 1.0 : 0.75
            width: infoLayout.width + 10
            height: infoLayout.height + 10
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 10
            anchors.bottomMargin: UI.isMobile ? 25 : 10
            visible: backend.ensembleInfo[0] !== ""
            z: 3
            // Behavior on width {
            //     SmoothedAnimation {
            //         velocity: 1000
            //     }
            // }
            // Behavior on height {
            //     SmoothedAnimation {
            //         velocity: 1000
            //     }
            // }
            Behavior on opacity {
                SmoothedAnimation {
                    velocity: 20
                }
            }
            ColumnLayout {
                id: infoLayout
                anchors.centerIn: parent
                Repeater {
                    model: backend.ensembleInfo
                    delegate: Text {
                        Layout.fillWidth: true
                        text: modelData
                        visible: backend.txInfo.length === 0 || (mainItem.height > 400 && !UI.isMobile)
                    }
                }
                Rectangle {
                    height: 1;
                    color: "#c0c0c0";
                    Layout.fillWidth: true;
                    visible: backend.txInfo.length > 0 && (mainItem.height > 400  && !UI.isMobile)
                }
                Repeater {
                    model: backend.txInfo
                    delegate: Text {
                        Layout.fillWidth: true
                        text: modelData
                    }
                }
            }
        }
        Loader {
            active: backend.isTii && showTable
            asynchronous: true

            anchors.right: map.right
            anchors.top: map.top
            anchors.rightMargin: (map.width - width) > width/2 ? 10 : (map.width - width) / 2
            anchors.topMargin: 10

            sourceComponent: AbracaTableView {
                id: tiiTableView
                model: backend.tableModel
                selectionModel: backend.tableSelectionModel
                visible: backend.tableModel.rowCount > 0

                sortingEnabled: true
                sortIndicatorColumn: 1
                sortIndicatorOrder: Qt.DescendingOrder
                cellsLeftAligned: false
                maxColumnWidth: 200

                height: map.width > 3*width ? infoBox.y - map.y - 20 : Math.min(infoBox.y - map.y - 20, map.height * 0.4)
                width: Math.min(preferedWidth, map.width - 20)
            }
        }
        Connections {
            target: backend
            function onTxTableColChanged() {
                if (tiiTableView.visible) {
                    backend.selectTx(-1); // deselection of transmitter
                    tiiTableView.calculatePreferedWidth();
                    tiiTableView.autoAdjustColumns();
                }
            }
        }

        ColumnLayout {
            id: buttonsLayout
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.bottomMargin: 25
            z: 3
            FontMetrics {
                id: fMetrics
            }
            Item {
                id: centerPosition
                Layout.preferredWidth: mainItem.buttonsSize
                Layout.preferredHeight: mainItem.buttonsSize
                Rectangle {
                    anchors.fill: parent
                    radius: UI.controlRadius
                    color: "white"
                    opacity: centerPosMouseArea.containsMouse ? 1.0 : 0.75
                }

                readonly property int m: 2
                readonly property int l: 6
                readonly property int w: 2

                Rectangle {
                    anchors.centerIn: parent
                    width: 20
                    height: width
                    radius: width /2
                    color: "transparent"
                    border.width: centerPosition.w
                    border.color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: centerPosition.m
                    width: centerPosition.l
                    height: centerPosition.w
                    color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: centerPosition.m
                    width: centerPosition.l
                    height: centerPosition.w
                    color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: centerPosition.m
                    width: centerPosition.w
                    height: centerPosition.l
                    color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: centerPosition.m
                    width: centerPosition.w
                    height: centerPosition.l
                    color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 8
                    height: width
                    radius: width /2
                    color: centerPosMouseArea.containsMouse ? "black" : "#707070"
                    visible: backend.centerToCurrentPosition
                }
                MouseArea {
                    id: centerPosMouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        backend.centerToCurrentPosition = true;
                        // map.center = backend.positionValid ? backend.currentPosition : QtPositioning.coordinate(50.08804, 14.42076) // Prague
                    }
                }
            }
            Item {
                id: plus
                visible: UI.isDesktop
                Layout.preferredWidth: mainItem.buttonsSize
                Layout.preferredHeight: mainItem.buttonsSize
                Rectangle {
                    anchors.fill: parent
                    radius: UI.controlRadius
                    color: "white"
                    opacity: plusMouseArea.containsMouse ? 1.0 : 0.75
                }
                Rectangle {
                    width: 14
                    height: 4
                    color: plusMouseArea.containsMouse ? "black" : "#707070"
                    anchors.centerIn: parent
                }
                Rectangle {
                    width: 4
                    height: 14
                    color: plusMouseArea.containsMouse ? "black" : "#707070"
                    anchors.centerIn: parent
                }
                MouseArea {
                    id: plusMouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        map.zoomLevel = (map.zoomLevel < map.maximumZoomLevel) ? (map.zoomLevel+0.1) : map.maximumZoomLevel;
                    }
                }
            }
            Item {
                id: minus
                visible: UI.isDesktop
                Layout.preferredWidth: mainItem.buttonsSize
                Layout.preferredHeight: mainItem.buttonsSize
                Rectangle {
                    radius: UI.controlRadius
                    anchors.fill: parent
                    color: "white"
                    opacity: minusMouseArea.containsMouse ? 1.0 : 0.75
                }
                Rectangle {
                    width: 14
                    height: 4
                    color: minusMouseArea.containsMouse ? "black" : "#707070"
                    anchors.centerIn: parent
                }
                MouseArea {
                    id: minusMouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                    onClicked: {
                        map.zoomLevel -= 0.1;
                    }
                }
            }
            Item {
                id: logButtonItem
                readonly property int spacing: 7
                Layout.preferredHeight: mainItem.buttonsSize
                Layout.preferredWidth: Math.max(logText.width + recSymbol.width + 2*spacing + (logText.visible ? spacing : 0), mainItem.buttonsSize)
                visible: backend.isTii
                Rectangle {
                    id: logButton
                    anchors.fill: parent
                    radius: UI.controlRadius
                    color: "white"
                    opacity: logMouseArea.containsMouse ? 1.0 : 0.75
                    Behavior on opacity {
                        SmoothedAnimation {
                            velocity: 100
                        }
                    }
                    AbracaLabel {
                        id: logText
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: recSymbol.width + 2*logButtonItem.spacing
                        text: visible ? (backend.isRecordingLog ? qsTr("Stop logging") : qsTr("Record CSV log")) : ""
                        visible: logMouseArea.containsMouse && UI.isDesktop
                    }
                }
                Rectangle {
                    id: recSymbol
                    color: backend.isRecordingLog ? "#ff4b4b" : logMouseArea.containsMouse ? "black" : "#707070"
                    height: mainItem.buttonsSize / 2
                    width: height
                    radius: 50
                    anchors.verticalCenter: logButton.verticalCenter
                    anchors.left: logButton.left
                    anchors.leftMargin: (mainItem.buttonsSize- height) / 2
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PropertyAnimation { from: 1.0; to: 0.5; duration: 1000 }
                        PropertyAnimation { from: 0.5; to: 1.0; duration: 1000 }
                        running: backend.isRecordingLog
                    }
                }
                MouseArea {
                    id: logMouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                    propagateComposedEvents: true
                    onClicked: {
                        recSymbol.opacity = 1.0
                        backend.startStopLog();
                    }
                }
            }
        }
    }
}
