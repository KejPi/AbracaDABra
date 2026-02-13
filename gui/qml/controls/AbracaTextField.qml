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

TextField {
    id: control

    property string defaultText: ""

    selectByMouse: true
    color: control.enabled ? UI.colors.textPrimary : UI.colors.textDisabled
    selectionColor: control.enabled ? UI.colors.selectionColor : UI.colors.inputBackground
    selectedTextColor: control.enabled ? UI.colors.textSelected : control.color
    rightPadding: 2*UI.standardMargin + UI.iconSize

    readonly property bool iconActive: defaultText.length > 0 ? control.text !== defaultText : control.text.length > 0

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: UI.controlHeight
        color: UI.colors.inputBackground
        border.color: control.enabled && (control.hovered || control.activeFocus) ? UI.colors.accent : UI.colors.inactive
        border.width: 1
        radius: UI.controlRadius
        opacity: control.enabled ? 1.0 : 0.5

        AbracaColorizedImage {
            id: clearIcon
            source: UI.imagesUrl + (defaultText !== "" ? "reload.svg" : "close.svg")
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: UI.standardMargin
            width: UI.iconSize
            height: UI.iconSize
            sourceSize.width: width
            sourceSize.height: height
            colorizationColor: control.iconActive ? UI.colors.iconInactive : UI.colors.iconDisabled
        }
    }
    MouseArea {
        //anchors.fill: clearIcon
        x: background.width - control.rightPadding
        width: control.rightPadding
        height: background.height
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        propagateComposedEvents: false
        onClicked: {
            if (defaultText !== "") {
                control.text = defaultText
                editingFinished();
            }
            else {
                control.clear()
                editingFinished();
                control.forceActiveFocus()
            }
        }
    }
}
