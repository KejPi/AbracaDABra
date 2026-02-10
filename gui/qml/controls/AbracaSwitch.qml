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

import QtQuick
//import QtQuick.Controls.Basic
import QtQuick.Controls
import abracaComponents

Switch {
    id: control
    property string toolTipText: ""
    readonly property int labelX: control.leftPadding + control.indicator.width + UI.standardMargin
    property int elideMode: Text.ElideRight
    property int wrapMode: Text.NoWrap

    text: "Text"
    checked: false

    // ToolTip
    hoverEnabled: true        

    indicator: Rectangle {
        id: indicatorRect

        // this is for corerct opacity handling
        layer.enabled: !control.enabled
        opacity: control.enabled ? 1.0 : 0.5


        implicitWidth: 1.8*implicitHeight
        implicitHeight: UI.controlHeightSmall
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: indicatorRect.height / 2
        color: control.checked ? UI.colors.accent : UI.colors.inactive

        Rectangle {
            id: handleRect
            x: control.checked ? parent.width - width : 0
            width: indicatorRect.height
            height: indicatorRect.height
            radius: indicatorRect.height / 2
            color: UI.colors.buttonTextPrimary
            border.color: control.checked ? UI.colors.accent : UI.colors.inactive
            border.width: 3 // control.checked ? 2 : 4
            Behavior on x {
                NumberAnimation { duration: 150 }
            }
        }
        // Rectangle {
        //     id: hoverRect
        //     anchors.centerIn: handleRect
        //     width: indicatorRect.height + 10
        //     height: width
        //     radius: width / 2
        //     color: "transparent"
        //     border.color: control.checked ?  UI.colors.accent : UI.colors.inactive
        //     border.width: 5
        //     opacity: control.hovered ? 0.2 : 0.0
        //     Behavior on opacity {
        //         NumberAnimation { duration: 150 }
        //     }
        // }
    }

    contentItem: AbracaLabel {
        text: control.text
        enabled: control.enabled
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + UI.standardMargin
        elide: control.elideMode
        wrapMode: control.wrapMode
    }
}
