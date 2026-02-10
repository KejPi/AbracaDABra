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
import QtQuick.Layouts

import abracaComponents

AbracaDialog {
    id: dialog
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    modal: true
    title: qsTr("Channel selection")

    ColumnLayout {
        anchors.fill: parent
        spacing: UI.standardMargin
        GridLayout {
            id: channelsLayout
            rows: 4
            flow: GridLayout.TopToBottom
            rowSpacing: 0
            Repeater {
                id: channelCheckBoxRepeater
                model: scannerBackend.channelSelectionModel
                delegate: AbracaCheckBox {
                    id: checkBox
                    required property int index
                    required property string channelName
                    required property bool isSelected
                    text: channelName
                    checked: isSelected
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: UI.standardMargin
            spacing: UI.standardMargin
            AbracaButton {
                text: qsTr("Select all")
                // check all checkboxes when clicked
                onClicked: {
                    for (var i = 0; i < channelCheckBoxRepeater.count; ++i) {
                        var cb = channelCheckBoxRepeater.itemAt(i);
                        cb.checked = true;
                    }
                }
            }
            AbracaButton {
                text: qsTr("Unselect all")
                // uncheck all checkboxes when clicked
                onClicked: {
                    for (var i = 0; i < channelCheckBoxRepeater.count; ++i) {
                        var cb = channelCheckBoxRepeater.itemAt(i);
                        cb.checked = false;
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Loader {
                active: true
                sourceComponent: UI.isMacOS ? dismissButtonComponent : okButtonComponent
            }
            Loader {
                active: true
                sourceComponent: UI.isMacOS ? okButtonComponent : dismissButtonComponent
            }

            Component {
                id: okButtonComponent
                AbracaButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("OK")
                    buttonRole: UI.ButtonRole.Primary
                    onClicked: {
                        for (var i = 0; i < channelCheckBoxRepeater.count; ++i) {
                            var cb = channelCheckBoxRepeater.itemAt(i);
                            scannerBackend.channelSelectionModel.setChecked(i, cb.checked);
                        }
                        dialog.accept();
                    }
                }
            }
            Component {
                id: dismissButtonComponent
                AbracaButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Cancel")
                    onClicked: dialog.reject()
                }
            }
        }
    }
}
