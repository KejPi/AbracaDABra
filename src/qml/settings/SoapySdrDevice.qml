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
import QtQuick.Controls
import QtQuick.Layouts
import abracaComponents 1.0

Item {
    implicitHeight: contentLayout.implicitHeight + 2*UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin
    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2*UI.standardMargin

        GridLayout {
            id: deviceSelectionGrid
            Layout.fillWidth: true
            rowSpacing: UI.standardMargin
            columnSpacing: UI.standardMargin
            columns: 5

            readonly property bool mobileView: mainItem.width <= UI.narrowViewWidth

            AbracaLabel {
                text: qsTr("Device arguments:")
            }
            AbracaTextField {
                Layout.fillWidth: true
                Layout.columnSpan: 4
                placeholderText: qsTr("Soapy SDR device arguments")
                text: settingsBackend.soapySdrDevArgs
                onEditingFinished: if (settingsBackend.soapySdrDevArgs !== text) {
                    settingsBackend.soapySdrDevArgs = text;
                }
            }

            AbracaLabel {
                text: qsTr("RX channel number:")
            }
            AbracaSpinBox {
                from: 0
                to: 9
                value: settingsBackend.soapySdrChannelNum
                onValueChanged: if (settingsBackend.soapySdrChannelNum !== value) {
                    settingsBackend.soapySdrChannelNum = value;
                }
            }
            Item {
                Layout.fillWidth: true
            }
            AbracaLabel {
                Layout.row: deviceSelectionGrid.mobileView ? 2 : 1
                Layout.column: deviceSelectionGrid.mobileView ? 0 : 3
                text: qsTr("Antenna:")
            }
            AbracaTextField {
                text: settingsBackend.soapySdrAntenna
                onEditingFinished: if (settingsBackend.soapySdrAntenna !== text) {
                    settingsBackend.soapySdrAntenna = text;
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }
        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.soapySdrDeviceDesc.length > 0
            Layout.alignment: Qt.AlignHCenter
            columnSpacing: 50
            rows: 1
            flow: GridLayout.TopToBottom
            AbracaLabel {
                text: qsTr("Connected device:")
            }
            Repeater {
                model: settingsBackend.soapySdrDeviceDesc
                AbracaLabel {
                    required property string modelData
                    text: modelData
                }
            }
        }
        AbracaLine {
            Layout.fillWidth: true
            isVertical: false
            Layout.topMargin: UI.standardMargin
            Layout.bottomMargin: 2*UI.standardMargin
        }
        AbracaGroupBox {
            id: gainGroupBox
            title: qsTr("Gain control")
            Layout.fillWidth: true
            enabled: settingsBackend.isSoapySdrControlEnabled
            GridLayout {
                width: gainGroupBox.width
                columns: appUI.isPortraitView ? 2 : 4
                Repeater {
                    id: gainModeRepeater
                    model: [qsTr("Device"), qsTr("Manual")]
                    delegate: AbracaRadioButton {
                        id: button
                        required property int index
                        required property string modelData
                        text: modelData
                        checked: settingsBackend.soapySdrGainMode === index
                        onToggled: {
                            if (checked) {
                                settingsBackend.requestSoapySdrGainMode(index);
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: gainSettingsLayout.implicitHeight
            GridLayout {
                id: gainSettingsLayout
                enabled: settingsBackend.isSoapySdrGainEnabled
                visible: settingsBackend.soapySdrGainModel.rowCount > 0
                anchors.left: parent.left
                anchors.right: parent.right
                rows: settingsBackend.soapySdrGainModel.rowCount
                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 30
                flow: GridLayout.TopToBottom
                Repeater {
                    model: settingsBackend.soapySdrGainModel
                    AbracaLabel {
                        required property string name
                        text: name
                    }
                }
                Repeater {
                    model: settingsBackend.soapySdrGainModel
                    AbracaSlider {
                        required property int gainValue
                        required property int minimum
                        required property int maximum
                        required property int step
                        required property int index
                        Layout.fillWidth: true

                        from: minimum
                        to: maximum
                        value: gainValue
                        stepSize: step
                        onMoved: {
                            if (gainValue !== value) {
                                settingsBackend.soapySdrGainModel.setGainValue(index, value);
                            }
                        }
                    }
                }
                Repeater {
                    model: settingsBackend.soapySdrGainModel
                    AbracaLabel {
                        id: gainValueLabel
                        required property int gainValue
                        text: gainValue + " dB"
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: fontMetrics.boundingRect("-155.5 dB").width
                        FontMetrics {
                            id: fontMetrics
                            font: gainValueLabel.font
                        }
                    }
                }
            }
        }
        AbracaLine {
            Layout.fillWidth: true
            isVertical: false
            Layout.topMargin: UI.standardMargin
            Layout.bottomMargin: 2*UI.standardMargin
        }
        AbracaGroupBox {
            id: expertGroupBox
            title: qsTr("Expert settings")
            enabled: settingsBackend.isSoapySdrControlEnabled
            Layout.fillWidth: true
            GridLayout {
                id: expertSettingsLayout
                // anchors.fill: parent
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 3
                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 30

                readonly property bool showIcon: mainItem.width < UI.narrowViewWidth

                AbracaLabel {
                    text: qsTr("Bandwidth:")
                }
                AbracaSpinBox {
                    id: bandwidthSpinBox
                    Layout.fillWidth: true
                    from: 0
                    to: 3000
                    stepSize: 100
                    value: settingsBackend.soapySdrBandWidth
                    onValueChanged: if (settingsBackend.soapySdrBandWidth !== value) {
                        settingsBackend.soapySdrBandWidth = value;
                    }
                    editable: true
                    suffix: " kHz"
                    specialValue: 0
                    specialValueString: qsTr("Default")
                }
                AbracaButton {
                    Layout.fillWidth: true
                    Layout.preferredWidth: expertSettingsLayout.showIcon ? 2*UI.iconSize : implicitWidth
                    // Layout.minimumWidth: implicitWidth
                    text: qsTr("Set default")
                    icon {
                        source: UI.imagesUrl + "reload.svg"
                        width: UI.iconSize
                        height: UI.iconSize
                        color: enabled ? UI.colors.icon : UI.colors.iconDisabled
                    }
                    display: expertSettingsLayout.showIcon ? AbstractButton.IconOnly : AbstractButton.TextOnly
                    enabled: settingsBackend.soapySdrBandWidth > 0
                    onClicked: settingsBackend.soapySdrBandWidth = 0
                }
                AbracaLabel {
                    text: qsTr("Frequency correction:")
                }
                AbracaSpinBox {
                    id: freqCorrSpinBox
                    Layout.fillWidth: true
                    from: -100
                    to: 100
                    stepSize: 1
                    value: settingsBackend.soapySdrFreqCorrection
                    onValueChanged: if (settingsBackend.soapySdrFreqCorrection !== value) {
                        settingsBackend.soapySdrFreqCorrection = value;
                    }
                    editable: true
                    suffix: " PPM"
                }
                AbracaButton {
                    Layout.fillWidth: true
                    Layout.preferredWidth: expertSettingsLayout.showIcon ? 2*UI.iconSize : implicitWidth
                    // Layout.minimumWidth: implicitWidth
                    text: qsTr("Reset")
                    icon {
                        source: UI.imagesUrl + "reload.svg"
                        width: UI.iconSize
                        height: UI.iconSize
                        color: enabled ? UI.colors.icon : UI.colors.iconDisabled
                    }
                    display: expertSettingsLayout.showIcon ? AbstractButton.IconOnly : AbstractButton.TextOnly
                    enabled: settingsBackend.soapySdrFreqCorrection !== 0
                    onClicked: settingsBackend.soapySdrFreqCorrection = 0
                }
            }
        }
    }
}
