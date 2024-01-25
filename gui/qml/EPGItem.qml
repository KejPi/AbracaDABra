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
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5
// import QtQuick.Effects              // not available in Qt < 6.5

Item {
    id: progItem

    property double pointsPerSec: 0.0
    signal clicked
    property bool isSelected: false
    property int itemHeight: 50
    property int viewX: 0

    //property color pastProgColor: "#e4e4e4"
    property color pastProgColor: EPGColors.pastProgColor
    property color nextProgColor: EPGColors.nextProgColor
    property color currentProgColor: EPGColors.currentProgColor
    property color progressColor: EPGColors.progressColor
    property color borderColor: EPGColors.gridColor
    property color selectedBorderColor: EPGColors.selectedBorderColor

    height: itemHeight
    clip: true
    width: mouseAreaId.containsMouse ? Math.max(textId.x + textId.width + 5, durationSec * pointsPerSec) : durationSec * pointsPerSec
    x:  startTimeSec * pointsPerSec
    z: mouseAreaId.containsMouse ? 1000 : index

    Rectangle {
        id: rect
        property bool textFits: (textId.x + textId.width + 5) <= width
        color: {
            if (endTimeSecSinceEpoch <= currentTimeSec) return pastProgColor;
            if (startTimeSecSinceEpoch >= currentTimeSec) return nextProgColor;
            return currentProgColor;
        }
        border.color: isSelected ? selectedBorderColor : borderColor
        border.width: isSelected ? 2 : 1
        anchors.fill: parent
        x:  startTimeSec * pointsPerSec

        Rectangle {
            id: remainingTime
            visible: (startTimeSecSinceEpoch < currentTimeSec) && (endTimeSecSinceEpoch > currentTimeSec)
            color: progressColor
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                topMargin: isSelected ? 2 : 1
                bottomMargin: isSelected ? 2 : 1
                leftMargin: isSelected ? 2 : 1
            }
            width: mouseAreaId.containsMouse
                   ? ((currentTimeSec - startTimeSecSinceEpoch) / durationSec) * parent.width - (isSelected ? 2 : 1)
                   : ((currentTimeSec - startTimeSecSinceEpoch) * pointsPerSec - 1)
        }
        Text {
            id: textId
            anchors {
                //left: parent.left
                //leftMargin: 5
                top: parent.top
                bottom: parent.bottom
            }
            x: (viewX > startTimeSec * pointsPerSec) && (viewX < (endTimeSec * pointsPerSec + 20))
                ? (viewX - startTimeSec * pointsPerSec + 5) : (parent.x + 5)
            text: name
            //font.bold: isSelected
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft
        }
        clip: true
        visible: true
    }

    Rectangle {
        id: shadowItem
        color: "black"
        anchors.left: rect.right
        anchors.top: rect.top
        anchors.bottom: rect.bottom
        width: 100
        visible: false
    }
    // Qt >= 6.5
    /*
    MultiEffect {
        source: shadowItem
        anchors.fill: shadowItem
        autoPaddingEnabled: true
        shadowBlur: 1.0
        shadowColor: 'black'
        shadowEnabled: true
        shadowOpacity: 0.5
        shadowHorizontalOffset: -2 // rect.shadowSize
        shadowVerticalOffset: 0
        visible: !rect.textFits
    }
    */
    DropShadow {
        id: shadowId
        anchors.fill: shadowItem
        source: shadowItem
        samples: 21
        color: "darkgray"
        enabled: true
        opacity: 0.4
        horizontalOffset: -6
        verticalOffset: 0
        visible: !rect.textFits
    }
    MouseArea {
        id: mouseAreaId
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            progItem.clicked()
        }
    }
}
