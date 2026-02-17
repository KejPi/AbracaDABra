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
    implicitHeight: contentLayout.implicitHeight + 2 * UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin
    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2*UI.standardMargin
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: UI.standardMargin
            rowSpacing: UI.standardMargin
            columns: UI.isMobile ? 2 : 4
            AbracaLabel {
                text: qsTr("IP address:")
            }
            AbracaTextField {
                id: ipAddressField
                Layout.fillWidth: true
                defaultText: "127.0.0.1"
                placeholderText: qsTr("IP address of RTL-TCP server")
                readonly property string ipRange: "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])"
                readonly property regexp ipRegex: new RegExp("^" + ipRange + "(\\." + ipRange + "){3}$")
                validator: RegularExpressionValidator {
                    regularExpression: ipAddressField.ipRegex
                }
                text: settingsBackend.rtlTcpIpAddress
                onEditingFinished: if (settingsBackend.rtlTcpIpAddress !== text) {
                    settingsBackend.rtlTcpIpAddress = text;
                }
            }
            AbracaLabel {
                text: qsTr("Port:")
            }
            AbracaSpinBox {
                from: 0
                to: 65535
                locale: Qt.locale("C")
                value: settingsBackend.rtlTcpPort
                onValueChanged: if (settingsBackend.rtlTcpPort !== value) {
                    settingsBackend.rtlTcpPort = value;
                }
                editable: true
            }
        }
        AbracaSwitch {
            text: qsTr("Connect to control socket if available")
            wrapMode: Text.WordWrap
            checked: settingsBackend.isRtlTcpControlSocketChecked
            onCheckedChanged: {
                if (settingsBackend.isRtlTcpControlSocketChecked !== checked) {
                    settingsBackend.isRtlTcpControlSocketChecked = checked;
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }
        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.rtlTcpDeviceDesc.length > 0
            Layout.alignment: Qt.AlignHCenter
            columnSpacing: 50
            rows: 3
            flow: GridLayout.TopToBottom
            AbracaLabel {
                text: qsTr("Connected device:")
            }
            AbracaLabel {
                text: qsTr("Tuner:")
            }
            AbracaLabel {
                text: qsTr("Sample format:")
            }
            Repeater {
                model: settingsBackend.rtlTcpDeviceDesc
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
            enabled: settingsBackend.isRtlTcpControlEnabled
            GridLayout {
                width: gainGroupBox.width
                columns: appUI.isPortraitView ? 2 : 4
                Repeater {
                    id: gainModeRepeater
                    model: [qsTr("Software"), qsTr("Device"), qsTr("Manual")]
                    delegate: AbracaRadioButton {
                        id: button
                        required property int index
                        required property string modelData
                        text: modelData
                        checked: settingsBackend.rtlTcpGainMode === index
                        onToggled: {
                            if (checked) {
                                settingsBackend.requestRtlTcpGainMode(index);
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
                Layout.preferredWidth: gainValueLabel.width
                text: qsTr("Gain")
            }
            AbracaSlider {
                Layout.fillWidth: true
                from: 0
                to: settingsBackend.rtlTcpGainIndexMax
                value: settingsBackend.rtlTcpGainIndex
                enabled: settingsBackend.isRtlTcpControlEnabled && settingsBackend.isRtlTcpGainEnabled
                onMoved: {
                    if (settingsBackend.rtlTcpGainIndex !== value) {
                        settingsBackend.rtlTcpGainIndex = value;
                    }
                }
            }
            AbracaLabel {
                id: gainValueLabel
                text: settingsBackend.rtlTcpGainLabel
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
            enabled: settingsBackend.isRtlTcpControlEnabled
            Layout.fillWidth: true
            GridLayout {
                id: expertSettingsLayout
                // anchors.fill: parent:
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 3
                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 30
                readonly property bool showIcon: mainItem.width < UI.narrowViewWidth

                AbracaLabel {
                    text: qsTr("SW AGC level threshold:")
                }
                AbracaSpinBox {
                    id: agcThrSpinBox
                    Layout.fillWidth: true
                    from: 0
                    to: 127
                    stepSize: 1
                    value: settingsBackend.rtlTcpAgcLevelThr
                    onValueChanged: if (settingsBackend.rtlTcpAgcLevelThr !== value) {
                        settingsBackend.rtlTcpAgcLevelThr = value;
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
                    enabled: settingsBackend.rtlTcpAgcLevelThr > 0
                    onClicked: settingsBackend.rtlTcpAgcLevelThr = 0
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
                    value: settingsBackend.rtlTcpFreqCorrection
                    onValueChanged: if (settingsBackend.rtlTcpFreqCorrection !== value) {
                        settingsBackend.rtlTcpFreqCorrection = value;
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
                    enabled: settingsBackend.rtlTcpFreqCorrection !== 0
                    onClicked: settingsBackend.rtlTcpFreqCorrection = 0
                }
                AbracaLabel {
                    text: qsTr("RF level correction:")
                }
                AbracaDoubleSpinBox {
                    id: rfLevelCorrSpinBox
                    Layout.fillWidth: true
                    stepSizeReal: 1.0
                    fromReal: -50.0
                    toReal: 50.0
                    realValue: settingsBackend.rtlTcpRfLevelCorrection
                    onValueChanged: {
                        var newReal = rfLevelCorrSpinBox.valueToReal();
                        if (settingsBackend.rtlTcpRfLevelCorrection !== newReal) {
                            settingsBackend.rtlTcpRfLevelCorrection = newReal;
                        }
                    }
                    editable: true
                    suffix: " dB"
                }
                AbracaButton {
                    id: rfLevelCorrResetButton
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
                    enabled: settingsBackend.rtlTcpRfLevelCorrection !== 0.0
                    onClicked: settingsBackend.rtlTcpRfLevelCorrection = 0.0
                }
            }
        }
    }
}
