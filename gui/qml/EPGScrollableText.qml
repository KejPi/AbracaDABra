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
import QtQuick.Controls
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5
// import QtQuick.Effects              // not available in Qt < 6.5


Item {
    id: control
    property alias text: label.text

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.rightMargin: vbar.width
        contentHeight: label.height
        contentWidth: width
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        visible: false
        clip: true
        Text {
            id: label
            color: EPGColors.textColor
            width: flickable.width
            clip: true
            wrapMode: Text.WordWrap
        }
        //ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        //ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            id: vbar
            parent: flickable.parent
            anchors.top: flickable.top
            anchors.left: flickable.right
            anchors.bottom: flickable.bottom
            policy: flickable.contentHeight > flickable.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }
    }
    LinearGradient {
        id: mask
        anchors.fill: flickable
        gradient: Gradient {
            GradientStop {
                position: -0.05
                color: "transparent"
            }
            GradientStop {
                position: (flickable.contentY > 0) ? 0.15 : 0.0
                color: "black"
            }
            GradientStop {
                position: (flickable.contentY < (flickable.contentHeight - flickable.height - 2)) ? 0.85 : 1.0
                color: "black"
            }
            GradientStop {
                position: 1.05
                color: "transparent"
            }
        }
        visible: false
    }
    OpacityMask {
        anchors.fill: flickable
        source: flickable
        maskSource: mask
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event)=>{flickable.flick(0, event.angleDelta.y * event.y * 0.2)}
    }
}
