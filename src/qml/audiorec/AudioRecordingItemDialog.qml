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
import QtQuick.Controls
import QtQuick.Layouts

import abracaComponents

AbracaDialog {
    id: dialog
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    parent: Overlay.overlay
    height: parent.height * 0.9

    modal: true
    title: qsTr("Audio recording schedule")

    ColumnLayout {
        anchors.fill: parent
        spacing: UI.standardMargin
        AbracaTextField {
            id: labelField
            Layout.fillWidth: true
            placeholderText: qsTr("Scheduled item name")
            text: audioRecording.label
            onTextChanged: {
                if (audioRecording.label !== text.trim()) {
                    audioRecording.label = text.trim();
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 300
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: timeSelectionColumn.implicitWidth
                contentHeight: timeSelectionColumn.implicitHeight
                ColumnLayout {
                    id: timeSelectionColumn
                    // Start Date Picker
                    AbracaDatePicker {
                        id: eventDatePicker
                        Layout.fillWidth: true
                        Layout.preferredWidth: 300

                        label: "Date:"
                        selectedDate: audioRecording.startDate
                        onDateChanged: (newDate) => {
                            audioRecording.startDate = newDate
                        }
                    }

                    // Start Time Picker
                    AbracaTimePicker {
                        id: startTimePicker
                        Layout.fillWidth: true
                        Layout.preferredWidth: 300

                        label: "Start Time"
                        selectedTime: audioRecording.startTime
                        onTimeChanged: (newTime) => {
                            audioRecording.startTime = newTime
                        }
                    }
                    // Duration time Picker
                    AbracaTimePicker {
                        id: durationPicker
                        Layout.fillWidth: true
                        Layout.preferredWidth: 300

                        label: "Duration:"
                        selectedTime: audioRecording.duration
                        onTimeChanged: (newTime) => {
                            audioRecording.duration = newTime
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        AbracaLabel {
                            text: qsTr("Stop time:")
                        }
                        AbracaLabel {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            font.bold: true
                            text: audioRecording.stopTime
                        }
                    }
                }
            }            
            ListView {
                id: serviceList
                Layout.minimumWidth: buttonRow.width
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: slModel

                // connection to selection model
                Connections {
                    target: audioRecording.serviceSelectionModel
                    function onSelectionChanged(selected, deselected) {
                        if (audioRecording.serviceSelectionModel.hasSelection && selected.length > 0) {
                            serviceList.currentIndex = audioRecording.serviceSelectionModel.selectedIndexes[0].row
                        }
                        else {
                            serviceList.currentIndex = -1
                        }
                    }
                }
                // initial index
                Component.onCompleted: {
                    if (audioRecording.serviceSelectionModel.hasSelection) {
                        currentIndex = audioRecording.serviceSelectionModel.selectedIndexes[0].row
                    }
                    else {
                        // using current service as fallback
                        audioRecording.serviceSelectionModel.select(serviceList.model.index(slSelectionModel.currentIndex.row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current)
                    }
                }

                delegate: AbracaItemDelegate {
                    width: serviceList.width
                    highlighted: ListView.isCurrentItem
                    required property string serviceName
                    required property int index
                    text: serviceName
                    onClicked: {
                        audioRecording.serviceSelectionModel.select(serviceList.model.index(index, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current)
                    }
                }
                focus: true

                AbracaHorizontalShadow {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: parent.width
                    topDownDirection: true
                    shadowDistance: serviceList.contentY - serviceList.originY
                }
                AbracaHorizontalShadow {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: parent.width
                    topDownDirection: false
                    shadowDistance: serviceList.contentHeight - serviceList.height - (serviceList.contentY - serviceList.originY)
                }
            }
        }
        // DialogButtonBox {
        //     Layout.alignment: Qt.AlignRight
        //     AbracaButton {
        //         text: qsTr("OK")
        //         DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        //         enabled: audioRecording.label.length > 0 && audioRecording.serviceSelectionModel.hasSelection
        //     }
        //     AbracaButton {
        //         text: qsTr("Cancel")
        //         DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        //     }
        //     onAccepted: {
        //         dialog.accept();
        //     }
        //     onRejected: dialog.close()
        // }
        RowLayout {
            id: buttonRow
            Layout.alignment: Qt.AlignRight
            spacing: UI.standardMargin
            Loader {
                active: true
                sourceComponent: UI.isMacOS ? cancelButtonComponent : okButtonComponent
            }
            Loader {
                active: true
                sourceComponent: UI.isMacOS ? okButtonComponent : cancelButtonComponent
            }
            Component {
                id: okButtonComponent
                AbracaButton {
                    text: qsTr("OK")
                    buttonRole: UI.ButtonRole.Primary
                    enabled: audioRecording.serviceSelectionModel.hasSelection
                    onClicked: dialog.accept()
                }
            }
            Component {
                id: cancelButtonComponent
                AbracaButton {
                    text: qsTr("Cancel")
                    onClicked: dialog.reject()
                }
            }
        }
    }
}
