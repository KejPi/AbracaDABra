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
import QtQuick.Controls.Basic

import abracaComponents

AbracaDialog {
    id: bandScanDialog
    property var backend: null

    x: (appWindow.width - width) / 2
    y: (appWindow.height - height) / 2
    width: Math.min(appWindow.width*0.8, defaultWidth + 4*bandScanDialog.leftPadding)
    parent: Overlay.overlay

    readonly property int defaultWidth: (startButton.implicitWidth + stopCancelButton.implicitWidth + clearServiceListSwitch.implicitWidth + UI.standardMargin * 3)

    modal: true
    title: qsTr("Band scan")
    closePolicy: Popup.NoAutoClose

    signal finished()

    Connections {
        target: backend
        function onDone(result) {
            finished()
        }
    }

    ColumnLayout {
        id: mainLayout
        spacing: 20
        anchors.fill: parent

        TextMetrics {
            id: metrics
            font: Qt.application.font
            text: "200"
        }

        RowLayout {
            AbracaLabel {
                text: backend.currentChannel
                textFormat: Text.RichText
            }
            Item { Layout.fillWidth: true }
            AbracaLabel {
                visible: backend.isScanning
                text: backend.channelProgress
            }
        }
        AbracaProgressBar {
            Layout.fillWidth: true
            from: 0.0
            to: 100.0
            value: backend.progress
        }
        GridLayout {
            columns: 2
            AbracaLabel {
                text: qsTr("Ensembles found:")
            }
            AbracaLabel {
                Layout.preferredWidth: metrics.width
                horizontalAlignment: Text.AlignRight
                text: backend.ensemblesFound
            }

            AbracaLabel {
                text: qsTr("Services found:")
            }
            AbracaLabel {
                Layout.preferredWidth: metrics.width
                text: backend.servicesFound
                horizontalAlignment: Text.AlignRight
            }
        }
        GridLayout {
            id: buttonLayout
            Layout.fillWidth: true
            columns: defaultWidth > mainLayout.width ? 1 : 4

            AbracaSwitch {
                id: clearServiceListSwitch
                text: qsTr("Clear service list on start")
                // enabled: !backend.isScanning
                checked: backend.keepServiceListOnScan === false
                onCheckedChanged: {
                    if (checked === backend.keepServiceListOnScan) {
                        backend.keepServiceListOnScan = !checked
                    }
                }
            }                        
            Item { Layout.fillWidth: true }

            AbracaButton {
                id: stopCancelButton
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: buttonLayout.columns === 1
                text: backend.isScanning ? qsTr("Stop") :  qsTr("Cancel")
                onClicked: {
                    if (backend.isScanning ) {
                        backend.stopScan()
                    }
                    else {
                        backend.cancelScan()
                    }
                }
            }
            AbracaButton {
                id: startButton
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: buttonLayout.columns === 1
                text: qsTr("Start")
                buttonRole: UI.ButtonRole.Primary
                enabled: !backend.isScanning
                onClicked: {
                    backend.startScan()
                }
            }
/*
            Loader {
                active: true
                sourceComponent: UI.isWindows === false ? stopCancelButtonComponent : startButtonComponent
            }
            Loader {
                active: true
                sourceComponent: UI.isWindows === false ? startButtonComponent : stopCancelButtonComponent
            }

            Component {
                id: startButtonComponent
                AbracaButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: buttonLayout.columns === 1
                    text: qsTr("Start")
                    buttonRole: UI.ButtonRole.Primary
                    enabled: !backend.isScanning
                    onClicked: {
                        backend.startScan()
                    }
                }
            }
            Component {
                id: stopCancelButtonComponent
                AbracaButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: buttonLayout.columns === 1
                    text: backend.isScanning ? qsTr("Stop") :  qsTr("Cancel")
                    onClicked: {
                        if (backend.isScanning ) {
                            backend.stopScan()
                        }
                        else {
                            backend.cancelScan()
                        }
                    }
                }
            }
*/
        }
    }
}

