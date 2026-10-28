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
    property Item entryItem: null
    property Item exitItem: null
    readonly property Item firstItem: airspyCombo
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
                KeyNavigation.tab: kn1
                KeyNavigation.backtab: mainItem.entryItem
                accessibleName: qsTr("Airspy device")
                id: airspyCombo
                Layout.preferredWidth: 100
                Layout.fillWidth: true
                model: settingsBackend.airspyDevicesModel
                enabled: settingsBackend.isAirspyControlEnabled || settingsBackend.isConnectButtonEnabled
                textRole: "itemName"
                currentIndex: settingsBackend.airspyDevicesModel.currentIndex
                onActivated: {
                    if (settingsBackend.airspyDevicesModel.currentIndex !== currentIndex) {
                        settingsBackend.airspyDevicesModel.currentIndex = currentIndex
                    }
                }
                elideMode: Text.ElideMiddle
            }
            AbracaButton {
                id: kn1
                KeyNavigation.tab: airspyFallbackSwitch
                KeyNavigation.backtab: airspyCombo
                text: qsTr("Reload")
                onClicked: settingsBackend.airspyReloadDeviceList()
            }
        }
        Item {  // required for correct text wrapping in the switch below
            Layout.fillWidth: true
            implicitHeight: airspyFallbackSwitch.implicitHeight
            AbracaSwitch {
                KeyNavigation.tab: gainModeRepeater.count > 0 ? gainModeRepeater.itemAt(0) : kn2
                KeyNavigation.backtab: kn1
                id: airspyFallbackSwitch
                anchors.fill: parent
                text: qsTr("Use any available Airspy device if the selected one fails")
                checked: settingsBackend.isAirspyFallbackChecked
                enabled: settingsBackend.isAirspyControlEnabled || settingsBackend.isConnectButtonEnabled
                onCheckedChanged: if (settingsBackend.isAirspyFallbackChecked !== checked) {
                    settingsBackend.isAirspyFallbackChecked = checked
                }
                wrapMode: Text.WordWrap
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: UI.standardMargin
        }
        GridLayout {
            id: deviceInfoLayout
            visible: settingsBackend.airspyDeviceDesc.length > 0
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
                model: settingsBackend.airspyDeviceDesc
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
            enabled: settingsBackend.isAirspyControlEnabled
            GridLayout {
                width: gainGroupBox.width
                columns: appUI.isPortraitView ? 2 : 4
                Repeater {
                    id: gainModeRepeater
                    model: [qsTr("Software"), qsTr("Hybrid"), qsTr("Sensitivity"), qsTr("Manual")]
                    delegate: AbracaRadioButton {
                        KeyNavigation.tab: index + 1 < gainModeRepeater.count ? gainModeRepeater.itemAt(index + 1) : (kn2)
                        KeyNavigation.backtab: index > 0 ? gainModeRepeater.itemAt(index - 1) : (airspyFallbackSwitch)
                        id: button
                        required property int index
                        required property string modelData
                        text: modelData
                        checked: settingsBackend.airspyGainMode === index
                        onToggled: {
                            if (checked) {
                                settingsBackend.requestAirspyGainMode(index)
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
                enabled: settingsBackend.isAirspyControlEnabled
                AbracaLabel {
                    text: qsTr("Sensitivity gain")
                    enabled: settingsBackend.airspyGainMode === 0 || settingsBackend.isAirspySensitivityGainEnabled
                }
                AbracaSlider {
                    id: kn2
                    KeyNavigation.tab: kn3
                    KeyNavigation.backtab: gainModeRepeater.count > 0 ? gainModeRepeater.itemAt(gainModeRepeater.count - 1) : airspyFallbackSwitch
                    accessibleName: qsTr("Sensitivity gain")
                    Layout.fillWidth: true
                    from: 0
                    to: 21
                    value: settingsBackend.airspySensitivityGainIndex
                    enabled: settingsBackend.isAirspySensitivityGainEnabled
                    onMoved: {
                        if (settingsBackend.airspySensitivityGainIndex !== value) {
                            settingsBackend.airspySensitivityGainIndex = value
                        }
                    }
                }
                AbracaLabel {
                    text: settingsBackend.airspySensitivityGainIndex
                    enabled: settingsBackend.airspyGainMode === 0 || settingsBackend.isAirspySensitivityGainEnabled
                }
                Item {}
                AbracaLabel {
                    text: qsTr("LNA gain")
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyLnaAgcChecked === false
                }
                AbracaSlider {
                    id: kn3
                    KeyNavigation.tab: lnaAgcSwitch
                    KeyNavigation.backtab: kn2
                    accessibleName: qsTr("LNA gain")
                    Layout.fillWidth: true
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyLnaAgcChecked === false
                    from: 0
                    to: 21
                    value: settingsBackend.airspyLnaGainIndex
                    onMoved: {
                        if (settingsBackend.airspyLnaGainIndex !== value) {
                            settingsBackend.airspyLnaGainIndex = value
                        }
                    }
                }
                AbracaLabel {
                    text: settingsBackend.airspyLnaGainIndex
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyLnaAgcChecked === false
                }
                AbracaSwitch {
                    KeyNavigation.tab: kn4
                    KeyNavigation.backtab: kn3
                    id: lnaAgcSwitch
                    text: qsTr("AGC")
                    enabled: settingsBackend.isAirspyManualGainEnabled
                    checked: settingsBackend.isAirspyLnaAgcChecked
                    onCheckedChanged: if (settingsBackend.isAirspyLnaAgcChecked !== checked) {
                        settingsBackend.isAirspyLnaAgcChecked = checked
                    }
                }

                AbracaLabel {
                    text: qsTr("Mixer gain")
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyMixerAgcChecked === false
                }
                AbracaSlider {
                    id: kn4
                    KeyNavigation.tab: kn5
                    KeyNavigation.backtab: lnaAgcSwitch
                    accessibleName: qsTr("Mixer gain")
                    Layout.fillWidth: true
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyMixerAgcChecked === false
                    from: 0
                    to: 21
                    value: settingsBackend.airspyMixerGainIndex
                    onMoved: {
                        if (settingsBackend.airspyMixerGainIndex !== value) {
                            settingsBackend.airspyMixerGainIndex = value
                        }
                    }
                }
                AbracaLabel {
                    text: settingsBackend.airspyMixerGainIndex
                    enabled: settingsBackend.isAirspyManualGainEnabled && settingsBackend.isAirspyMixerAgcChecked === false
                }
                AbracaSwitch {
                    id: kn5
                    KeyNavigation.tab: kn6
                    KeyNavigation.backtab: kn4
                    text: qsTr("AGC")
                    enabled: settingsBackend.isAirspyManualGainEnabled
                    checked: settingsBackend.isAirspyMixerAgcChecked
                    onCheckedChanged: if (settingsBackend.isAirspyMixerAgcChecked !== checked) {
                        settingsBackend.isAirspyMixerAgcChecked = checked
                    }
                }

                AbracaLabel {
                    text: qsTr("IF gain")
                    enabled: settingsBackend.airspyGainMode === 1 || settingsBackend.isAirspyManualGainEnabled
                }
                AbracaSlider {
                    id: kn6
                    KeyNavigation.tab: kn7
                    KeyNavigation.backtab: kn5
                    accessibleName: qsTr("IF gain")
                    Layout.fillWidth: true
                    from: 0
                    to: 21
                    value: settingsBackend.airspyIfGainIndex
                    enabled: settingsBackend.isAirspyManualGainEnabled
                    onMoved: {
                        if (settingsBackend.airspyIfGainIndex !== value) {
                            settingsBackend.airspyIfGainIndex = value
                        }
                    }
                }
                AbracaLabel {
                    text: settingsBackend.airspyIfGainIndex
                    enabled: settingsBackend.airspyGainMode === 1 || settingsBackend.isAirspyManualGainEnabled
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
            Layout.fillWidth: true
            GridLayout {
                anchors.fill: parent
                // anchors.top: parent.top
                // anchors.horizontalCenter: parent.horizontalCenter
                columns: 3
                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 30

                AbracaLabel {
                    text: qsTr("Bias Tee:")
                }
                AbracaSwitch {
                    id: kn7
                    KeyNavigation.tab: kn8
                    KeyNavigation.backtab: kn6
                    accessibleName: qsTr("Bias Tee")
                    Layout.alignment: Qt.AlignHCenter
                    text: ""
                    enabled: settingsBackend.isAirspyControlEnabled
                    checked: settingsBackend.airspyBiasT
                    onCheckedChanged: if (settingsBackend.airspyBiasT !== checked) {
                        settingsBackend.airspyBiasT = checked
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                AbracaLabel {
                    text: qsTr("Prefer 4096kHz rate:")
                }
                AbracaSwitch {
                    id: kn8
                    KeyNavigation.tab: mainItem.exitItem
                    KeyNavigation.backtab: kn7
                    accessibleName: qsTr("Prefer 4096kHz rate")
                    //Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    text: ""
                    enabled: settingsBackend.isAirspyControlEnabled === false
                    checked: settingsBackend.airspyPrefer4096kHz
                    onCheckedChanged: if (settingsBackend.airspyPrefer4096kHz !== checked) {
                        settingsBackend.airspyPrefer4096kHz = checked
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
}
