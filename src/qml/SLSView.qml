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

Rectangle {

    property var backend: undefined
    property string slsSource: ""

    implicitWidth: 320
    implicitHeight: 240

    color: backend.backgroundColor
    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: slsSource

        AbracaToolTip {
            text: backend.toolTip
            visible: slsMouseArea.containsMouse && !contextMenu.visible
            hoverMouseArea: slsMouseArea
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


