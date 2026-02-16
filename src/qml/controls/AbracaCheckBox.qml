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

CheckBox {
    id: control

    onPressed: forceActiveFocus()

    indicator: Item {
        id: indicatorItem
        implicitWidth: UI.controlHeightSmaller
        implicitHeight: UI.controlHeightSmaller
        x: control.leftPadding
        y: parent.height / 2 - height / 2

        Rectangle {
            id: boxRect
            color: checked ? (control.hovered ?  UI.colors.buttonPrimaryHover : UI.colors.buttonPrimary) : "transparent"
            border.color: enabled ? (control.checked ? UI.colors.accent : UI.colors.textPrimary) : UI.colors.textDisabled
            border.width: checked ? 0 : 1
            radius: UI.controlRadius / 2
            anchors.fill: parent
            anchors.margins: (UI.controlHeightSmaller - UI.controlHeightSmall) / 2
            AbracaColorizedImage {
                id: checkmarkImage
                source: UI.imagesUrl + "check.svg"
                colorizationColor: enabled ? UI.colors.buttonTextPrimary : UI.colors.textDisabled
                anchors.centerIn: boxRect
                width: UI.controlHeightSmall
                height: UI.controlHeightSmall
                fillMode: Image.PreserveAspectFit
                sourceSize.width: width
                sourceSize.height: height
                visible: control.checked
            }
        }
    }

    contentItem: AbracaLabel {
        text: control.text
        enabled: control.enabled
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width
    }
}


