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
import QtQuick.Effects
import QtQuick.Controls

Item {
    id: recordingButton
    implicitWidth: 18
    implicitHeight: 18

    // Property to control recording state
    property bool isRecording: true
    property bool trigger: true
    property bool interactive: true

    // Signal emitted when recording is stopped
    signal clicked

    Rectangle {
        id: redCircle
        anchors.fill: parent
        anchors.margins: 4
        radius: width / 2
        color: "red"
        opacity: recordingButton.trigger ? 1.0 : 0.2

        // Smooth opacity transition
        Behavior on opacity {
            NumberAnimation {
                duration: 100
                easing.type: Easing.InOutQuad
            }
        }
        AbracaToolTip {
            text: qsTr("Stop recording")
            hoverMouseArea: mouseArea
        }
        MouseArea {
            id: mouseArea
            enabled: recordingButton.interactive
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: enabled ? Qt.PointingHandCursor : undefined
            onClicked: {
                if (recordingButton.isRecording) {
                    recordingButton.clicked();
                }
            }
        }
    }

    Rectangle {
        anchors.centerIn: redCircle
        width: redCircle.width + 4
        height: redCircle.height + 4
        radius: width / 2
        color: "transparent"
        border.color: "red" // "#E74C3C"
        border.width: 2
        opacity: redCircle.opacity * 0.3
    }
}
