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
                implicitWidth: Math.min(contentLayout.implicitWidth, contentFlickable.width) + 2*UI.standardMargin
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
                        id: dbGroupBox
                        title: qsTr("Transmitter database")
                        Layout.fillWidth: true
                        Item {
                            width: dbGroupBox.width
                            implicitHeight: dbUpdateLayout.implicitHeight
                            ColumnLayout {
                                id: dbUpdateLayout
                                anchors.fill: parent
                                spacing: UI.standardMargin
                                AbracaLabel {
                                    id: fmlistInfoLabel
                                    Layout.fillWidth: true
                                    text: qsTr("Application uses DAB transmiter database provided by <a href=\"https://www.fmlist.org/\">FMLIST</a>.<br>By pressing <i>Update</i> button you agree with data usage <a href='https://www.fmlist.org/fmlist_copyright_disclaimer_legal_notice.php'>terms and conditions</a>.")
                                    textFormat: Text.StyledText
                                    wrapMode: Text.WordWrap
                                    role: UI.LabelRole.Secondary
                                }                            
                                RowLayout {
                                    AbracaLabel {
                                        Layout.fillWidth: true
                                        text: settingsBackend.tiiDbLabel
                                    }
                                    AbracaBusyIndicator {
                                        id: dbUpdateSpinner
                                        Layout.preferredHeight: updateDbButton.height * 0.75
                                        Layout.preferredWidth: updateDbButton.height * 0.75
                                        visible: settingsBackend.isTiiDbUpdating
                                        running: settingsBackend.isTiiDbUpdating
                                    }
                                    AbracaButton {
                                        id: updateDbButton
                                        text: qsTr("Update")
                                        enabled: settingsBackend.isTiiUpdateEnabled
                                        onClicked: settingsBackend.requestTiiDbUpdate()
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
                        id: receiverLocationGroupBox
                        title: qsTr("Receiver location")
                        Layout.fillWidth: true
                        Item {
                            width: receiverLocationGroupBox.width
                            implicitHeight: receiverLocationLayout.implicitHeight
                            ColumnLayout {
                                id: receiverLocationLayout
                                anchors.fill: parent
                                readonly property int labelWidth: Math.max(geoLocationLabel.implicitWidth, coordinatesLabel.implicitWidth)
                                spacing: UI.standardMargin
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: receiverLocationLayout.labelWidth
                                    spacing: UI.standardMargin
                                    AbracaLabel {
                                        id: geoLocationLabel
                                        text: qsTr("Geolocation source:")
                                    }
                                    AbracaComboBox {
                                        model: settingsBackend.locationSourceModel
                                        textRole: "itemName"
                                        currentIndex: settingsBackend.locationSourceModel.currentIndex
                                        onActivated: {
                                            if (settingsBackend.locationSourceModel.currentIndex !== currentIndex) {
                                                settingsBackend.locationSourceModel.currentIndex = currentIndex;
                                            }
                                        }
                                    }
                                }
                                StackLayout {
                                    currentIndex: settingsBackend.locationSourceModel.currentIndex === 2 ? 1 : 0
                                    Layout.fillWidth: true
                                    RowLayout {
                                        id: coordinatesRow
                                        Layout.fillWidth: true
                                        spacing: UI.standardMargin
                                        enabled: settingsBackend.locationSourceModel.currentIndex === 1
                                        AbracaLabel {
                                            id: coordinatesLabel
                                            Layout.preferredWidth: receiverLocationLayout.labelWidth
                                            text: qsTr("GPS coordinates:")
                                        }
                                        AbracaTextField {
                                            id: coordinatesTextField
                                            Layout.fillWidth: true
                                            readonly property regexp coordRegExp: /^\s*[+-]?\d+(?:\.\d+)?\s*,\s*[+-]?\d+(?:\.\d+)?\s*$/
                                            validator: RegularExpressionValidator { regularExpression: coordinatesTextField.coordRegExp }
                                            text: settingsBackend.locationCoordinates
                                            onEditingFinished: {
                                                var txt = text.trim();
                                                if (settingsBackend.locationCoordinates !== txt) {
                                                    settingsBackend.locationCoordinates = txt
                                                }
                                            }
                                        }
                                    }
                                    GridLayout {
                                        id: serialPortLayout
                                        Layout.fillWidth: true
                                        columnSpacing: UI.standardMargin
                                        rowSpacing: UI.standardMargin
                                        columns: mainItem.width < UI.narrowViewWidth ? 2 : 4
                                        AbracaLabel {
                                            Layout.preferredWidth: receiverLocationLayout.labelWidth
                                            text: qsTr("Serial port:")
                                        }
                                        AbracaTextField {
                                            Layout.fillWidth: true
                                            text: settingsBackend.tiiSerialPort
                                            onEditingFinished: {
                                                var txt = text.trim();
                                                if (settingsBackend.tiiSerialPort !== txt) {
                                                    settingsBackend.tiiSerialPort = txt
                                                }
                                            }
                                        }
                                        AbracaLabel {
                                            text: qsTr("Baudrate:")
                                        }
                                        AbracaComboBox {
                                            model: settingsBackend.serialPortBaudrateModel
                                            textRole: "itemName"
                                            currentIndex: settingsBackend.serialPortBaudrateModel.currentIndex
                                            onActivated: {
                                                if (settingsBackend.serialPortBaudrateModel.currentIndex !== currentIndex) {
                                                    settingsBackend.serialPortBaudrateModel.currentIndex = currentIndex;
                                                }
                                            }
                                        }
                                    }
                                }
                                AbracaLabel {
                                    id: coordinatesHelpLabel
                                    Layout.fillWidth: true
                                    enabled: settingsBackend.locationSourceModel.currentIndex === 1
                                    visible: settingsBackend.locationSourceModel.currentIndex !== 2
                                    textFormat: Text.StyledText
                                    text: settingsBackend.coordinatesHelp
                                    wrapMode: Text.WordWrap
                                    role: UI.LabelRole.Secondary
                                }
                            }
                        }
                    }
                    AbracaLine {
                        Layout.fillWidth: true
                        isVertical: false
                        // Layout.topMargin: UI.standardMargin
                        Layout.bottomMargin: 2*UI.standardMargin
                    }
                    AbracaGroupBox {
                        id: loggingGroupBox
                        title: qsTr("Logging")
                        Layout.fillWidth: true
                        Layout.topMargin: UI.standardMargin
                        Item {
                            width: loggingGroupBox.width
                            implicitHeight: loggingLayout.implicitHeight
                            ColumnLayout {
                                id: loggingLayout
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
                                        text: settingsBackend.tiiLogPath
                                        elide: Text.ElideLeft
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: UI.standardMargin
                                    uniformCellSizes: true
                                    AbracaSwitch {
                                        Layout.fillWidth: true
                                        text: qsTr("Timestamp in UTC")
                                        wrapMode: Text.WordWrap
                                        checked: settingsBackend.tiiLogUtcTimestamp
                                        onCheckedChanged: {
                                            if (settingsBackend.tiiLogUtcTimestamp !== checked) {
                                                settingsBackend.tiiLogUtcTimestamp = checked
                                            }
                                        }
                                    }
                                    AbracaSwitch {
                                        Layout.fillWidth: true
                                        text: qsTr("GPS coordinates")
                                        wrapMode: Text.WordWrap
                                        checked: settingsBackend.tiiLogCoordinates
                                        onCheckedChanged: {
                                            if (settingsBackend.tiiLogCoordinates !== checked) {
                                                settingsBackend.tiiLogCoordinates = checked
                                            }
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
                        title: qsTr("Detector")
                        Layout.fillWidth: true
                        RowLayout {
                            anchors.fill: parent
                            spacing: appUI.isPortraitView ? UI.standardMargin : 30
                            AbracaLabel {
                                text: qsTr("Reliable")
                            }
                            AbracaSlider {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 1
                                snapMode: Slider.SnapOnRelease
                                value: settingsBackend.tiiModeValue
                                onValueChanged: {
                                    if (settingsBackend.tiiModeValue !== value) {
                                        settingsBackend.tiiModeValue = value
                                    }
                                }
                            }
                            AbracaLabel {
                                text: qsTr("Sensitive")
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
                            implicitHeight: uiLayout.implicitHeight
                            ColumnLayout {
                                id: uiLayout
                                anchors.fill: parent
                                spacing: UI.standardMargin


                                AbracaSwitch {
                                    Layout.fillWidth: true
                                    text: qsTr("Show spectrum plot")
                                    wrapMode: Text.WordWrap
                                    checked: settingsBackend.tiiShowSpectrumPlot
                                    onCheckedChanged: {
                                        if (settingsBackend.tiiShowSpectrumPlot !== checked) {
                                            settingsBackend.tiiShowSpectrumPlot = checked
                                        }
                                    }
                                }
                                AbracaSwitch {
                                    id: showInactiveSwitch
                                    Layout.fillWidth: true
                                    text: qsTr("Keep no longer detected transmitters on map (grey marker)")
                                    checked: settingsBackend.tiiShowInactive
                                    onCheckedChanged: {
                                        if (settingsBackend.tiiShowInactive !== checked) {
                                            settingsBackend.tiiShowInactive = checked
                                        }
                                    }
                                    elideMode: Text.ElideNone
                                    wrapMode: Text.WordWrap
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    enabled: settingsBackend.tiiShowInactive
                                    Item {
                                        Layout.preferredWidth: 4 * UI.standardMargin
                                    }
                                    AbracaSwitch {
                                        id: timeoutSwitch
                                        text: qsTr("Remove after:")
                                        wrapMode: Text.WordWrap
                                        checked: settingsBackend.isTiiInactiveTimeoutEnabled
                                        onCheckedChanged: {
                                            if (settingsBackend.isTiiInactiveTimeoutEnabled !== checked) {
                                                settingsBackend.isTiiInactiveTimeoutEnabled = checked
                                            }
                                        }
                                    }
                                    AbracaSpinBox {
                                        enabled: settingsBackend.isTiiInactiveTimeoutEnabled
                                        value: settingsBackend.tiiInactiveTimeout
                                        stepSize: 1
                                        from: 1
                                        to: 600
                                        editable: true
                                        suffix: " min"
                                        onValueChanged: if (value !== settingsBackend.tiiInactiveTimeout) {
                                            settingsBackend.tiiInactiveTimeout = value;
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
                        id: txTableGroupBox
                        title: qsTr("Transmitter table columns")
                        Layout.fillWidth: true
                        Item {
                            width: txTableGroupBox.width
                            implicitHeight: tiiTableColsView.implicitHeight

                            ListView {
                                id: tiiTableColsView
                                // anchors.fill: parent
                                anchors.top: parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(parent.width, 430)

                                implicitHeight: 6*UI.controlHeight + 5*4  // 6 items + 5 spacings
                                model: settingsBackend.tiiTableColsModel
                                spacing: 4
                                clip: true
                                interactive: false

                                property int dragFromIndex: -1

                                displaced: Transition {
                                    NumberAnimation { properties: "y"; duration: 150; easing.type: Easing.OutQuad }
                                }

                                delegate: DropArea {
                                    id: dropArea
                                    width: tiiTableColsView.width
                                    height: UI.controlHeight

                                    required property int index
                                    required property string name
                                    required property bool enabled
                                    required property bool editable

                                    onEntered: (drag) => {
                                        let from = drag.source.visualIndex
                                        let to = dropArea.index
                                        if (from !== to) {
                                            settingsBackend.tiiTableColsModel.move(from, to)
                                        }
                                    }

                                    Rectangle {
                                        id: content
                                        width: dropArea.width
                                        height: dropArea.height
                                        radius: UI.controlRadius
                                        color: mouseArea.held ? UI.colors.highlight : UI.colors.backgroundLight
                                        border.color: mouseArea.held || mouseArea.containsMouse ? UI.colors.accent : UI.colors.controlBorder
                                        border.width: 1
                                        //opacity: dropArea.enabled ? 1.0 : 0.5
                                        //layer.enabled: !dropArea.enabled

                                        // Track which model index this delegate currently represents
                                        property int visualIndex: dropArea.index

                                        Drag.active: mouseArea.held
                                        Drag.source: content
                                        Drag.hotSpot.x: width / 2
                                        Drag.hotSpot.y: height / 2

                                        MouseArea {
                                            id: mouseArea
                                            anchors.fill: parent
                                            // Only left 40px acts as drag handle; rest passes through for checkbox
                                            //anchors.rightMargin: parent.width - 40

                                            property bool held: drag.active
                                            hoverEnabled: true
                                            //acceptedButtons: Qt.NoButton

                                            drag.target: content
                                            drag.axis: Drag.YAxis

                                            onPressed: event=> {
                                                content.z = 10
                                            }
                                            onReleased: {
                                                content.z = 0
                                            }
                                        }

                                        states: State {
                                            when: mouseArea.held
                                            ParentChange {
                                                target: content
                                                parent: tiiTableColsView
                                            }
                                            AnchorChanges {
                                                target: content
                                                anchors.horizontalCenter: undefined
                                                anchors.verticalCenter: undefined
                                            }
                                        }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 8

                                            // Drag handle
                                            AbracaColorizedImage {
                                                width: UI.iconSizel
                                                height: UI.iconSize
                                                source: UI.imagesUrl + "icon-drag.svg"
                                                cache: true
                                                colorizationEnabled: true
                                                colorizationColor: UI.colors.textPrimary
                                            }
                                            AbracaLabel {
                                                role: UI.LabelRole.Primary
                                                text: dropArea.name
                                                enabled: dropArea.enabled
                                                Layout.fillWidth: true
                                            }

                                            AbracaSwitch {
                                                id: colSwitch
                                                text: ""
                                                checked: dropArea.enabled
                                                enabled: editable
                                                onToggled: {
                                                    settingsBackend.tiiTableColsModel.setData(
                                                        settingsBackend.tiiTableColsModel.index(dropArea.index, 0),
                                                        checked, Qt.UserRole + 3  // EnabledRole
                                                    )
                                                }
                                            }
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

