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
import QtQuick.Effects

import abracaComponents

Dialog {
    id: dialog

    property bool canEscape: false
    property int contentMargin: UI.standardMargin * 2

    parent: Overlay.overlay
    anchors.centerIn: parent
    closePolicy: canEscape ? Popup.CloseOnPressOutside : Popup.NoAutoClose

    topInset: -10
    bottomInset: -10
    rightInset: -10
    leftInset: -10

    spacing: 0

    topPadding: title.length > 0 ? contentMargin/2 : contentMargin
    leftPadding: contentMargin
    rightPadding: contentMargin
    bottomPadding: contentMargin    

    header: Item {
        id: headerRect
        implicitHeight: dialog.title.length > 0 ? titleLabel.height * 2 : 0
        visible: dialog.title.length > 0
        Text {
            id: titleLabel
            visible: dialog.title.length > 0
            color: UI.colors.textPrimary
            text: dialog.title
            font.bold: true
            font.pointSize: UI.largeFontPointSize
            anchors.top: headerRect.top
            anchors.topMargin: contentMargin // height/2
            anchors.left: headerRect.left
            anchors.leftMargin: contentMargin
        }
        AbracaImgButton {
            id: closeButton
            visible: dialog.canEscape
            source: UI.imagesUrl + "close.svg"
            colorizationColor: UI.colors.iconInactive

            anchors.right: headerRect.right
            anchors.rightMargin: dialog.rightPadding - (closeButton.width - closeButton.iconSize)/2 - 4
            anchors.verticalCenter: titleLabel.verticalCenter

            onClicked: reject()
        }
    }

    background: Item {
        id: backgroundItem
        implicitWidth: 200
        implicitHeight: 200
        Rectangle {
            id: backgroundRect
            anchors.fill: backgroundItem
            anchors.margins: 10
            color: UI.colors.background
            radius: UI.controlHeight / 2            
            border.width: UI.colors.background < "#808080" ? 1 : 0
            border.color: UI.colors.divider
        }
        MultiEffect {
            source: backgroundRect
            anchors.fill: backgroundRect
            autoPaddingEnabled: true
            shadowBlur: 1.0
            shadowColor: 'black'
            shadowEnabled: true
            shadowOpacity: 0.5
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 5
            visible: true
        }
    }

    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.9; to: 1.0; easing.type: Easing.OutQuint; duration: 220 }
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutCubic; duration: 150 }
    }

    exit: Transition {
        NumberAnimation { property: "scale"; from: 1.0; to: 0.9; easing.type: Easing.OutQuint; duration: 220 }
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.OutCubic; duration: 150 }
    }

    Overlay.modal: Rectangle {
        color: Qt.alpha(UI.colors.inactive, 0.5)
    }

    Overlay.modeless: Rectangle {
        color: Qt.alpha(UI.colors.inactive, 0.12)
    }

    function accept() {
        dialog.close();
        dialog.accepted();
    }
    function reject() {
        dialog.close();
        dialog.rejected();
    }
}
