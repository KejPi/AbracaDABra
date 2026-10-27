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
import abracaComponents 1.0

Item {
    property var backend: undefined
    property string slsSource: ""
    readonly property double preferredHeight: width / 4 * 3 + progressBar.height

    implicitWidth: 320
    implicitHeight: 240 + (progressBar.visible ? progressBar.height : 0)

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: progressBar.visible ? progressBar.height : 0

        color: backend.backgroundColor
        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            source: slsSource
            Accessible.role: Accessible.Graphic
            Accessible.name: backend.toolTip !== "" ? backend.toolTip : qsTr("Slideshow image")

            AbracaToolTip {
                text: backend.toolTip
                visible: slsMouseArea.containsMouse && !contextMenu.visible
            }
            AbracaMenu {
                id: contextMenu
                AbracaMenuItem {
                    text: qsTr("Save to file")
                    onTriggered: backend.saveSlideToFile()
                }
                AbracaMenuItem {
                    text: qsTr("Copy to clipboard")
                    onTriggered: backend.copySlideToClipboard()
                }
            }
            MouseArea {
                id: slsMouseArea
                anchors.fill: parent
                hoverEnabled: backend.toolTip.length > 0
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: backend.cursorShape
                onClicked: (mouse)=>{
                    if (mouse.button === Qt.LeftButton) {
                        backend.handleLeftClick()
                    }
                    else if (mouse.button === Qt.RightButton) {
                        if (backend.handleRightClick()) {
                            contextMenu.popup()
                        }
                    }
                }
                onPressAndHold: (mouse) => {
                    if (mouse.source === Qt.MouseEventNotSynthesized) {
                        if (backend.handleRightClick()) {
                            contextMenu.popup()
                        }
                    }
                }
            }
        }
    }
    Item {
        id: progressBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        opacity: (backend.totalBytes < 0 || backend.decodedBytes === backend.totalBytes) ? 0.0 : 0.5
        Behavior on opacity { NumberAnimation { duration: 1000; easing.type: Easing.InOutQuad } }
        visible: backend.progressEnabled
        height: backend.progressEnabled ? 8 : 0
        Rectangle {
            // visible: backend.totalBytes > 0
            height: progressBar.height - 4
            anchors.left: progressBar.left
            anchors.bottom: progressBar.bottom
            width: Math.min(Math.max(0, progressBar.width * backend.decodedBytes/backend.totalBytes), progressBar.width)
            color: UI.colors.textSecondary
        }
        AbracaToolTip {
            text: qsTr("Decoded %1 of %2 bytes (%3%)").arg(backend.decodedBytes).arg(backend.totalBytes).arg(Math.round(backend.decodedBytes*100.0/backend.totalBytes))
            visible: progressMouseArea.containsMouse
        }
        MouseArea {
            id: progressMouseArea
            anchors.fill: parent
            hoverEnabled: backend.totalBytes > 0
        }
    }
}


