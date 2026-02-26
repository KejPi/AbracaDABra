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
import QtQuick.Controls
import QtQuick.Layouts

import abracaComponents

UndockablePage {
    id: mainItem

    pageId: "ensembleInfoPage"
    title: "Ensemble information"
    minWidth: 2 * UI.narrowViewWidth
    minHeight: 550

    content: Item {
        id: contentItem
        anchors.fill: parent

        onWidthChanged: setFittingLayout()
        Component.onCompleted: setFittingLayout()

        readonly property int recControlsWidth: timeoutSpinBox.implicitWidth + timeoutSwitch.implicitWidth + recordButton.implicitWidth + 2*UI.standardMargin
        readonly property int controlRowMinWidth: recControlsWidth + exportCSVButton.implicitWidth + uploadCSVButton.implicitWidth + 280 + 2*UI.standardMargin
        readonly property int controlGridMinWidth: recControlsWidth
                                                       + uploadCSVButton.implicitWidth + exportCSVButton.implicitWidth // using CSV bottons as width reference
                                                       + 3 * UI.standardMargin

        function setFittingLayout() {
            //console.log(contentItem.width, recControlsWidth, controlRowMinWidth, controlGridMinWidth)

/*
            if (contentItem.width > controlRowMinWidth) {
                mobileLayout.visible = false;
                controlGrid.visible = false;
                controlGridEnsText.visible = false;
                controlRow.visible = true;
                console.log("--------------- Using controlRow layout", contentItem.width, controlRowMinWidth)
            } else {
                controlRow.visible = false;
                if (contentItem.width > controlGridMinWidth) {
                    if (ensTextItem.visible) {
                        controlGridEnsText.visible = true;
                        controlGrid.visible = false;
                        console.log("--------------- Using controlGridEnsText layout", contentItem.width, controlGridMinWidth, controlGrid.implicitWidth)
                    } else {
                        controlGridEnsText.visible = false;
                        controlGrid.visible = true;
                        console.log("--------------- Using controlGrid layout", contentItem.width, controlGridMinWidth, controlGrid.implicitWidth)
                    }
                    mobileLayout.visible = false;
                } else {
                    controlGrid.visible = false;
                    controlGridEnsText.visible = false;
                    mobileLayout.visible = true;
                    console.log("--------------- Using mobileLayout layout", contentItem.width, controlGridMinWidth, controlGrid.implicitWidth)
                }
            }
*/
            if (contentItem.width > controlRowMinWidth) {
                mobileLayout.visible = false;
                tabletLayout.visible = false;
                controlGridEnsTextVisible.visible = false;
                controlGridEnsTextHidden.visible = false;
                controlRow.visible = true;
            }
            else if (contentItem.width > controlGridMinWidth) {
                mobileLayout.visible = false;
                tabletLayout.visible = false;
                controlGridEnsTextVisible.visible = ensTextItem.visible;
                controlGridEnsTextHidden.visible = !ensTextItem.visible;
                controlRow.visible = false;
            }
            else if (contentItem.width < UI.narrowViewWidth) {
                mobileLayout.visible = true;
                tabletLayout.visible = false;
                controlGridEnsTextVisible.visible = false;
                controlGridEnsTextHidden.visible = false;
                controlRow.visible = false;
            }
            else {
                mobileLayout.visible = false;
                tabletLayout.visible = true;
                controlGridEnsTextVisible.visible = false;
                controlGridEnsTextHidden.visible = false;
                controlRow.visible = false;
            }
        }

        FontMetrics {
            id: fontMetrics
            font: recordingLengthLabel.font
        }

        Flickable {
            id: flickable
            anchors.fill: parent
            contentWidth: contentFlickableItem.implicitWidth
            contentHeight: contentFlickableItem.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Item {
                id: contentFlickableItem
                implicitHeight: colLayout.implicitHeight + 2 * UI.standardMargin
                implicitWidth: flickable.width // colLayout.implicitWidth + 2 * UI.standardMargin
                width: flickable.width  // Math.max(flickable.width, implicitWidth)
                ColumnLayout {
                    id: colLayout
                    anchors.fill: parent
                    anchors.margins: UI.standardMargin
                    GridLayout {
                        id: infoGrid
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        // Layout.topMargin: UI.standardMargin
                        columns: (flickable.width < 2 * UI.narrowViewWidth ? (flickable.width < contentItem.controlGridMinWidth ? 1 : 2) : 4)
                        Repeater {
                            id: boxRepeater
                            model: 3
                            delegate: Rectangle {
                                id: box
                                Layout.fillWidth: true
                                Layout.preferredHeight: boxLayout.implicitHeight + 2 * UI.standardMargin

                                color: "transparent"
                                border.width: 1
                                border.color: UI.colors.inactive
                                radius: UI.controlRadius

                                required property int index
                                ColumnLayout {
                                    id: boxLayout
                                    x: UI.standardMargin
                                    y: UI.standardMargin
                                    width: box.width - 2 * UI.standardMargin
                                    Repeater {
                                        model: EnsembleInfoModel {
                                            group: box.index
                                            sourceModel: ensembleInfo
                                        }
                                        delegate: RowLayout {
                                            //height: 50
                                            //width: parent.width
                                            required property string label
                                            required property string info
                                            AbracaLabel {                                    
                                                text: label
                                                elide: Text.ElideMiddle
                                                role: UI.LabelRole.Secondary
                                            }
                                            AbracaLabel {
                                                text: info
                                                Layout.fillWidth: true
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle {
                            id: statsBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: statsBoxLayout.implicitHeight + 2 * UI.standardMargin

                            color: "transparent"
                            border.width: 1
                            border.color: UI.colors.inactive
                            radius: UI.controlRadius

                            ColumnLayout {
                                id: statsBoxLayout
                                x: UI.standardMargin
                                y: UI.standardMargin
                                width: statsBox.width - 2 * UI.standardMargin
                                Repeater {
                                    model: EnsembleInfoModel {
                                        group: 3
                                        sourceModel: ensembleInfo
                                    }
                                    delegate: RowLayout {
                                        required property string label
                                        required property string info
                                        AbracaLabel {
                                            text: label
                                            elide: Text.ElideMiddle
                                            role: UI.LabelRole.Secondary
                                        }
                                        AbracaLabel {
                                            text: info
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: (mouse)=> {
                                    if (mouse.button === Qt.RightButton) {
                                        statsContextMenu.x = mouse.x;
                                        statsContextMenu.y = mouse.y;
                                        statsContextMenu.open();
                                    }
                                }
                                onPressAndHold: (mouse) => {
                                    statsContextMenu.x = mouse.x;
                                    statsContextMenu.y = mouse.y;
                                    statsContextMenu.open();
                                }
                            }
                            AbracaMenu {
                                id: statsContextMenu
                                AbracaMenuItem {
                                    text: qsTr("Reset statistics")
                                    onTriggered: {
                                        ensembleInfo.resetFibStat()
                                        ensembleInfo.resetMscStat()
                                    }
                                }
                                AbracaMenuSeparator {}
                                AbracaMenuItem {
                                    text: qsTr("Reset FIB statistics")
                                    onTriggered: {
                                        ensembleInfo.resetFibStat()
                                    }
                                }
                                AbracaMenuItem {
                                    text: qsTr("Reset MSC statistics")
                                    onTriggered: {
                                        ensembleInfo.resetMscStat()
                                    }
                                }
                            }
                        }
                    }
                    SubchannelsView {
                        id: subchView
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        Layout.topMargin: UI.standardMargin
                        showLegend: appUI.isPortraitView || UI.isMobile
                    }
                    GridLayout {
                        id: subchGrid
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        columns: appUI.isPortraitView || (width < 850) ? 1 : 3
                        Repeater {
                            id: subchBoxRepeater
                            model: [4, 5, 6]
                            delegate: Rectangle {
                                id: subchBox
                                Layout.fillWidth: true
                                Layout.preferredHeight: subchBoxLayout.implicitHeight + 2 * UI.standardMargin

                                color: "transparent"
                                border.width: 1
                                border.color: UI.colors.inactive
                                radius: UI.controlRadius

                                enabled: subchBox.modelData === 4 || ensembleInfo.ensembleSubchModel.currentRow !== -1

                                required property int modelData
                                ColumnLayout {
                                    id: subchBoxLayout
                                    x: UI.standardMargin
                                    y: UI.standardMargin
                                    width: subchBox.width - 2 * UI.standardMargin
                                    Repeater {
                                        id: subchRepeater
                                        model: EnsembleInfoModel {
                                            group: subchBox.modelData
                                            sourceModel: ensembleInfo
                                        }
                                        delegate: RowLayout {
                                            required property string label
                                            required property string info
                                            AbracaLabel {
                                                text: label
                                                role: UI.LabelRole.Secondary
                                            }
                                            AbracaLabel {
                                                text: info
                                                Layout.fillWidth: true
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: ensTextItem
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: controlRow.visible ? (contentItem.height - infoGrid.height - subchView.height - subchGrid.height - 4 * colLayout.spacing - 3 * UI.standardMargin - controlRow.height)
                                                                   : (contentItem.height - infoGrid.height - subchView.height - subchGrid.height - 4 * colLayout.spacing - 3 * UI.standardMargin - controlGridEnsTextVisible.implicitHeight)
                        Layout.minimumHeight: 150
                        visible: controlRow.visible || (Layout.preferredHeight > 150 && contentItem.width > contentItem.controlGridMinWidth)
                        onVisibleChanged:  contentItem.setFittingLayout()
                        AbracaScrollView {
                            id: textEditFlickable
                            anchors.fill: parent
                            AbracaTextArea {
                                id: ensEdit
                                textFormat: TextEdit.RichText
                                text: ensembleInfo.ensembleConfigurationText
                                readOnly: true
                            }
                        }
                    }

                    // // -------------------------------------
                    // // components for bottom layouts
                    AbracaButton {
                        id: showEnsTextButton
                        text: qsTr("Show ensemble configuration")
                        Layout.alignment: Qt.AlignHCenter
                        enabled: ensembleInfo.ensembleConfigurationText.length > 0
                        onClicked: {
                            if (mainItem.width < UI.narrowViewWidth || UI.isAndroid) {
                                ensembleConfigDrawerLoader.active = true
                            } else {
                                ensembleConfigDialogLoader.active = true
                            }
                        }
                    }
                    AbracaButton {
                        id: exportCSVButton
                        enabled: ensembleInfo.isCsvExportEnabled
                        text: qsTr("Export as CSV")
                        onClicked: {
                            ensembleInfo.saveCSV();
                        }
                    }
                    AbracaButton {
                        id: uploadCSVButton
                        text: qsTr("Upload to FMLIST")
                        onClicked: ensembleInfo.uploadCSV()
                        enabled: ensembleInfo.isCsvUploadEnabled && ensembleInfo.isRecordingVisible // recoring visible false => playing from file
                    }

                    AbracaSwitch {
                        id: timeoutSwitch
                        text: qsTr("Timeout")
                        toolTipText: qsTr("When checked recording stops automatically when timeout is reached.")
                        visible: ensembleInfo.isRecordingVisible
                        enabled: ensembleInfo.isRecordingEnabled && !ensembleInfo.isRecordingOngoing
                        checked: ensembleInfo.isTimeoutEnabled
                        onCheckedChanged: if (ensembleInfo.isTimeoutEnabled !== checked) {
                            ensembleInfo.isTimeoutEnabled = checked;
                        }
                    }
                    AbracaSpinBox {
                        id: timeoutSpinBox
                        enabled: timeoutSwitch.checked && ensembleInfo.isRecordingEnabled && !ensembleInfo.isRecordingOngoing
                        visible: ensembleInfo.isRecordingVisible
                        value: ensembleInfo.recordingTimeout
                        stepSize: 10
                        from: 10
                        to: 7200
                        editable: true
                        suffix: " sec"
                        onValueChanged: if (value !== ensembleInfo.recordingTimeout) {
                            ensembleInfo.recordingTimeout = value;
                        }
                    }
                    AbracaButton {
                        id: recordButton
                        buttonRole: ensembleInfo.isRecordingOngoing ? UI.ButtonRole.Negative : UI.ButtonRole.Neutral
                        visible: ensembleInfo.isRecordingVisible
                        enabled: ensembleInfo.isRecordingEnabled
                        text: ensembleInfo.isRecordingOngoing ? qsTr("Stop recording") : qsTr("Record raw data")
                        onClicked: ensembleInfo.startStopRecording()
                        implicitWidth: Math.max(fontMetrics.boundingRect(qsTr("Stop recording")).width,
                                                fontMetrics.boundingRect(qsTr("Record raw data")).width)  + 4 * UI.standardMargin
                    }
                    AbracaLabel {
                        id: recordingLengthLabel
                        text: ensembleInfo.recordingLength
                        visible: ensembleInfo.isRecordingOngoing
                        horizontalAlignment: Text.AlignRight
                        Layout.minimumWidth: fontMetrics.boundingRect("320.0 sec").width
                    }

                    AbracaLabel {
                        id: recordingSizeLabel
                        text: ensembleInfo.recordingSize
                        visible: ensembleInfo.isRecordingOngoing
                        horizontalAlignment: Text.AlignRight
                        Layout.minimumWidth: fontMetrics.boundingRect("2000 MB").width
                    }

                    RowLayout {
                        id: recordingInfoRow
                        AbracaLabel {
                            // using empty label as placeholder
                            text: ensembleInfo.isRecordingOngoing ? qsTr("Length:") : " "
                            font.bold: true
                        }
                        LayoutItemProxy {
                            target: recordingLengthLabel
                        }
                        AbracaLabel {
                            Layout.leftMargin: 2 * UI.standardMargin
                            text: qsTr("File size:")
                            font.bold: true
                            visible: ensembleInfo.isRecordingOngoing
                        }
                        LayoutItemProxy {
                            target: recordingSizeLabel
                        }
                    }

                    RowLayout {
                        id: recordingControls
                        spacing: UI.standardMargin / 2
                        LayoutItemProxy {
                            target: timeoutSwitch
                        }
                        LayoutItemProxy {
                            target: timeoutSpinBox
                        }
                        LayoutItemProxy {
                            target: recordButton
                            Layout.leftMargin: UI.standardMargin
                        }
                    }

                    RowLayout {
                        id: ensCsvButtonsRow
                        LayoutItemProxy {
                            target: exportCSVButton
                        }
                        LayoutItemProxy {
                            target: uploadCSVButton
                        }
                    }

                    // -------------------------------------
                    GridLayout {
                        id: mobileLayout
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: UI.narrowViewWidth
                        columns: 2
                        columnSpacing: UI.standardMargin
                        rowSpacing: UI.standardMargin
                        uniformCellWidths: true
                        LayoutItemProxy {
                            target: showEnsTextButton
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                        }
                        AbracaLine {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            target: timeoutSwitch
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            target: timeoutSpinBox
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            visible: ensembleInfo.isRecordingOngoing
                            Layout.fillWidth: true
                            LayoutItemProxy {
                                Layout.fillWidth: true
                                target: recordingLengthLabel
                            }
                            LayoutItemProxy {
                                Layout.fillWidth: true
                                target: recordingSizeLabel
                            }
                        }
                        LayoutItemProxy {
                            Layout.columnSpan: ensembleInfo.isRecordingOngoing ? 1 : 2
                            target: recordButton
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        id: tabletLayout
                        Layout.fillWidth: true
                        LayoutItemProxy {
                            target: showEnsTextButton
                            Layout.alignment: Qt.AlignHCenter
                        }
                        AbracaLine {
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            Layout.alignment: Qt.AlignHCenter
                            target: recordingControls
                        }
                        LayoutItemProxy {
                            Layout.alignment: Qt.AlignHCenter
                            target: recordingInfoRow
                        }
                    }

                    RowLayout {
                        id: controlRow
                        Layout.fillWidth: true
                        LayoutItemProxy {
                            target: exportCSVButton
                        }
                        LayoutItemProxy {
                            target: uploadCSVButton
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            target: recordingInfoRow
                        }
                        LayoutItemProxy {
                            target: recordingControls
                        }
                    }

                    GridLayout {
                        id: controlGridEnsTextVisible
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 0
                        Item {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.preferredHeight: 3*UI.standardMargin
                        }
                        LayoutItemProxy {
                            Layout.alignment: Qt.AlignBottom | Qt.AlignRight
                            target: recordingInfoRow
                        }
                        // -----
                        LayoutItemProxy {
                            target: ensCsvButtonsRow
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            target: recordingControls
                        }
                    }
                    GridLayout {
                        id: controlGridEnsTextHidden
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 0
                        Item {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.preferredHeight: 3*UI.standardMargin
                        }
                        LayoutItemProxy {
                            Layout.alignment: Qt.AlignBottom | Qt.AlignRight
                            target: recordingInfoRow
                        }
                        // -----
                        LayoutItemProxy {
                            target: showEnsTextButton
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        LayoutItemProxy {
                            target: recordingControls
                        }
                    }
                }
            }
            ScrollIndicator.vertical: AbracaScrollIndicator {}
        }
        AbracaHorizontalShadow {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: parent.width
            topDownDirection: true
            shadowDistance: flickable.contentY
        }
        AbracaHorizontalShadow {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: parent.width
            topDownDirection: false
            shadowDistance: flickable.contentHeight - flickable.height - flickable.contentY
        }
        AbracaMessage {
            id: infoMessage
            Connections {
                target: ensembleInfo
                function onShowInfoMessage(text : string, type : int) {
                    infoMessage.text = text;
                    infoMessage.messageType = type;
                    infoMessage.visible = true;
                }
            }
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
            ensembleText: ensembleInfo.ensembleConfigurationText
            uploadCsvVisible: true
            uploadCsvEnabled: ensembleInfo.isCsvUploadEnabled && ensembleInfo.isRecordingVisible // recoring visible false => playing from file
        }
    }

    Loader {
        id: ensembleConfigDrawerLoader
        active: false
        asynchronous: true
        onLoaded: {
            item.open()
        }
        property string ensembleText: ""
        property int modelRow: -1

        sourceComponent: EnsembleConfigDrawer {
            ensembleText: ensembleInfo.ensembleConfigurationText
            uploadCsvVisible: true
            uploadCsvEnabled: ensembleInfo.isCsvUploadEnabled && ensembleInfo.isRecordingVisible // recoring visible false => playing from file
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
        function onExportCsvRequest() {
            ensembleInfo.saveCSV();
        }
        function onUploadCsvRequest() {
            ensembleInfo.uploadCSV();
        }
    }
    Connections {
        target: ensembleConfigDrawerLoader.item
        enabled: ensembleConfigDrawerLoader.item !== null

        function onClosed() {
            ensembleConfigDrawerLoader.active = false
        }
        function onExportCsvRequest() {
            ensembleInfo.saveCSV();
        }
        function onUploadCsvRequest() {
            ensembleInfo.uploadCSV();
        }
    }
}
