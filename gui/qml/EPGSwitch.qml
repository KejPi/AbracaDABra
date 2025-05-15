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

import QtQuick
import QtQuick.Controls

Switch {
    id: control
    text: "Text"
    checked: false
    //property int spacing: 5

    indicator: Rectangle {
        implicitWidth: 28
        implicitHeight: 16
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 8
        color: control.checked ? EPGColors.highlightColor : EPGColors.switchBgColor
        border.color: control.checked ? EPGColors.highlightColor : EPGColors.switchBorderColor

        Rectangle {
            x: control.checked ? parent.width - width : 0
            width: 16
            height: 16
            radius: 8
            color: control.down ? EPGColors.switchHandleDownColor : EPGColors.switchHandleColor
            border.color: control.checked ? (control.down ? EPGColors.currentProgColor : EPGColors.highlightColor) : EPGColors.switchHandleBorderColor
        }
    }

    contentItem: Text {
        text: control.text
        //font: control.font
        color: EPGColors.textColor
        opacity: enabled ? 1.0 : 0.3
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
