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

//import QtQuick.Controls.Material
//import QtQuick.Controls.Universal
//import QtQuick.Controls.Imagine
import QtQuick.Controls.Basic
import QtQuick
import QtQuick.Layouts

import abracaComponents

ApplicationWindow {
    id: appWindow

    visible: true
    title: qsTr("AbracaDABra")
    // minimumHeight: 400
    // minimumWidth: 380
    minimumHeight: appUI.isCompact ? 500 : 600
    minimumWidth: 675


    color: UI.colors.background

    Connections {
        target: Application.styleHints
        function onColorSchemeChanged() {
            console.log("Color scheme changed:", Application.styleHints.colorScheme);
            appUI.isSystemDarkMode = (Application.styleHints.colorScheme === Qt.ColorScheme.Dark);
        }
    }

    // // initial size
    // height: Math.min(Screen.height, 640)
    // width: Math.min(Screen.width, 1024)

    function quitApplication() {
        application.saveWindowGeometry(appWindow.x, appWindow.y, appWindow.width, appWindow.height);
        application.saveUndockedWindows();
        contentView.dockAll();
        application.close();
    }

    onClosing: function (close) {
        quitApplication();
        close.accepted = false;
    }

    Navigation {
        id: navigation
    }

    ContentView {
        id: contentView
        isSmallNavigation: navigation.isSmallNavigation
    }
    Connections {
        target: navigation
        function onFunctionalityClicked(funcId) {
            contentView.handleFunctionalityClicked(funcId);
        }
        function onFunctionalityUndockRequested(funcId) {
            contentView.handleFunctionalityUndocking(funcId);
        }
    }

    /*
    function delayedFunctionCall(callback, delayMs) {
        var timer = Qt.createQmlObject('import QtQuick; Timer { repeat: false }', this);
        timer.interval = delayMs;
        timer.triggered.connect(function () {
            callback();
            timer.destroy(); // clean up
        });
        timer.start();
    }
    */
    Connections {
        target: appUI
        function onIsPortraitViewChanged() {
            Qt.callLater(function () {
                // align current id for what is availble
                navigationModel.alignIdForCurrentView(appUI.isPortraitView);
                if (UI.isAndroid && settingsBackend.fullscreen === false  ) {
                    application.setAndroidNavigationBar();
                }
            });
        }
    }

    Component {
        id: signalStatus
        Rectangle {
            color: UI.colors.statusbarBackground
            radius: 6
            implicitHeight: sigStatusLayout.implicitHeight + 2 * 5
            RowLayout {
                id: sigStatusLayout
                anchors.fill: parent
                anchors.margins: 5
                SignalState {
                    id: signalState
                }
                Item {
                    Layout.fillWidth: true
                }
                AudioRecordingStatus {
                    visible: audioRecording.isAudioRecordingActive
                    showLabel: false
                    Layout.preferredHeight: 20
                }
                Item {
                    Layout.fillWidth: true
                }
                AbracaLabel {
                    text: appUI.timeLabel
                    toolTipText: appUI.timeLabelToolTip
                }
            }
        }
    }

    ColumnLayout {
        id: smallLayout
        anchors.fill: parent
        spacing: 0

        Loader {
            sourceComponent: signalStatus
            Layout.fillWidth: true
            Layout.margins: 5
        }

        LayoutItemProxy {
            target: contentView
            Layout.fillHeight: true
            Layout.fillWidth: true
        }

        AbracaLine {
            Layout.fillWidth: true
            isVertical: false
        }

        LayoutItemProxy {
            target: navigation
            Layout.fillWidth: true
            Layout.preferredHeight: navigation.implicitHeight
        }
    }

    Item {
        id: largeLayout
        anchors.fill: parent
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: statusBar.top
            spacing: 0

            LayoutItemProxy {
                target: navigation
                Layout.fillHeight: true
                Layout.preferredWidth: navigation.implicitWidth
            }

            AbracaLine {
                Layout.fillHeight: true
                isVertical: true
            }

            LayoutItemProxy {
                target: contentView
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
        }

        StatusBar {
            id: statusBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

        MessageBox {
            id: messageBox
            messageBoxBackend: application.messageBoxBackend
        }
    }

    Loader {
        active: UI.isAndroid === false
        source: "SystemTray.qml"
        onLoaded: {
            item.appWindow = appWindow;
            item.application = application;
            item.appUI = appUI;
            item.settingsBackend = settingsBackend;
        }
    }

    function setFittingLayout() {
        if (appWindow.width < appUI.serviceViewLandscapeWidth + UI.navigationButtonSize + 2 * UI.standardMargin) {
            smallLayout.visible = true;
            largeLayout.visible = false;
            if (appWindow.width < UI.narrowViewWidth) {
                appUI.currentView = ApplicationUI.PortraitNarrowView;
            }
            else {
                appUI.currentView = ApplicationUI.PortraitWideView;
            }
        } else {
            smallLayout.visible = false;
            largeLayout.visible = true;
            appUI.currentView = ApplicationUI.LandscapeView;
        }
    }
    onWidthChanged: setFittingLayout()

    Component.onCompleted: {
        if (UI.isAndroid) {
            // Apply fullscreen on Android if enabled
            if (settingsBackend.fullscreen) {
                appWindow.visibility = Window.FullScreen;
            }
        }
        else {
            var geometry = application.restoreWindowGeometry();
            if (geometry.x >= 0 && geometry.y >= 0) {
                appWindow.x = geometry.x;
                appWindow.y = geometry.y;
            }
            if (geometry.width > 0 && geometry.height > 0) {
                appWindow.width = Math.min(Screen.width, geometry.width);
                appWindow.height = Math.min(Screen.height, geometry.height);
            }
        }
        setFittingLayout()

        appUI.isSystemDarkMode = (Application.styleHints.colorScheme === Qt.ColorScheme.Dark);
    }

    Connections {
        target: settingsBackend
        function onFullscreenChanged() {
            if (UI.isAndroid) {
                if (settingsBackend.fullscreen) {
                    appWindow.visibility = Window.FullScreen;
                } else {
                    appWindow.visibility = Window.Windowed;
                }
            }
        }
    }
    onVisibilityChanged: {
        if (UI.isAndroid && settingsBackend.fullscreen === false  ) {
            application.setAndroidNavigationBar();
        }
    }

    Connections {
        target: application
        function onStartBandScan() {
            startBandScan();
        }
        function onUpdateAvailable() {
            dialogLoader.sourceComponent = updateComponent;
            dialogLoader.active = true;
        }
        function onShowMessage() {
            dialogLoader.sourceComponent = messageDialogComponent;
            dialogLoader.active = true;
        }
        function onShowPageRequest(pageId) {
            contentView.showPage(pageId);
        }
        function onApplicationQuitEvent() {
            quitApplication();
        }
    }

    function startBandScan() {
        dialogLoader.sourceComponent = bandScanComponent;
        dialogLoader.dialogBackend = application.createBackend(NavigationModel.BandScan);
        dialogLoader.active = true;
    }

    Connections {
        target: audioRecording
        function onShowRecordingItemDialog() {
            dialogLoader.sourceComponent = audioRecItemComponent;
            dialogLoader.active = true;
        }
    }

    Loader {
        id: dialogLoader
        property var dialogBackend: null
        active: false
        asynchronous: true
        onLoaded: item.open()
    }

    Component {
        id: bandScanComponent
        BandScan {
            id: bandScanDialog
            backend: dialogLoader.dialogBackend
            onFinished: closeAndDestroy()

            function closeAndDestroy() {
                backend.destroy();
                dialogLoader.active = false;
            }
        }
    }
    Component {
        id: audioRecItemComponent
        AudioRecordingItemDialog {
            id: audioRecItemDialog
            onAccepted: {
                audioRecording.addOrEditItem();
                dialogLoader.active = false;
            }
            onRejected: dialogLoader.active = false
            onClosed: dialogLoader.active = false
        }
    }
    Component {
        id: updateComponent
        UpdateDialog {
            id: updateDialog
            onClosed: dialogLoader.active = false
            onRejected: {
                settingsBackend.isCheckForUpdatesEnabled = false;
                dialogLoader.active = false;
            }
        }
    }
    Component {
        id: messageDialogComponent
        AbracaDialog {
            id: messageDialog
            x: (appWindow.width - width) / 2
            y: (appWindow.height - height) / 2
            title: appUI.messageInfoTitle
            modal: true
            canEscape: true
            implicitHeight: contentItem.implicitHeight + 2 * contentMargin + header.implicitHeight
            contentItem: ColumnLayout {
                id: messageContent
                AbracaLabel {
                    text: appUI.messageInfoDetails
                }
            }
            onClosed: dialogLoader.active = false
            onRejected: dialogLoader.active = false
        }
    }
    AbracaMessage {
        id: infoMessage
        Connections {
            target: application
            function onShowInfoMessage(text : string, type : int) {
                infoMessage.text = text;
                infoMessage.messageType = type;
                infoMessage.visible = true;
            }
        }
    }
}
