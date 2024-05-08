/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
import QtQuick.Controls
import QtQuick.Layouts
import app.qmlcomponents 1.0

Item {
    anchors.fill: parent

    Loader {
        anchors.fill: parent
        active: tii.isVisible
        visible: active
        sourceComponent: mainComponent
    }

    Component {
        id: mainComponent
        Item {
            property bool centerToCurrentPosition: true
            Connections {
                target: tii
                function onCurrentPositionChanged() {
                    if (tii.positionValid && centerToCurrentPosition) {
                        map.center = tii.currentPosition;
                    }
                }
            }
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
                center: tii.positionValid ? tii.currentPosition : QtPositioning.coordinate(50.08804, 14.42076) // Prague
                zoomLevel: 9
                property geoCoordinate startCentroid

                activeMapType: map.supportedMapTypes[map.supportedMapTypes.length-1]

                onCopyrightLinkActivated: link => Qt.openUrlExternally(link)

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
                    onRotationChanged: (delta) => {
                                           map.bearing -= delta
                                           map.alignCoordinateToPoint(map.startCentroid, pinch.centroid.position)
                                       }
                    grabPermissions: PointerHandler.TakeOverForbidden
                }
                WheelHandler {
                    id: wheel
                    // workaround for QTBUG-87646 / QTBUG-112394 / QTBUG-112432:
                    // Magic Mouse pretends to be a trackpad but doesn't work with PinchHandler
                    // and we don't yet distinguish mice and trackpads on Wayland either
                    acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"  || Qt.platform.pluginName === "xcb"
                                     ? PointerDevice.Mouse | PointerDevice.TouchPad
                                     : PointerDevice.Mouse
                    rotationScale: 1/120
                    property: "zoomLevel"
                }
                DragHandler {
                    id: drag
                    target: null
                    onTranslationChanged: (delta) => {
                                              map.pan(-delta.x, -delta.y);
                                              centerToCurrentPosition = false;
                                          }
                }
                TapHandler {
                    id: tapHandler
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.WithinBounds
                    onTapped: {
                        // deselection of transmitter
                        tii.selectedRow = -1;
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
                    coordinate: tii.currentPosition
                    opacity: 1.0
                    anchorPoint: Qt.point(sourceItem.width/2, sourceItem.height/2)
                    visible: tii.positionValid
                }

                MapItemView {
                    model: tiiTable
                    delegate: TransmitterMarker {
                        id: marker
                        parent: map
                        coordinate:  coordinates
                        tiiCode: tiiString
                        markerColor: levelColor
                        isSelected: index === tii.selectedRow
                        z: (index === tii.selectedRow) ? 2 : 1

                        TapHandler {
                            id: txTapHandler
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.WithinBounds
                            onTapped: {
                                tii.selectedRow = index;
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
                    opacity: infoHoverHandler.hovered ? 1.0 : 0.6
                    width: infoLayout.width + 10
                    height: infoLayout.height + 10
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 10
                    anchors.topMargin: 10
                    visible: tii.ensembleInfo[0] !== ""
                    z: 3

                    ColumnLayout {
                        id: infoLayout
                        anchors.centerIn: parent
                        Repeater {
                            model: tii.ensembleInfo
                            delegate: Text {
                                Layout.fillWidth: true
                                text: modelData
                            }
                        }
                    }
                }

                Rectangle {
                    id: txInfoBox

                    HoverHandler {
                        id: txInfoHoverHandler
                    }

                    color: "white"
                    opacity: txInfoHoverHandler.hovered ? 1.0 : 0.6
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 10
                    anchors.bottomMargin: 10
                    width: txInfoLayout.width + 10
                    height: txInfoLayout.height + 10

                    visible: tii.txInfo.length > 0
                    z: 3
                    ColumnLayout {
                        id: txInfoLayout
                        anchors.centerIn: parent
                        Repeater {
                            model: tii.txInfo
                            delegate: Text {
                                Layout.fillWidth: true
                                text: modelData
                            }
                        }
                    }
                }

                Item {
                    id: tiiTableItem
                    FontMetrics {
                        id: fontMetrics
                    }
                    HoverHandler {
                        id: tiiTableItemHoverHandler
                    }
                    opacity: tiiTableItemHoverHandler.hovered ? 1.0 : 0.75

                    readonly property int rowHeight: fontMetrics.font.pointSize + 10

                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 10
                    anchors.topMargin: 10
                    width: 5*100
                    height: (tiiTableItem.rowHeight * tiiTableView.model.rowCount) + horizontalHeader.height
                    visible: tiiTableView.model.rowCount > 0
                    z: 3

                    property var columnWidths: []
                    Component.onCompleted: {
                        for (var n = 0; n < tiiTableView.model.columnCount(); ++n) {
                            columnWidths[n] = Math.ceil(fontMetrics.boundingRect(tiiTableView.model.headerData(n, Qt.Horizontal, Qt.DisplayRole)).width + (n < 2 ? 15 : 25));
                        }
                        columnWidths[0] = Math.max(columnWidths[0], columnWidths[1]);
                        columnWidths[1] = columnWidths[0];
                        tiiTableView.forceLayout();
                    }

                    HorizontalHeaderView {
                        id: horizontalHeader
                        anchors.left: tiiTableView.left
                        anchors.top: parent.top
                        syncView: tiiTableView
                        clip: true                        
                        property int sortIndicatorColumn: 2
                        property int sortIndicatorOrder: Qt.DescendingOrder
                        Component.onCompleted: {
                            tiiTableView.model.sort(sortIndicatorColumn, sortIndicatorOrder);
                        }
                        delegate: Rectangle {
                            color: "gainsboro"
                            implicitWidth: 10
                            implicitHeight: tiiTableItem.rowHeight
                            Text {
                                property string indicator: (index === horizontalHeader.sortIndicatorColumn)
                                                           ? " " + (horizontalHeader.sortIndicatorOrder === Qt.AscendingOrder ? String.fromCodePoint(0x23F6) : String.fromCodePoint(0x23F7))
                                                           : "";
                                anchors.centerIn: parent
                                text: display
                                        ? display + indicator
                                        : ""
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked:{
                                    console.log("onClicked", index, horizontalHeader.sortIndicatorColumn, horizontalHeader.sortIndicatorOrder);
                                    if (index === horizontalHeader.sortIndicatorColumn) {
                                        // change order
                                        horizontalHeader.sortIndicatorOrder = (horizontalHeader.sortIndicatorOrder === Qt.AscendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder);
                                    }
                                    else {
                                        horizontalHeader.sortIndicatorOrder = Qt.AscendingOrder;
                                        horizontalHeader.sortIndicatorColumn = index;
                                    }
                                    console.log("===> onClicked", index, horizontalHeader.sortIndicatorColumn, horizontalHeader.sortIndicatorOrder);
                                    tiiTableView.model.sort(index, horizontalHeader.sortIndicatorOrder);
                                }
                            }
                        }
                    }
                    TableView {
                        id: tiiTableView
                        anchors.left: parent.left
                        anchors.top: horizontalHeader.bottom
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: tiiTableSorted
                        selectionModel: tiiTableSelectionModel                        
                        delegate: Rectangle {
                            implicitWidth: 10
                            implicitHeight: tiiTableItem.rowHeight
                            required property bool current
                            required property bool selected
                            color: selected ? palette.highlight : palette.base
                            Text {
                                anchors.centerIn: parent
                                text: display ? display : ""
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    tiiTableView.selectionModel.select(tiiTableView.model.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current | ItemSelectionModel.Rows);

                                }
                            }
                        }

                        columnWidthProvider: function (column) { return tiiTableItem.columnWidths[column] }
                    }
                }
            }
        }
    }
}
