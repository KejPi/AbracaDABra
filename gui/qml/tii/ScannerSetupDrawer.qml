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

AbracaDrawer {
    id: drawer
    width: parent.width
    edge: Qt.BottomEdge
    height: Math.min(mainLayout.implicitHeight + 4*UI.standardMargin, parent.height * 0.9)

    onAboutToHide: {
        for (var i = 0; i < channelCheckBoxRepeater.count; ++i) {
            var cb = channelCheckBoxRepeater.itemAt(i);
            scannerBackend.channelSelectionModel.setChecked(i, cb.checked);
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: contentContainer.implicitWidth
        contentHeight: contentContainer.implicitHeight

        clip: true
        //boundsBehavior: Flickable.StopAtBounds
        Item {
            id: contentContainer
            implicitHeight: mainLayout.implicitHeight + 4*UI.standardMargin
            implicitWidth: flickable.width
            ColumnLayout {
                id: mainLayout
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 2*UI.standardMargin                

                Item {
                    Layout.alignment: Qt.AlignLeft
                    Layout.leftMargin: UI.standardMargin
                    implicitWidth: settingsLayout.implicitWidth
                    implicitHeight: settingsLayout.implicitHeight
                    GridLayout {
                        id: settingsLayout
                        columnSpacing: 2*UI.standardMargin
                        rowSpacing: UI.standardMargin
                        columns: 2
                        AbracaLabel {
                            text: qsTr("Mode:")
                        }

                        AbracaComboBox {
                            Layout.fillWidth: true
                            model: ListModel {
                                id: modeModel
                                ListElement {
                                    text: qsTr("Fast")
                                    mode: 1 // ScannerBackend.ModeFast // ListElement: cannot use script for property value
                                }
                                ListElement {
                                    text: qsTr("Normal")
                                    mode: 2 // ScannerBackend.ModeNormal // ListElement: cannot use script for property value
                                }
                                ListElement {
                                    text: qsTr("Precise")
                                    mode: 4 // ScannerBackend.ModePrecise // ListElement: cannot use script for property value
                                }
                            }
                            textRole: "text"
                            valueRole: "mode"
                            enabled: !scannerBackend.isScanning
                            currentIndex: {
                                switch (scannerBackend.mode) {
                                    case 1: return 0;
                                    case 2: return 1;
                                    case 4: return 2;
                                    default: return -1;
                                }
                            }
                            onActivated: {
                                scannerBackend.mode = valueAt(currentIndex);
                            }
                        }
                        AbracaLabel {
                            text: qsTr("Number of cycles:")
                        }
                        AbracaSpinBox {
                            id: cyclesSpinBox
                            Layout.fillWidth: true
                            value: scannerBackend.numCycles
                            onValueChanged: {
                                if (scannerBackend.numCycles !== value) {
                                    scannerBackend.numCycles = value
                                }
                            }
                            stepSize: 1
                            from: 0
                            to: 99
                            editable: true
                            enabled: !scannerBackend.isScanning
                            specialValue: 0
                            specialValueString: qsTr("Inf")
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
                    id: selectChannelsGroupBox
                    title: qsTr("Channel selection")
                    Layout.fillWidth: true
                    Item {
                        width: selectChannelsGroupBox.width
                        implicitHeight: channelsLayout.implicitHeight
                        GridLayout {
                            id: channelsLayout
                            anchors.fill: parent
                            columns: 4
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
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: UI.standardMargin
                    spacing: UI.standardMargin

                    AbracaButton {
                        Layout.fillWidth: true
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
                        Layout.fillWidth: true
                        text: qsTr("Unselect all")
                        // uncheck all checkboxes when clicked
                        onClicked: {
                            for (var i = 0; i < channelCheckBoxRepeater.count; ++i) {
                                var cb = channelCheckBoxRepeater.itemAt(i);
                                cb.checked = false;
                            }
                        }
                    }
                }
                AbracaLine {
                    Layout.fillWidth: true
                    isVertical: false
                    Layout.topMargin: UI.standardMargin
                    Layout.bottomMargin: UI.standardMargin
                }

                AbracaMenuItem {
                    text: qsTr("Clear scan results on start")
                    checkable: true
                    onTriggered: scannerBackend.clearOnStart = checked
                    Component.onCompleted: checked = scannerBackend.clearOnStart
                }
                AbracaMenuItem {
                    text: qsTr("Hide local (known) transmitters")
                    checkable: true
                    onTriggered: scannerBackend.hideLocalTx = checked
                    Component.onCompleted: checked = scannerBackend.hideLocalTx
                }
                AbracaMenuItem {
                    text: qsTr("AutoSave CSV")
                    checkable: true
                    onTriggered: scannerBackend.autoSave = checked
                    Component.onCompleted: checked = scannerBackend.autoSave
                }
                AbracaLine {
                    Layout.fillWidth: true
                    isVertical: false
                    Layout.topMargin: UI.standardMargin
                    Layout.bottomMargin: UI.standardMargin
                }
                AbracaMenuItem {
                    text: qsTr("Save as CSV")
                    onTriggered: scannerBackend.saveCSV()
                    enabled: scannerBackend.tableModel.rowCount > 0
                }
                AbracaMenuItem {
                    text: qsTr("Load from CSV")
                    enabled: !scannerBackend.isScanning
                    onTriggered: scannerBackend.importAction()
                }
                AbracaMenuItem {
                    text: qsTr("Clear scan results")
                    onTriggered: scannerBackend.clearTableAction()
                    enabled: scannerBackend.tableModel.rowCount > 0
                }
                AbracaMenuItem {
                    text: qsTr("Clear local (known) transmitter database")
                    onTriggered: scannerBackend.clearLocalTxAction()
                }
            }
            AbracaImgButton {
                id: closeButton
                anchors.right: parent.right
                anchors.rightMargin: UI.standardMargin
                anchors.top: parent.top
                anchors.topMargin: UI.standardMargin
                source: UI.imagesUrl + "close.svg"
                colorizationColor: UI.colors.iconInactive
                onClicked: {
                    close()
                }
            }
        }
        ScrollIndicator.vertical: AbracaScrollIndicator { }
    }
}
