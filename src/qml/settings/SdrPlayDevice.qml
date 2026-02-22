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
    id: mainItem
    implicitHeight: contentLayout.implicitHeight + 2*UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin

    readonly property bool isDeviceInfoLoaded: settingsBackend.sdrplayDevicesModel.rowCount > 0
                                               && settingsBackend.sdrplayChannelModel.rowCount > 0
                                               && settingsBackend.sdrplayAntennaModel.rowCount > 0

    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2*UI.standardMargin
        GridLayout {
            id: deviceSelectionGrid
            Layout.fillWidth: true
            columnSpacing: UI.standardMargin
            rowSpacing: UI.standardMargin
            enabled: settingsBackend.isSdrplayDeviceSelectionEnabled        
            columns: 5

            readonly property bool mobileView: mainItem.width <= UI.narrowViewWidth

            AbracaComboBox {
                Layout.columnSpan: 4
                Layout.fillWidth: true
                model: settingsBackend.sdrplayDevicesModel
                enabled: mainItem.isDeviceInfoLoaded && !settingsBackend.isSdrplayControlEnabled
                textRole: "itemName"
                currentIndex: settingsBackend.sdrplayDevicesModel.currentIndex
                onActivated: {
                    if (settingsBackend.sdrplayDevicesModel.currentIndex !== currentIndex) {
                        settingsBackend.sdrplayDevicesModel.currentIndex = currentIndex
                    }
                }
            }
            AbracaButton {
                Layout.preferredWidth: antennaCombo.width
                enabled: settingsBackend.isSdrplayControlEnabled === false
                text: qsTr("Reload")
                onClicked: settingsBackend.sdrplayReloadRequest()
            }

            AbracaLabel {
                text: qsTr("RX channel number:")
                enabled: mainItem.isDeviceInfoLoaded && !settingsBackend.isSdrplayControlEnabled
            }
            AbracaComboBox {
                enabled: mainItem.isDeviceInfoLoaded && !settingsBackend.isSdrplayControlEnabled
                // Layout.columnSpan: deviceSelectionGrid.mobileView ? 3 : 1
                model: settingsBackend.sdrplayChannelModel
                textRole: "itemName"
                currentIndex: settingsBackend.sdrplayChannelModel.currentIndex
                onActivated: {
                    if (settingsBackend.sdrplayChannelModel.currentIndex !== currentIndex) {
                        settingsBackend.sdrplayChannelModel.currentIndex = currentIndex
                    }
                }

            }
            Item { Layout.fillWidth: true }            
            AbracaLabel {
                Layout.row: deviceSelectionGrid.mobileView ? 2 : 1
                Layout.column: deviceSelectionGrid.mobileView ? 0 : 3
                text: qsTr("Antenna:")
                enabled: mainItem.isDeviceInfoLoaded
            }
            AbracaComboBox {
                id: antennaCombo
                // Layout.columnSpan: deviceSelectionGrid.mobileView ? 3 : 1
                enabled: mainItem.isDeviceInfoLoaded
                model: settingsBackend.sdrplayAntennaModel                
                textRole: "itemName"
                currentIndex: settingsBackend.sdrplayAntennaModel.currentIndex
                onActivated: {
                    if (settingsBackend.sdrplayAntennaModel.currentIndex !== currentIndex) {
                        settingsBackend.sdrplayAntennaModel.currentIndex = currentIndex
                    }
                }
            }
        }

        Item {  // required for correct text wrapping in the switch below
            Layout.fillWidth: true
            implicitHeight: fallbackSwitch.implicitHeight
            AbracaSwitch {
                id: fallbackSwitch
                anchors.fill: parent
                text: qsTr("Use any available SDRplay device if the selected one fails")
                enabled: settingsBackend.isSdrplayDeviceSelectionEnabled
                checked: settingsBackend.isSdrplayFallbackChecked
                wrapMode: Text.WordWrap
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }

        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.sdrplayDeviceDesc.length > 0
            Layout.alignment: Qt.AlignHCenter
            columnSpacing: 50
            rows: 2
            flow: GridLayout.TopToBottom
            AbracaLabel {
                text: qsTr("Connected device:")
            }
            AbracaLabel {
                text: qsTr("Serial number:")
            }
            Repeater {
                model: settingsBackend.sdrplayDeviceDesc
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
            enabled: settingsBackend.isSdrplayControlEnabled
            GridLayout {
                width: gainGroupBox.width
                columns: appUI.isPortraitView ? 2 : 4
                Repeater {
                    id: gainModeRepeater
                    model: [qsTr("Software"), qsTr("Manual")]
                    delegate: AbracaRadioButton {
                        id: button
                        required property int index
                        required property string modelData
                        text: modelData
                        checked: settingsBackend.sdrplayGainMode === index
                        onToggled: {
                            if (checked) {
                                settingsBackend.requestSdrplayGainMode(index)
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
                anchors.left: parent.left
                anchors.right: parent.right
                columns: 4
                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 30
                enabled: settingsBackend.isSdrplayControlEnabled
                AbracaLabel {
                    text: qsTr("RF gain")
                }
                AbracaSlider {
                    id: rfGainSlider
                    Layout.fillWidth: true
                    from: 0
                    to: settingsBackend.sdrplayRfGainIndexMax
                    stepSize: 1
                    value: settingsBackend.sdrplayRfGainIndex
                    enabled: settingsBackend.sdrplayGainMode === 1
                    // snapMode: Slider.SnapOnRelease
                    // snapMode: Slider.SnapAlways
                    onValueChanged: {
                        if (settingsBackend.sdrplayRfGainIndex !==value) {
                            settingsBackend.sdrplayRfGainIndex = value
                        }
                    }
                }
                AbracaLabel {
                    id: gainValueLabel
                    text: settingsBackend.sdrplayRfGainLabel
                    Layout.preferredWidth: fontMetrics.boundingRect("155.5 dB").width
                    horizontalAlignment: Text.AlignRight
                    FontMetrics {
                        id: fontMetrics
                        font: gainValueLabel.font
                    }
                }
                Item {}

                AbracaLabel {
                    text: qsTr("IF gain")
                    enabled: settingsBackend.sdrplayGainMode === 0 || settingsBackend.isSdrplayIfAgcChecked === false
                }
                AbracaSlider {
                    Layout.fillWidth: true
                    enabled: settingsBackend.sdrplayGainMode === 1 && settingsBackend.isSdrplayIfAgcChecked === false
                    from: -59
                    to: -20
                    stepSize: 1
                    value: settingsBackend.sdrplayIfGain
                    onValueChanged: {
                        if (settingsBackend.sdrplayIfGain !== value) {
                            settingsBackend.sdrplayIfGain = value
                        }
                    }
                }
                AbracaLabel {
                    Layout.alignment: Qt.AlignRight
                    text: settingsBackend.sdrplayIfGain !== 0 ? settingsBackend.sdrplayIfGain + " dB" : qsTr("N/A")
                    enabled: settingsBackend.sdrplayGainMode === 0 || settingsBackend.isSdrplayIfAgcChecked === false
                }
                AbracaSwitch {
                    text: qsTr("AGC")
                    enabled: settingsBackend.sdrplayGainMode === 1
                    checked: settingsBackend.isSdrplayIfAgcChecked
                    onCheckedChanged: if (settingsBackend.isSdrplayIfAgcChecked !== checked) {
                        settingsBackend.isSdrplayIfAgcChecked = checked
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
            enabled: settingsBackend.isSdrplayControlEnabled
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
                    text: qsTr("Frequency correction:")
                }
                AbracaSpinBox {
                    id: freqCorrSpinBox
                    Layout.fillWidth: true
                    from: -100
                    to: 100
                    stepSize: 1
                    value: settingsBackend.sdrplayFreqCorrection
                    onValueChanged: if (settingsBackend.sdrplayFreqCorrection !== value) {
                        settingsBackend.sdrplayFreqCorrection = value
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
                    enabled: settingsBackend.sdrplayFreqCorrection !== 0
                    onClicked: settingsBackend.sdrplayFreqCorrection = 0
                }
                AbracaLabel {
                    text: qsTr("Bias Tee:")
                }
                AbracaSwitch {
                    Layout.alignment: Qt.AlignHCenter
                    text: ""
                    checked: settingsBackend.sdrplayBiasT
                    onCheckedChanged: if (settingsBackend.sdrplayBiasT !== checked) {
                        settingsBackend.sdrplayBiasT = checked
                    }
                }
            }
        }
    }
}
