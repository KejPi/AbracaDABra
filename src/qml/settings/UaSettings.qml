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

Item {
    id: mainItem

    Flickable {
        id: contentFlickable
        anchors.fill: parent

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: contentItem.implicitWidth
        contentHeight: contentItem.implicitHeight

        Item {
            id: contentContainer
            width: contentFlickable.width
            height: contentItem.implicitHeight
            Item {
                id: contentItem
                implicitHeight: contentLayout.implicitHeight + 2*UI.standardMargin
                // implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin
                implicitWidth: Math.min(contentLayout.implicitWidth, contentFlickable.width) + 2*UI.standardMargin
                //width: Math.min(700, Math.Max(contentFlickable.width, implicitWidth)
                width: Math.max(Math.min(contentFlickable.width, 700), implicitWidth)
                anchors.horizontalCenter: parent.horizontalCenter

                ColumnLayout {
                    id: contentLayout
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 2*UI.standardMargin
                    //width: Math.min(implicitWidth, mainItem.width - 2*UI.standardMargin)
                    AbracaGroupBox {
                        title: qsTr("SPI application")
                        Layout.fillWidth: true
                        GridLayout {
                            anchors.fill: parent
                            flow: GridLayout.TopToBottom
                            rows: appUI.isPortraitView ? 5 : 3
                            uniformCellWidths: true
                            AbracaSwitch {
                                id: enableSpiSwitch
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("Enable SPI application")
                                wrapMode: Text.WordWrap
                                checked: settingsBackend.isSpiAppEnabled
                                onCheckedChanged: {
                                    if (settingsBackend.isSpiAppEnabled !== checked) {
                                        settingsBackend.isSpiAppEnabled = checked
                                    }
                                }
                            }
                            AbracaSwitch {
                                id: downloadDataSwitch
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("Download data from internet")
                                wrapMode: Text.WordWrap
                                enabled: enableSpiSwitch.checked
                                checked: settingsBackend.isSpiInternetEnabled
                                onCheckedChanged: {
                                    if (settingsBackend.isSpiInternetEnabled !== checked) {
                                        settingsBackend.isSpiInternetEnabled = checked
                                    }
                                }
                            }
                            AbracaSwitch {
                                id: radiodnsSwitch
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("RadioDNS")
                                enabled: enableSpiSwitch.checked && downloadDataSwitch.checked
                                checked: settingsBackend.isRadioDnsEnabled
                                onCheckedChanged: {
                                    if (settingsBackend.isRadioDnsEnabled !== checked) {
                                        settingsBackend.isRadioDnsEnabled = checked
                                    }
                                }
                            }
                            AbracaSwitch {
                                id: progressSwitch
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("Show decoding progress")
                                wrapMode: Text.WordWrap
                                enabled: enableSpiSwitch.checked
                                checked: settingsBackend.isSpiProgressEnabled
                                onCheckedChanged: {
                                    if (settingsBackend.isSpiProgressEnabled !== checked) {
                                        settingsBackend.isSpiProgressEnabled = checked
                                    }
                                }
                            }
                            AbracaSwitch {
                                Layout.fillWidth: true
                                Layout.minimumWidth: implicitWidth
                                text: qsTr("Hide progress when completed")
                                wrapMode: Text.WordWrap
                                enabled: enableSpiSwitch.checked && progressSwitch.checked
                                checked: settingsBackend.spiProgressHideComplete
                                onCheckedChanged: {
                                    if (settingsBackend.spiProgressHideComplete !== checked) {
                                        settingsBackend.spiProgressHideComplete = checked
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
                        id: dataStorageGroup
                        title: qsTr("Data storage")
                        Layout.fillWidth: true
                        Item {
                            width: dataStorageGroup.width
                            implicitHeight: dataStorageLayout.implicitHeight
                            ColumnLayout {
                                id: dataStorageLayout
                                anchors.fill: parent
                                spacing: UI.standardMargin
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: UI.standardMargin
                                    AbracaLabel {
                                        text: qsTr("Folder:")
                                        role: UI.LabelRole.Secondary
                                    }
                                    AbracaLabel {
                                        Layout.fillWidth: true
                                        text: settingsBackend.dataDumpPath
                                        elide: Text.ElideLeft
                                    }
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: implicitWidth
                                    text: qsTr("Overwrite existing files")
                                    wrapMode: Text.WordWrap
                                    checked: settingsBackend.isUaDumpOverwiteEnabled
                                    onCheckedChanged: {
                                        if (settingsBackend.isUaDumpOverwiteEnabled !== checked) {
                                            settingsBackend.isUaDumpOverwiteEnabled = checked
                                        }
                                    }
                                }

                                AbracaLine {
                                    id: line
                                    Layout.fillWidth: true
                                    Layout.topMargin: UI.standardMargin
                                    Layout.bottomMargin: UI.standardMargin
                                    isVertical: false
                                }
                                AbracaSwitch {
                                    id: slsDataSwitch
                                    Layout.fillWidth: true
                                    // Layout.minimumWidth: implicitWidth
                                    text: qsTr("Slideshow data")
                                    wrapMode: Text.WordWrap
                                    checked: settingsBackend.isUaDumpSlsEnabled
                                    onCheckedChanged: {
                                        if (settingsBackend.isUaDumpSlsEnabled !== checked) {
                                            settingsBackend.isUaDumpSlsEnabled = checked
                                        }
                                    }
                                }
                                AbracaTextField {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: slsDataSwitch.labelX
                                    placeholderText: qsTr("SLS folder template")
                                    defaultText: settingsBackend.slsDumpPaternDefault
                                    text: settingsBackend.uaDumpSlsPattern
                                    onEditingFinished: if (settingsBackend.uaDumpSlsPattern !== text.trim()) {
                                        settingsBackend.uaDumpSlsPattern = text.trim()
                                    }
                                }
                                AbracaSwitch {
                                    id: spiDataSwitch
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: implicitWidth
                                    text: qsTr("SPI data")
                                    wrapMode: Text.WordWrap
                                    enabled: enableSpiSwitch.checked
                                    checked: settingsBackend.isUaDumpSpiEnabled
                                    onCheckedChanged: {
                                        if (settingsBackend.isUaDumpSpiEnabled !== checked) {
                                            settingsBackend.isUaDumpSpiEnabled = checked
                                        }
                                    }
                                }
                                AbracaTextField {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: spiDataSwitch.labelX
                                    placeholderText: qsTr("SPI folder template")
                                    text: settingsBackend.uaDumpSpiPattern
                                    defaultText: settingsBackend.spiDumpPaternDefault
                                    onEditingFinished: if (settingsBackend.uaDumpSpiPattern !== text.trim()) {
                                        settingsBackend.uaDumpSpiPattern = text.trim()
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: tokensLayout.implicitHeight
                                    GridLayout {
                                        id: tokensLayout
                                        anchors.fill: parent
                                        columns: 2
                                        readonly property string str1: qsTr("Data storage path pattern supports these tokens.")
                                        readonly property string str2: qsTr("For more information see")
                                        readonly property string str3: qsTr("Documentation")
                                        AbracaLabel {
                                            id: longLabel
                                            visible: appUI.isPortraitView === false
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            text: String("%1 %2 <a href=\"https://github.com/KejPi/AbracaDABra/blob/main/README.md#user-application-data-storage\">%3</a>")
                                            .arg(tokensLayout.str1).arg(tokensLayout.str2).arg(tokensLayout.str3)
                                            textFormat: Text.StyledText
                                            wrapMode: Text.WordWrap
                                            role: UI.LabelRole.Secondary
                                        }
                                        AbracaLabel {
                                            visible: appUI.isPortraitView === true
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            text: tokensLayout.str1
                                            textFormat: Text.StyledText
                                            wrapMode: Text.WordWrap
                                            role: UI.LabelRole.Secondary
                                        }
                                        AbracaLabel {
                                            visible: appUI.isPortraitView === true
                                            Layout.fillWidth: true
                                            Layout.columnSpan: 2
                                            text: String("%1 <a href=\"https://github.com/KejPi/AbracaDABra/blob/main/README.md#user-application-data-storage\">%2</a>")
                                            .arg(tokensLayout.str2)
                                            .arg(tokensLayout.str3)
                                            textFormat: Text.StyledText
                                            wrapMode: Text.WordWrap
                                            role: UI.LabelRole.Secondary
                                        }
                                        AbracaLabel {
                                            text: "{serviceId}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("current audio service ID (hex number)")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{ensId}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("current ensemble ID (hex number)")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{transportId}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("transport ID")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{contentName}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("content name")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{contentNameWithExt}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("content name with extension (only SLS)")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{directoryId}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("transport ID of directory (only SPI)")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                        AbracaLabel {
                                            text: "{scId}"
                                            Layout.alignment: Qt.AlignTop
                                        }
                                        AbracaLabel {
                                            text: qsTr("service component ID (only SPI)")
                                            role: UI.LabelRole.Secondary
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        ScrollIndicator.vertical: AbracaScrollIndicator { }
        ScrollIndicator.horizontal: AbracaScrollIndicator { }
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: contentItem.width
        topDownDirection: true
        shadowDistance: contentFlickable.contentY
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: contentItem.width
        topDownDirection: false
        shadowDistance: contentFlickable.contentHeight - contentFlickable.height - contentFlickable.contentY
    }
}
