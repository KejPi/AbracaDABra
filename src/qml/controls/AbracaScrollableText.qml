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
import QtQuick.Controls.Basic

import abracaComponents

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
        visible: true
        clip: true
        Text {
            id: label
            color: UI.colors.textPrimary
            width: flickable.width
            clip: true
            wrapMode: Text.WordWrap
        }
        //ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        //ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: AbracaScrollBar {
            id: vbar
            parent: flickable.parent
            anchors.top: flickable.top
            anchors.left: flickable.right
            anchors.bottom: flickable.bottom
            policy: flickable.contentHeight > flickable.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: parent.width
        topDownDirection: true
        shadowDistance: flickable.contentY
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: parent.width
        topDownDirection: false
        shadowDistance: flickable.contentHeight - flickable.height - flickable.contentY
    }
}
