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

Item {
    anchors.fill: parent

    // title: map.center + " zoom " + map.zoomLevel.toFixed(3)
    //        + " min " + map.minimumZoomLevel + " max " + map.maximumZoomLevel

    // LocationPermission {
    //     id: permission
    //     accuracy: LocationPermission.Precise
    //     availability: LocationPermission.Always
    // }

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
        center: QtPositioning.coordinate(50.08804, 14.42076) // Prague
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
            acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                             ? PointerDevice.Mouse | PointerDevice.TouchPad
                             : PointerDevice.Mouse
            rotationScale: 1/120
            property: "zoomLevel"
        }
        DragHandler {
            id: drag
            target: null
            onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
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

        // //----------------------
        // PositionSource{
        //     id: positionSource
        //     active: true // followme
        //     onPositionChanged: {
        //         console.log("onPositionChanged")
        //         map.center = positionSource.position.coordinate
        //     }
        // }
        // MapQuickItem {
        //     id: mePoisition
        //     parent: map
        //     sourceItem: Rectangle { width: 14; height: 14; color: "#251ee4"; border.width: 2; border.color: "white"; smooth: true; radius: 7 }
        //     coordinate: positionSource.position.coordinate
        //     opacity: 1.0
        //     anchorPoint: Qt.point(sourceItem.width/2, sourceItem.height/2)
        //     visible: true//followme
        // }
        // MapQuickItem {
        //     parent: map
        //     sourceItem: Text{
        //         text: qsTr("You're here!")
        //         color:"#242424"
        //         font.bold: true
        //         styleColor: "#ECECEC"
        //         style: Text.Outline
        //     }
        //     coordinate: positionSource.position.coordinate
        //     anchorPoint: Qt.point(-mePoisition.sourceItem.width * 0.5, mePoisition.sourceItem.height * 1.5)
        //     visible: true // followme
        // }

        // Component.onCompleted: permission.request();
    }
}
