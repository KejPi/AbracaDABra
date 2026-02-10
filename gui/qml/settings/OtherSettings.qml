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
                implicitHeight: contentLayout.implicitHeight + 4*UI.standardMargin
                implicitWidth: Math.min(contentLayout.implicitWidth, contentFlickable.width) + 2*UI.standardMargin
                // implicitWidth: uiGroupBox.implicitWidth + 2*UI.standardMargin
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
                        id: uiThemeGroupBox
                        title: qsTr("Visual style")
                        Layout.fillWidth: true
                        GridLayout {
                            //width: uiStyleGroupBox.width
                            anchors.fill: parent
                            columns: mainItem.width < UI.narrowViewWidth ? 1 : 3
                            uniformCellWidths: true
                            Repeater {
                                id: gainModeRepeater
                                model: [qsTr("System"), qsTr("Light"), qsTr("Dark") ]
                                delegate: AbracaRadioButton {
                                    id: button
                                    required property int index
                                    required property string modelData
                                    Layout.fillWidth: true
                                    text: modelData
                                    checked: settingsBackend.applicationTheme === index
                                    onToggled: {
                                        if (checked) {
                                            settingsBackend.requestVisualStyle(index)
                                        }
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
                        id: uiGroupBox
                        title: qsTr("User interface")
                        Layout.fillWidth: true
                        Item {
                            width: uiGroupBox.width
                            implicitHeight: uiSettingsLayout.implicitHeight
                            ColumnLayout {
                                id: uiSettingsLayout
                                anchors.fill: parent
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Fullscreen mode")
                                    checked: settingsBackend.fullscreen
                                    onCheckedChanged: {
                                        if (settingsBackend.fullscreen !== checked) {
                                            settingsBackend.fullscreen = checked
                                        }
                                    }
                                    visible: UI.isAndroid === true
                                    wrapMode: Text.WordWrap
                                    elideMode: Text.ElideNone
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Dynamic Label Plus (DL+)")
                                    checked: settingsBackend.showDlPlus
                                    onCheckedChanged: {
                                        if (settingsBackend.showDlPlus !== checked) {
                                            settingsBackend.showDlPlus = checked
                                        }
                                    }
                                    wrapMode: Text.WordWrap
                                    elideMode: Text.ElideNone
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Show tray icon")
                                    checked: settingsBackend.showTrayIcon
                                    onCheckedChanged: {
                                        if (settingsBackend.showTrayIcon !== checked) {
                                            settingsBackend.showTrayIcon = checked
                                        }
                                    }
                                    visible: UI.isAndroid === false
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Show system time when DAB time is not available")
                                    checked: settingsBackend.showSystemTime
                                    onCheckedChanged: {
                                        if (settingsBackend.showSystemTime !== checked) {
                                            settingsBackend.showSystemTime = checked
                                        }
                                    }
                                    wrapMode: Text.WordWrap
                                    elideMode: Text.ElideNone
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Show ensemble country flag (internet connection required)")
                                    checked: settingsBackend.showEnsembleCountryFlag
                                    onCheckedChanged: {
                                        if (settingsBackend.showEnsembleCountryFlag !== checked) {
                                            settingsBackend.showEnsembleCountryFlag = checked
                                        }
                                    }
                                    wrapMode: Text.WordWrap
                                    elideMode: Text.ElideNone
                                }
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Show service country flag (internet connection required)")
                                    checked: settingsBackend.showServiceCountryFlag
                                    onCheckedChanged: {
                                        if (settingsBackend.showServiceCountryFlag !== checked) {
                                            settingsBackend.showServiceCountryFlag = checked
                                        }
                                    }
                                    wrapMode: Text.WordWrap
                                    elideMode: Text.ElideNone
                                }

                                GridLayout  {
                                    Layout.fillWidth: true
                                    columns: 4
                                    columnSpacing: UI.standardMargin
                                    rowSpacing: UI.standardMargin
                                    AbracaLabel {
                                        text: qsTr("Slideshow background:")
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: languageComboBox.width * 0.5
                                        Layout.preferredHeight: languageComboBox.height * 0.7
                                        color: settingsBackend.slsBackgroundColor
                                        radius: 4
                                        border.color: UI.colors.controlBorder
                                        border.width: 1
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            acceptedButtons: Qt.LeftButton
                                            onClicked: {
                                                colorDialogLoader.initialColor = settingsBackend.slsBackgroundColor;
                                                colorDialogLoader.item.open();
                                            }
                                        }

                                    }
                                    Item { Layout.columnSpan: 2; Layout.fillWidth: true }
                                    AbracaLabel {
                                        text: qsTr("Language:")
                                    }
                                    AbracaComboBox {
                                        id: languageComboBox
                                        //Layout.preferredWidth: implicitWidth
                                        model: settingsBackend.languageSelectionModel
                                        textRole: "itemName"
                                        currentIndex: settingsBackend.languageSelectionModel.currentIndex
                                        onActivated: {
                                            if (settingsBackend.languageSelectionModel.currentIndex !== currentIndex) {
                                                settingsBackend.languageSelectionModel.currentIndex = currentIndex;
                                            }
                                        }
                                    }
                                    AbracaButton {
                                        id: restartButton
                                        text: qsTr("Restart")
                                        onClicked: settingsBackend.requestRestart()
                                        visible: settingsBackend.languageChanged
                                    }
                                    Item { Layout.fillWidth: true }

                                    AbracaLabel {
                                        Layout.columnSpan: 4
                                        text: qsTr("Language change will take effect after application restart.")
                                        color: "red"
                                        visible: settingsBackend.languageChanged
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
                        id: dataStorageGroupBox
                        title: qsTr("Data storage")
                        Layout.fillWidth: true
                        Item {
                            width: dataStorageGroupBox.width
                            implicitHeight: dataStorageLayout.implicitHeight
                            ColumnLayout {
                                id: dataStorageLayout
                                anchors.fill: parent
                                spacing: UI.standardMargin
                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: dataStoragePathLayout.implicitHeight
                                    RowLayout {
                                        id: dataStoragePathLayout
                                        anchors.fill: parent
                                        spacing: UI.standardMargin
                                        AbracaLabel {
                                            text: qsTr("Folder:")
                                            role: UI.LabelRole.Secondary
                                        }
                                        AbracaLabel {
                                            Layout.fillWidth: true
                                            text: settingsBackend.getDisplayPath(settingsBackend.dataStoragePath)
                                            elide: Text.ElideLeft
                                        }
                                    }
                                }
                                AbracaButton {
                                    text: qsTr("Data storage folder...")
                                    onClicked: {
                                        dirDialogLoader.dirpath = settingsBackend.dataStoragePathUrl();
                                        dirDialogLoader.active = true;
                                    }
                                }
                                AbracaLabel {
                                    id: dataStorageInfoLabel
                                    Layout.fillWidth: true
                                    font.italic: true
                                    text: qsTr("Application uses this folder to store all data like audio recording, IQ recording and logs, etc.")
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                    AbracaLine {
                        Layout.fillWidth: true
                        isVertical: false
                        Layout.topMargin: UI.standardMargin
                        Layout.bottomMargin: 2*UI.standardMargin
                        visible: UI.isDesktop
                    }
                    AbracaGroupBox {
                        title: qsTr("Network proxy configuration")
                        Layout.fillWidth: true
                        visible: UI.isDesktop
                        GridLayout {
                            //width: proxyGroupBox.width
                            anchors.fill: parent
                            columns: appUI.isPortraitView ? 3 : 5
                            rowSpacing: UI.standardMargin
                            columnSpacing: UI.standardMargin

                            // row 1
                            AbracaLabel {
                                text: qsTr("Proxy type:")
                            }
                            AbracaComboBox {
                                model: settingsBackend.proxyConfigModel
                                textRole: "itemName"
                                currentIndex: settingsBackend.proxyConfigModel.currentIndex
                                onActivated: {
                                    if (settingsBackend.proxyConfigModel.currentIndex !== currentIndex) {
                                        settingsBackend.proxyConfigModel.currentIndex = currentIndex
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                AbracaButton {
                                    text: qsTr("Apply")
                                    enabled: settingsBackend.isProxyApplyEnabled
                                    onClicked: settingsBackend.requestApplyProxyConfig()
                                }
                            }
                            Item { Layout.fillWidth: true; Layout.columnSpan: 2; visible: appUI.isPortraitView === false }

                            // row 1
                            AbracaLabel {
                                text: qsTr("Server:")
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                            }
                            AbracaTextField {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                                text: settingsBackend.proxyServer
                                onTextChanged: {
                                    if (settingsBackend.proxyServer !== text) {
                                        settingsBackend.proxyServer = text
                                        settingsBackend.isProxyApplyEnabled = true
                                    }
                                }
                            }
                            AbracaLabel {
                                text: qsTr("Port:")
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                            }
                            AbracaSpinBox {
                                from: 0
                                to: 65535
                                editable: true
                                locale: Qt.locale("C")
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                                value: settingsBackend.proxyPort
                                onValueChanged: {
                                    if (settingsBackend.proxyPort !== value) {
                                        settingsBackend.proxyPort = value
                                        settingsBackend.isProxyApplyEnabled = true
                                    }
                                }
                            }
                            Item { visible: appUI.isPortraitView === true }

                            AbracaLabel {
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                                text: qsTr("Username:")
                            }
                            AbracaTextField {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                                text: settingsBackend.proxyUser
                                onTextChanged: {
                                    if (settingsBackend.proxyUser !== text) {
                                        settingsBackend.proxyUser = text
                                        settingsBackend.isProxyApplyEnabled = true
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true; Layout.columnSpan: 2; visible: appUI.isPortraitView === false }
                            AbracaLabel {
                                text: qsTr("Password:")
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                            }
                            AbracaTextField {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                echoMode: TextInput.Password
                                enabled: settingsBackend.proxyConfigModel.currentIndex === 2
                                text: settingsBackend.proxyPass
                                onTextChanged: {
                                    if (settingsBackend.proxyPass !== text) {
                                        settingsBackend.proxyPass = text
                                        settingsBackend.isProxyApplyEnabled = true
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
                        id: fmlistGroupbox
                        title: qsTr("FMLIST")
                        Layout.fillWidth: true
                        Item {
                            width: fmlistGroupbox.width
                            implicitHeight: fmlistLayout.implicitHeight
                            ColumnLayout {
                                id: fmlistLayout
                                anchors.fill: parent
                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: implicitWidth
                                    text: qsTr("Upload ensemble information")
                                    checked: settingsBackend.uploadEnsembleInfo
                                    onCheckedChanged: {
                                        if (settingsBackend.uploadEnsembleInfo !== checked) {
                                            settingsBackend.uploadEnsembleInfo = checked
                                        }
                                    }
                                }
                                AbracaLabel {
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 600
                                    font.italic: true
                                    // textFormat: Text.RichText
                                    wrapMode: Text.WordWrap
                                    text: qsTr("Ensemble information is a small CSV file with list of services in the ensemble,\nit is anonymous and contains no personal data.")
                                }
                                AbracaLabel {
                                    Layout.fillWidth: true
                                    font.italic: true
                                    textFormat: Text.StyledText
                                    text: settingsBackend.uploadEnsembleInfo
                                          ? qsTr("Application automatically uploads ensemble information to <a href=\"https://www.fmlist.org/\">FMLIST</a>.")
                                          : qsTr("Upload of ensemble information to <a href=\"https://www.fmlist.org/\">FMLIST</a> is currently disabled.")
                                    wrapMode: Text.WordWrap
                                }
                                AbracaLabel {
                                    Layout.fillWidth: true
                                    font.italic: true
                                    textFormat: Text.StyledText
                                    text: settingsBackend.uploadEnsembleInfo
                                          ? qsTr("Thank you for supporting the community!")
                                          : qsTr("Please consider enabling this option to help the community.")
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
                    AbracaSwitch {
                        Layout.fillWidth: true
                        Layout.minimumWidth: implicitWidth
                        text: qsTr("Restore application windows on start")
                        checked: settingsBackend.restoreWindowsOnStart
                        onCheckedChanged: {
                            if (settingsBackend.restoreWindowsOnStart !== checked) {
                                settingsBackend.restoreWindowsOnStart = checked
                            }
                        }
                        visible: UI.isAndroid === false
                    }
                    AbracaSwitch {
                        Layout.fillWidth: true
                        Layout.minimumWidth: implicitWidth
                        text: qsTr("Check for application update on start")
                        checked: settingsBackend.isCheckForUpdatesEnabled
                        onCheckedChanged: {
                            if (settingsBackend.isCheckForUpdatesEnabled !== checked) {
                                settingsBackend.isCheckForUpdatesEnabled = checked
                            }
                        }
                    }

                    AbracaSwitch {
                        Layout.fillWidth: true
                        Layout.minimumWidth: implicitWidth
                        text: qsTr("Include XML header in raw data recording")
                        checked: settingsBackend.isXmlHeaderEnabled
                        onCheckedChanged: {
                            if (settingsBackend.isXmlHeaderEnabled !== checked) {
                                settingsBackend.isXmlHeaderEnabled = checked
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
    Loader {
        id: colorDialogLoader
        property color initialColor: "white"
        asynchronous: true
        active: true
        sourceComponent: ColorDialog {
            id: colorDialog
            title: qsTr("Select SLS Background Color")
            options: ColorDialog.ShowAlphaChannel
            selectedColor: colorDialogLoader.initialColor
            onAccepted: settingsBackend.setSlsBackground(selectedColor)
        }
    }
    Loader {
        id: dirDialogLoader
        property string dirpath: ""
        asynchronous: true
        active: false
        onLoaded: item.open()
        sourceComponent: FolderDialog {
            id: dirDialog
            title: qsTr("Data storage folder")
            options: FileDialog.DontResolveSymlinks
            currentFolder: dirDialogLoader.dirpath
            onAccepted: {
                settingsBackend.selectDataStoragePath(selectedFolder)
                dirDialogLoader.active = false
            }
            onRejected: {
                dirDialogLoader.active = false
            }
        }
    }
}
