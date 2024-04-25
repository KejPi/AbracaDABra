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

import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Effects

MapQuickItem {
    property string tiiCode: ""
    property color markerColor: "gray"
    property bool isSelected: false

    HoverHandler {
        id: hoverHandler
    }

    //parent: map
    sourceItem: Item {
        width: img.width
        height: img.height
        Image {
            id: img
            source: "resources/map_marker_"+ tiiCode.length + ".png"
            visible: false
        }
        MultiEffect {
            id: colorizationEffect
            source: img
            anchors.fill: img
            opacity: (hoverHandler.hovered || isSelected) ? 1.0 : 0.6
            colorizationColor: markerColor
            colorization: 0.5
            shadowEnabled: true          // QTBUG-124730
            shadowOpacity: isSelected    // QTBUG-124730
        }
        Text{
            id: txtTII
            y: (19-height)/2
            anchors.horizontalCenter: parent.horizontalCenter
            text: tiiCode
            opacity: (hoverHandler.hovered || isSelected) ? 1.0 : 0.6
            horizontalAlignment: Text.AlignHCenter
        }
    }
    anchorPoint: Qt.point(sourceItem.width/2, sourceItem.height)
    visible: true
}

