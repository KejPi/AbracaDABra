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
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

import abracaComponents

Item {
    id: mainItem
    implicitHeight: contentLayout.implicitHeight + 2 * UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2 * UI.standardMargin
    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2 * UI.standardMargin
        RowLayout {
            Layout.fillWidth: true
            AbracaButton {
                Layout.fillWidth: true
                text: qsTr("Open file")
                onClicked: {
                    fileDialogLoader.filepath = settingsBackend.rawFilePath();
                    fileDialogLoader.active = true;
                }
            }
            AbracaComboBox {
                Layout.fillWidth: true
                // Layout.preferredWidth: 1.5*implicitWidth
                model: settingsBackend.rawFileFormatModel
                textRole: "itemName"
                enabled: settingsBackend.isRawFileFormatSelectionEnabled
                currentIndex: settingsBackend.rawFileFormatModel.currentIndex
                onActivated: {
                    // if (currentIndex >= 0) {
                    //     var formatId = model.data(model.index(currentIndex, 0), ItemModel.DataRole)
                    //     if (formatId !== settingsBackend.rawFileFormat) {
                    //         settingsBackend.rawFileFormat = formatId
                    //     }
                    // }
                    if (settingsBackend.rawFileFormatModel.currentIndex !== currentIndex) {
                        settingsBackend.rawFileFormatModel.currentIndex = currentIndex;
                    }
                }
            }
            AbracaSwitch {
                Layout.fillWidth: true
                text: qsTr("Loop file")
                checked: settingsBackend.isRawFileLoopActive
                onCheckedChanged: {
                    if (checked !== settingsBackend.isRawFileLoopActive) {
                        settingsBackend.isRawFileLoopActive = checked;
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: filePathLabel.implicitHeight
            AbracaLabel {
                id: filePathLabel
                anchors.fill: parent
                text: settingsBackend.rawFileName.length > 0 ? settingsBackend.getDisplayPath(settingsBackend.rawFileName) : qsTr("No file selected")
                elide: Text.ElideLeft
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: settingsBackend.isRawFileProgressVisible
            AbracaSlider {
                Layout.fillWidth: true
                stepSize: 1
                live: false
                from: 0
                to: settingsBackend.rawFileProgressMax
                value: pressed ? value : settingsBackend.rawFileProgressValue
                onPressedChanged: {
                    if (!pressed) {
                        // User released the slider - send the new position to backend
                        settingsBackend.rawFileProgressValue = value;
                        settingsBackend.requestRawFileSeek(value);
                    }
                }
            }
            AbracaLabel {
                text: settingsBackend.rawFileProgressLabel
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }
        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.rawFileXmlHeader.length > 0
            Layout.alignment: Qt.AlignHCenter
            rows: 8
            flow: GridLayout.TopToBottom
            AbracaLabel {
                text: qsTr("Recording date:")
            }
            AbracaLabel {
                text: qsTr("Recorder:")
            }
            AbracaLabel {
                text: qsTr("Device:")
            }
            AbracaLabel {
                text: qsTr("Model:")
            }
            AbracaLabel {
                text: qsTr("Sample rate [Hz]:")
            }
            AbracaLabel {
                text: qsTr("Frequency [kHz]:")
            }
            AbracaLabel {
                text: qsTr("Recording length [sec]:")
            }
            AbracaLabel {
                text: qsTr("Sample format:")
            }
            Repeater {
                model: settingsBackend.rawFileXmlHeader
                AbracaLabel {
                    required property string modelData
                    text: modelData
                }
            }
        }
    }

    Loader {
        id: fileDialogLoader
        property string filepath: ""
        property string filename: ""
        asynchronous: true
        active: false
        onLoaded: item.open()
        sourceComponent: FileDialog {
            id: fileDialog
            fileMode: FileDialog.OpenFile
            options: FileDialog.DontResolveSymlinks
            nameFilters: [qsTr("Binary files") + " (*.bin *.s16 *.u8 *.raw *.sdr *.uff *.wav *.iq)"]
            // currentFolder doesn't work well on Android with content:// URIs
            currentFolder: UI.isAndroid ? "" : fileDialogLoader.filepath
            onAccepted: {
                settingsBackend.selectRawFile(selectedFile);
                fileDialogLoader.active = false;
            }
            onRejected: {
                fileDialogLoader.active = false;
            }
        }
    }    
}
