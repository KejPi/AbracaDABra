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

MenuItem {
    id: control

    property url iconSource: ""
    readonly property bool showIcon: iconSource !== ""

    leftInset: 2
    rightInset: 2
    topInset: 2
    bottomInset: 2

    implicitHeight: UI.controlHeight    

    indicator: Item {
        id: indicatorItem
        implicitWidth: control.checkable ? UI.controlHeight : 0
        implicitHeight: UI.controlHeight
        visible: control.checkable
        AbracaColorizedImage {
            id: checkmarkImage
            source: UI.imagesUrl + "check.svg"
            colorizationColor: enabled ? UI.colors.textPrimary : UI.colors.textDisabled
            anchors.centerIn: indicatorItem
            width: UI.iconSize
            height: UI.iconSize
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
            visible: control.checked
        }
    }

    arrow: AbracaColorizedImage {
        id: arrowImage
        source: UI.imagesUrl + "chevron-right.svg"
        colorizationColor: enabled ? UI.colors.textSecondary : UI.colors.textDisabled
        width: UI.iconSize
        height: UI.iconSize
        fillMode: Image.PreserveAspectFit
        sourceSize.width: width
        sourceSize.height: height
        x: control.width - width - control.rightInset
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.subMenu
    }

    contentItem: Item {
        implicitWidth: label.implicitWidth
        AbracaColorizedImage {
            id: icon
            colorizationColor: enabled ? UI.colors.textSecondary : UI.colors.textDisabled
            width: UI.iconSize
            height: UI.iconSize
            fillMode: Image.PreserveAspectFit
            sourceSize.width: width
            sourceSize.height: height
            source: control.iconSource
            visible: !control.checkable && control.showIcon
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: Math.max(control.indicator.width + icon.visible ? (UI.iconSize + UI.standardMargin) : 0, UI.standardMargin)
            rightPadding: control.arrow.width
            text: control.text
            font: control.font
            color: enabled ? UI.colors.textPrimary : UI.colors.textDisabled
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: UI.controlHeight
        radius: UI.controlRadius
        color: control.highlighted ? UI.colors.listItemHovered : "transparent"
    }
}
