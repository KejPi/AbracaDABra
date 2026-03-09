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
import abracaComponents

Item {
    id: mainItem
    implicitHeight: contentLayout.implicitHeight + 2*UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin
    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2*UI.standardMargin
        RowLayout {
            Layout.fillWidth: true
            spacing: UI.standardMargin
            AbracaComboBox {
                id: rtlSdrCombo
                Layout.preferredWidth: 100
                Layout.fillWidth: true
                model: settingsBackend.rtlSdrDevicesModel
                enabled: settingsBackend.isRtlSdrControlEnabled || settingsBackend.isConnectButtonEnabled
                textRole: "itemName"
                currentIndex: settingsBackend.rtlSdrDevicesModel.currentIndex
                onActivated: {
                    if (settingsBackend.rtlSdrDevicesModel.currentIndex !== currentIndex) {
                        settingsBackend.rtlSdrDevicesModel.currentIndex = currentIndex
                    }
                }
                elideMode: Text.ElideMiddle
            }
            AbracaButton {
                text: qsTr("Reload")
                onClicked: settingsBackend.rtlSdrReloadDeviceList()
            }
        }
        Item {  // required for correct text wrapping in the switch below
            Layout.fillWidth: true
            implicitHeight: rtlSdrFallbackSwitch.implicitHeight
            AbracaSwitch {
                id: rtlSdrFallbackSwitch
                anchors.fill: parent
                text: qsTr("Use any available RTL-SDR device if the selected one fails")
                checked: settingsBackend.isRtlSdrFallbackChecked
                enabled: settingsBackend.isRtlSdrControlEnabled || settingsBackend.isConnectButtonEnabled
                onCheckedChanged: if (settingsBackend.isRtlSdrFallbackChecked !== checked) {
                    settingsBackend.isRtlSdrFallbackChecked = checked
                }
                wrapMode: Text.WordWrap
                // Rectangle {
                //     anchors.fill: parent
                //     color: "transparent"
                //     border.color: "red"
                //     border.width: 1
                // }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }
        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.rtlSdrDeviceDesc.length > 0
            Layout.alignment: Qt.AlignHCenter
            columnSpacing: 50
            rows: 4
            flow: GridLayout.TopToBottom
            AbracaLabel {
                text: qsTr("Connected device:")
            }
            AbracaLabel {
                text: qsTr("Serial number:")
            }
            AbracaLabel {
                text: qsTr("Tuner:")
            }
            AbracaLabel {
                text: qsTr("Sample format:")
            }
            Repeater {
                model: settingsBackend.rtlSdrDeviceDesc
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
            enabled: settingsBackend.isRtlSdrControlEnabled
            GridLayout {
                width: gainGroupBox.width
                columns: appUI.isPortraitView ? 2 : 4
                Repeater {
                    id: gainModeRepeater
                    model: [qsTr("Software"), qsTr("Driver"), qsTr("Device"), qsTr("Manual")]
                    delegate: AbracaRadioButton {
                        id: button
                        required property int index
                        required property string modelData
                        text: modelData
                        visible: index !== 1 || settingsBackend.haveRtlSdrOldDabDriver
                        checked: settingsBackend.rtlSdrGainMode === index
                        onToggled: {
                            if (checked) {
                                settingsBackend.requestRtlSdrGainMode(index)
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: appUI.isPortraitView ? UI.standardMargin : 30
            AbracaLabel {
                id: gainLabel
                Layout.preferredWidth: gainValueLabel.width
                text: qsTr("Gain")
            }
            AbracaSlider {
                Layout.fillWidth: true
                from: 0
                to: settingsBackend.rtlSdrGainIndexMax
                value: settingsBackend.rtlSdrGainIndex
                enabled: settingsBackend.isRtlSdrControlEnabled && settingsBackend.isRtlSdrGainEnabled
                onMoved: {
                    if (settingsBackend.rtlSdrGainIndex !== value) {
                        settingsBackend.rtlSdrGainIndex = value
                    }
                }
            }
            AbracaLabel {
                id: gainValueLabel
                text: settingsBackend.rtlSdrGainLabel
                horizontalAlignment: Text.AlignRight
                Layout.preferredWidth: fontMetrics.boundingRect("155.5 dB").width
                FontMetrics {
                    id: fontMetrics
                    font: gainValueLabel.font
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
            enabled: settingsBackend.isRtlSdrControlEnabled
            Layout.fillWidth: true
            Item {
                width: expertGroupBox.width
                implicitHeight: expertSettingsLayout.implicitHeight
                GridLayout {
                    id: expertSettingsLayout
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(expertGroupBox.width, implicitWidth)
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
                        value: settingsBackend.rtlSdrBandWidth
                        onValueChanged: if (settingsBackend.rtlSdrBandWidth !== value) {
                            settingsBackend.rtlSdrBandWidth = value
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
                        enabled: settingsBackend.rtlSdrBandWidth > 0
                        onClicked: settingsBackend.rtlSdrBandWidth = 0
                    }
                    AbracaLabel {
                        text: qsTr("SW AGC level threshold:")
                    }
                    AbracaSpinBox {
                        id: agcThrSpinBox
                        Layout.fillWidth: true
                        from: 0
                        to: 127
                        stepSize: 1
                        value: settingsBackend.rtlSdrAgcLevelThr
                        onValueChanged: if (settingsBackend.rtlSdrAgcLevelThr !== value) {
                            settingsBackend.rtlSdrAgcLevelThr = value
                        }
                        editable: true
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
                        enabled: settingsBackend.rtlSdrAgcLevelThr > 0
                        onClicked: settingsBackend.rtlSdrAgcLevelThr = 0
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
                        value: settingsBackend.rtlSdrFreqCorrection
                        onValueChanged: if (settingsBackend.rtlSdrFreqCorrection !== value) {
                            settingsBackend.rtlSdrFreqCorrection = value
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
                        enabled: settingsBackend.rtlSdrFreqCorrection !== 0
                        onClicked: settingsBackend.rtlSdrFreqCorrection = 0
                    }
                    AbracaLabel {
                        text: qsTr("RF level estimation:")
                        visible: settingsBackend.haveRtlSdrOldDabDriver
                    }
                    AbracaSwitch {
                        Layout.alignment: Qt.AlignHCenter
                        text: ""
                        checked: settingsBackend.isRtlSdrRfLevelEna
                        onCheckedChanged: if (settingsBackend.isRtlSdrRfLevelEna !== checked) {
                            settingsBackend.isRtlSdrRfLevelEna = checked
                        }
                        visible: settingsBackend.haveRtlSdrOldDabDriver
                    }
                    Item {visible: settingsBackend.haveRtlSdrOldDabDriver}
                    AbracaLabel {
                        text: qsTr("RF level correction:")
                        visible: settingsBackend.haveRtlSdrOldDabDriver
                    }
                    AbracaDoubleSpinBox {
                        id: rfLevelCorrSpinBox
                        Layout.fillWidth: true
                        stepSizeReal: 1.0
                        fromReal: -50.0
                        toReal: 50.0
                        realValue: settingsBackend.rtlSdrRfLevelCorrection
                        onValueChanged: {
                            var newReal = rfLevelCorrSpinBox.valueToReal()
                            if (settingsBackend.rtlSdrRfLevelCorrection !== newReal) {
                                settingsBackend.rtlSdrRfLevelCorrection = newReal;
                            }
                        }
                        editable: true
                        suffix: " dB"
                        enabled: settingsBackend.isRtlSdrRfLevelEna
                        visible: settingsBackend.haveRtlSdrOldDabDriver
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
                        onClicked: settingsBackend.rtlSdrRfLevelCorrection = 0.0
                        enabled: settingsBackend.isRtlSdrRfLevelEna && settingsBackend.rtlSdrRfLevelCorrection !== 0
                        visible: settingsBackend.haveRtlSdrOldDabDriver
                    }
                    AbracaLabel {
                        text: qsTr("Bias Tee:")
                    }
                    AbracaSwitch {
                        Layout.alignment: Qt.AlignHCenter
                        text: ""
                        checked: settingsBackend.rtlSdrBiasT
                        onCheckedChanged: if (settingsBackend.rtlSdrBiasT !== checked) {
                            settingsBackend.rtlSdrBiasT = checked
                        }
                    }
                }
            }
        }
    }
}
