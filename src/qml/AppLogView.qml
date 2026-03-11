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
import QtQuick.Layouts
import QtCore

import abracaComponents 1.0


Item {
    id: mainItem
    anchors.fill: parent
    anchors.margins: UI.standardMargin
    ListView {
        id: logListView
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: buttonRow.top
        anchors.bottomMargin: UI.standardMargin
        model: log.logModel
        clip: true
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.DragAndOvershootBounds
        contentWidth: Math.max(width, contentItem.childrenRect.width)

        function clampHorizontalBounds() {
            const minX = originX
            const maxX = Math.max(minX, originX + contentWidth - width)
            const boundedX = Math.max(minX, Math.min(contentX, maxX))
            if (contentX !== boundedX) {
                contentX = boundedX
            }
        }

        delegate: Text {
            width: implicitWidth
            height: implicitHeight
            text: logMsg
            color: textColor ? textColor : UI.colors.textPrimary
            font.family: UI.fixedFontFamily
            font.pointSize: Qt.application.font.pointSize - 2
        }
        onContentXChanged: clampHorizontalBounds()
        onCountChanged: positionViewAtEnd()

    }
    RowLayout {
        id: buttonRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        AbracaButton {
            text: qsTr("Save to file")
            onClicked: log.saveLogToFile()
        }
        AbracaButton {
            text: qsTr("Copy to clipboard")
            onClicked: log.copyToClipboard()
        }
        Item { Layout.fillWidth: true }
        AbracaButton {
            text: qsTr("Clear log")
            onClicked: log.clearLog()
        }
    }
}
