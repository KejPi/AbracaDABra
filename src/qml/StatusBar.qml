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
import QtQuick.Controls

import abracaComponents 1.0

Rectangle {
    id: statusBar
    color: UI.colors.statusbarBackground
    implicitHeight: UI.isDesktop ? UI.controlHeight : signalState.implicitHeight + 2*5

    RowLayout {
        id: statusLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 10
        anchors.rightMargin: anchors.leftMargin
        spacing: 8
        SignalState {
            id: signalState
        }
        StackLayout {
            currentIndex: appUI.infoLabelIndex
            AbracaLabel {
                id: timeLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:  Text.AlignVCenter
                text: appUI.timeLabel
                toolTipText: appUI.timeLabelToolTip
            }
            AbracaLabel {
                id: infoLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment:  Text.AlignVCenter
                text: appUI.messageInfoTitle
                toolTipText: appUI.messageInfoDetails
                font.bold: true
            }
        }
        AudioRecordingStatus {
            visible: audioRecording.isAudioRecordingActive
            Layout.preferredWidth: implicitWidth
            //Layout.fillWidth: true;
            Layout.preferredHeight: 22
        }

        RowLayout {
            id: volumeControl
            visible: UI.isDesktop === true
            property string volumeIcon: {
                if (volumeSlider.value > 80) {
                    return UI.imagesUrl + "volume-3.svg"
                }
                if (volumeSlider.value > 35) {
                    return  UI.imagesUrl + "volume-2.svg"
                }
                if (volumeSlider.value === 0) {
                    return UI.imagesUrl + "volume-0.svg"
                }
                return UI.imagesUrl + "volume-1.svg"
            }
            property int volumeIconId: 3
            AbracaImgButton {
                id: muteButton
                checkable: true
                checked: appUI.isMuted
                sourceChecked: UI.imagesUrl + "volume-0.svg"
                sourceUnchecked: volumeControl.volumeIcon
                toolTipChecked: qsTr("Unmute audio")
                toolTipUnchecked: qsTr("Mute audio")
                onToggled: application.onMuteButtonToggled(checked)
                Connections {
                    target: appUI
                    function isMutedChanged() {
                        if (muteButton.checked !== appUI.isMuted) {
                            muteButton.checked = appUI.isMuted
                        }
                    }
                }
            }
            AbracaSlider {
                id: volumeSlider                
                Layout.preferredWidth: 100
                //Layout.preferredHeight: muteButton.height
                orientation: Qt.Horizontal
                enabled: appUI.isMuted === false
                from: 0
                to: 100
                stepSize: 10
                value: appUI.isMuted ? 0 : appUI.audioVolume
                onValueChanged: {
                    if (appUI.isMuted === false) {
                        appUI.audioVolume = volumeSlider.value
                    }
                }
                Connections {
                    target: appUI
                    function audioVolumeChanged() {
                        if (muteButton.value !== appUI.audioVolume) {
                            muteButton.value = appUI.audioVolume
                        }
                    }
                }
            }
        }
    }
}
