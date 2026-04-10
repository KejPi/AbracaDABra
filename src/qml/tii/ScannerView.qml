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
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import abracaComponents

Item {
    id: scannerView
    anchors.fill: parent
    property var scannerBackend: application.scannerBackend

    function storeSplitterState() {
        scannerBackend.splitterState = splitView.saveState();
    }
    Component.onCompleted: {
        splitView.restoreState(scannerBackend.splitterState);
    }
    Component.onDestruction: {
        storeSplitterState();
    }
    Connections {
        target: appWindow
        function onClosing() {
            storeSplitterState();
        }
    }

    AbracaSplitView {
        id: splitView
        anchors.fill: parent

        orientation: Qt.Vertical

        TIIMap {
            id: mapView
            backend: scannerBackend
            dragEnabled: !scannerSetupDrawerLoader.item || !scannerSetupDrawerLoader.item.opened
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            SplitView.minimumHeight: 200
            // SplitView.preferredHeight: splitView.height * 0.5
        }

        ColumnLayout {
            id: bottomPane
            SplitView.fillWidth: true
            SplitView.minimumHeight: 200
            // SplitView.preferredHeight: implicitHeight
            spacing: 0 //UI.standardMargin

            readonly property bool showSetupControls: (scannerView.width > setupControlsItem.implicitWidth
                                                                            + startStopButton.implicitWidth
                                                                            + menuButton.implicitWidth
                                                                            + channelSelectButton.implicitWidth
                                                                            + 290 /*signal state, label, margins and spacings*/)

            // dynamic menu creation
            Component {
                id: setupMenuItemComponent
                AbracaMenuItem {
                    text: qsTr("Configuration...")
                    checkable: false
                    onTriggered: scannerSetupDialogLoader.active = true
                }
            }
            Component {
                id: menuSeparatorItemComponent
                AbracaMenuSeparator { }
            }

            onShowSetupControlsChanged: {
                updateMenuItems();
            }
            Component.onCompleted: {
                updateMenuItems();
            }

            property var menuItems: []
            function updateMenuItems() {
                if (bottomPane.showSetupControls) {
                    // delete menu items
                    if (menuItems.length > 0) {
                        optionsMenu.removeItem(menuItems[0])
                        optionsMenu.removeItem(menuItems[1])
                        menuItems.length = 0;
                    }
                } else {
                    // create menu items
                    if (menuItems.length === 0) {
                        let setupMenuItem = setupMenuItemComponent.createObject( optionsMenu.contentItem, { })
                        let menuSeparatorItem = menuSeparatorItemComponent.createObject(optionsMenu.contentItem, { })

                        optionsMenu.insertItem(0, menuSeparatorItem)
                        optionsMenu.insertItem(0, setupMenuItem)
                        menuItems.push(setupMenuItem)
                        menuItems.push(menuSeparatorItem)
                    }
                }
            }

            RowLayout {
                id: controlsItem
                Layout.fillWidth: true
                Layout.topMargin: UI.standardMargin
                Layout.leftMargin: UI.standardMargin
                AbracaLabel {
                    text: scannerBackend.scanningLabel
                    font.bold: scannerBackend.isScanning
                }
                AbracaLabel {
                    text: scannerBackend.progressChannel
                    visible: scannerBackend.isScanning
                }
                SignalState {
                    Layout.leftMargin: UI.standardMargin
                    showSnrLabel: true
                    visible: scannerBackend.isScanning
                }
                Item {
                    Layout.fillWidth: true
                }                
                Item {
                    id: setupControlsItem
                    visible: bottomPane.showSetupControls
                    implicitHeight: setupControlsLayout.implicitHeight
                    implicitWidth: setupControlsLayout.implicitWidth
                    RowLayout {
                        id: setupControlsLayout
                        anchors.fill: parent
                        AbracaLabel {
                            Layout.leftMargin: UI.standardMargin
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
                            Layout.leftMargin: UI.standardMargin
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
                AbracaButton {
                    id: channelSelectButton
                    Layout.leftMargin: UI.standardMargin
                    text: qsTr("Select channels")
                    enabled: !scannerBackend.isScanning
                    onClicked: channelSelectionDialogLoader.active = true
                    visible: appUI.isPortraitView === false && UI.isMobile === false
                }
                AbracaButton {
                    id: startStopButton
                    text: scannerBackend.isScanning ? qsTr("Stop") : qsTr("Start")
                    enabled: scannerBackend.isStartStopEnabled && scannerBackend.isScanningEnabled
                    onClicked: scannerBackend.startStopAction()
                    buttonRole: UI.ButtonRole.Primary
                }
                AbracaImgButton {
                    id: menuButton
                    source: UI.imagesUrl + "icon-config.svg"
                    onClicked: {
                        if (channelSelectButton.visible) {
                            optionsMenu.open()
                        }
                        else {
                            scannerSetupDrawerLoader.active = true
                        }
                    }

                    AbracaMenu {
                        id: optionsMenu
                        y: menuButton.height
                        width: {
                            var result = 0;
                            var padding = 0;
                            for (var i = 0; i < count; ++i) {
                                var item = itemAt(i);
                                result = Math.max(item.contentItem.implicitWidth, result);
                                padding = Math.max(item.padding, padding);
                            }
                            return result + padding * 2;
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
                        AbracaMenuSeparator {}
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
                }
            }        
            AbracaProgressBar {
                Layout.margins: UI.standardMargin
                Layout.fillWidth: true
                from: 0
                to: scannerBackend.progressMax
                value: scannerBackend.progressValue
            }
            AbracaTableView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: splitView.height * 0.1
                Layout.margins: UI.standardMargin
                Layout.topMargin: 0

                model: scannerBackend.tableModel
                selectionModel: scannerBackend.tableSelectionModel
                shrinkColumnIndex: TxTableModel.ColLocation
                contextMenuModel: scannerBackend.contextMenuModel
                sortingEnabled: true
                onDoubleClickedRow: function(row) {
                    scannerBackend.showEnsembleConfig(row)
                }
                onPopulateContextMenu: function(row) {
                    scannerBackend.createContextMenu(row)
                }
                sortIndicatorColumn: scannerBackend.txTableSortCol
                onSortIndicatorColumnChanged: {
                    if (sortIndicatorColumn !== scannerBackend.txTableSortCol) {
                        scannerBackend.txTableSortCol = sortIndicatorColumn;
                    }
                }
                sortIndicatorOrder: scannerBackend.txTableSortOrder
                onSortIndicatorOrderChanged: {
                    if (sortIndicatorOrder !== scannerBackend.txTableSortOrder) {
                        scannerBackend.txTableSortOrder = sortIndicatorOrder;
                    }
                }
                // minColumnWidth: 50
            }

        }
    }

    Connections {
        target: channelSelectionDialogLoader.item
        enabled: channelSelectionDialogLoader.item !== null

        function onAccepted() {
            scannerBackend.channelSelectionModel.save();
            channelSelectionDialogLoader.active = false
        }
        function onRejected() {
            channelSelectionDialogLoader.active = false            
        }
        function onClosed() {
            channelSelectionDialogLoader.active = false
        }
    }

    Loader {
        id: channelSelectionDialogLoader
        active: false
        asynchronous: true
        onLoaded: {
            item.open()
            // item.accepted.connect(function() {
            //     scannerBackend.channelSelectionModel.save()
            //     channelSelectionDialogLoader.active = false
            // })
            // item.rejected.connect(function() {
            //     channelSelectionDialogLoader.active = false
            // })
        }
        sourceComponent: ChannelSelectionDialog { }
    }

    Connections {
        target: scannerBackend
        function onShowEnsembleConfigDialog(row, ensConfig) {
            ensembleConfigDialogLoader.modelRow = row
            ensembleConfigDialogLoader.ensembleText = ensConfig
            ensembleConfigDialogLoader.active = true
        }
    }

    Connections {
        target: ensembleConfigDialogLoader.item
        enabled: ensembleConfigDialogLoader.item !== null

        function onAccepted() {
            ensembleConfigDialogLoader.active = false
        }
        function onRejected() {        
            ensembleConfigDialogLoader.active = false
        }
        function onClosed() {
            ensembleConfigDialogLoader.active = false
        }
        function onExportCsvRequest(sourceRow) {
            scannerBackend.saveEnsembleCSV(sourceRow)
        }
    }

    Loader {
        id: ensembleConfigDialogLoader
        active: false
        asynchronous: true
        onLoaded: {
            item.open()
        }
        property string ensembleText: ""
        property int modelRow: -1

        sourceComponent: EnsembleConfigDialog {
            modelRow: ensembleConfigDialogLoader.modelRow
            ensembleText: ensembleConfigDialogLoader.ensembleText
        }
    }

    Connections {
        target: scannerSetupDialogLoader.item
        enabled: scannerSetupDialogLoader.item !== null

        function onAccepted() {
            scannerSetupDialogLoader.active = false
        }
        function onRejected() {
            scannerSetupDialogLoader.active = false
        }
        function onClosed() {
            scannerSetupDialogLoader.active = false
        }
    }

    Loader {
        id: scannerSetupDialogLoader
        active: false
        asynchronous: true
        onLoaded: {
            item.open()
        }
        sourceComponent: ScannerSetupDialog { }
    }

    Connections {
        target: scannerBackend
        function onOpenFileDialog() {
            fileDialogLoader.filename = "";
            fileDialogLoader.filepath = scannerBackend.csvPath();
            fileDialogLoader.active = true;
        }
    }

    Loader {
        id: scannerSetupDrawerLoader
        active: false
        asynchronous: true
        onLoaded: {
            item.open()
        }
        sourceComponent: ScannerSetupDrawer { }
    }

    Connections {
        target: scannerSetupDrawerLoader.item
        enabled: scannerSetupDrawerLoader.item !== null

        function onClosed() {
            scannerSetupDrawerLoader.active = false
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
            // On Android, avoid using nameFilters as they don't work reliably with extensions
            // Leave empty on Android to allow all files to be selectable
            nameFilters: UI.isAndroid ? [] : [qsTr("CSV files") + " (*.csv)"]
            options: UI.isAndroid ? FileDialog.DontResolveSymlinks : 0
            // currentFolder doesn't work well on Android with content:// URIs
            currentFolder: UI.isAndroid ? "" : fileDialogLoader.filepath
            onAccepted: {
                scannerBackend.loadCSV(selectedFile);
                fileDialogLoader.active = false;
            }
            onRejected: {
                fileDialogLoader.active = false;
            }
        }
    }
    MessageBox {
        id: messageBox
        messageBoxBackend: scannerBackend.messageBoxBackend
    }
    AbracaMessage {
        id: infoMessage
        Connections {
            target: scannerBackend
            function onShowInfoMessage(text : string, type : int) {
                infoMessage.text = text;
                infoMessage.messageType = type;
                infoMessage.visible = true;
            }
        }
    }

    // Busy indicator overlay shown during CSV loading
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.3)
        visible: scannerBackend.isLoading
        z: 100

        AbracaBusyIndicator {
            anchors.centerIn: parent
            drawColor: "white"
            running: scannerBackend.isLoading
            width: 64
            height: 64
        }
    }
}
