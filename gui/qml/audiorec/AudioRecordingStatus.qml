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
import QtQuick.Layouts

import abracaComponents 1.0

Item {
    id: mainItem
    property bool showLabel: true
    implicitHeight: 24
    implicitWidth: mainLayout.implicitWidth
    RowLayout {
        id: mainLayout
        visible: true
        // anchors.top: parent.top
        // anchors.bottom: parent.bottom
        // anchors.left: parent.left
        anchors.fill: parent

        Connections {
            target: appUI
            function onAudioRecordingProgressLabelChanged() {
                if (appUI.audioRecordingProgressLabel === "0:00") {
                    recButton.trigger = true
                } else {
                    recButton.trigger = !recButton.trigger
                }
            }
        }

        AbracaRecordingButton {
            id: recButton
            Layout.preferredHeight: mainItem.height
            Layout.preferredWidth: Layout.preferredHeight
            interactive: true
            onClicked: application.audioRecordingToggle()
        }

        AbracaLabel {
            visible: mainItem.showLabel
            text: qsTr("Audio recording:")
            toolTipText: appUI.audioRecordingProgressLabelToolTip
        }
        AbracaLabel {
            id: recordingTimeLabel
            text: appUI.audioRecordingProgressLabel
            toolTipText: appUI.audioRecordingProgressLabelToolTip
            Layout.preferredWidth: fontMetrics.boundingRect("  00:00").width
            FontMetrics {
                id: fontMetrics
                font: recordingTimeLabel.font
            }
        }
    }

}
