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
import QtQuick.Controls

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
                        title: qsTr("Audio decoder")
                        Layout.fillWidth: true
                        visible: settingsBackend.audioDecoderModel.currentIndex === 0  // FAAD2
                        RowLayout {
                            AbracaLabel {
                                text: qsTr("Noise level during audio drop-out:")
                                wrapMode: Text.WordWrap
                            }
                            AbracaComboBox {
                                id: dropOutLevelCombo
                                Layout.fillWidth: true
                                model: settingsBackend.audioNoiseConcealModel
                                textRole: "itemName"
                                currentIndex: settingsBackend.audioNoiseConcealModel.currentIndex
                                onActivated: {
                                    if (settingsBackend.audioNoiseConcealModel.currentIndex !== currentIndex) {
                                        settingsBackend.audioNoiseConcealModel.currentIndex = currentIndex;
                                    }

                                }
                                property int w: -1
                                Layout.preferredWidth: w > 0 ? w : implicitWidth
                                onImplicitWidthChanged: {
                                    if (implicitWidth > w) {
                                        w = implicitWidth
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
                        visible: settingsBackend.audioDecoderModel.currentIndex === 0  // FAAD2
                    }
                    AbracaGroupBox {
                        id: recGroupBox
                        title: qsTr("Audio recording")
                        Layout.fillWidth: true
                        Item {
                            id: recContainer
                            width: recGroupBox.width
                            implicitHeight: recLayout.implicitHeight
                            ColumnLayout {
                                id: recLayout
                                anchors.fill: parent
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: UI.standardMargin
                                    AbracaLabel {
                                        text: qsTr("Folder:")
                                        role: UI.LabelRole.Secondary
                                    }
                                    AbracaLabel {
                                        Layout.fillWidth: true
                                        text: settingsBackend.audioRecPath
                                        elide: Text.ElideLeft
                                    }
                                }
                                Column {
                                    id: captureOutputColumn
                                    Layout.fillWidth: true
                                    AbracaRadioButton {
                                        anchors.left: captureOutputColumn.left
                                        anchors.right: captureOutputColumn.right
                                        text: qsTr("Record encoded DAB/DAB+ stream (MP2 or AAC format)")
                                        wrapMode: Text.WordWrap
                                        elideMode: Text.ElideNone
                                        checked: settingsBackend.audioRecCaptureOutput === false
                                        onCheckedChanged:  {
                                            if (settingsBackend.audioRecCaptureOutput !== !checked) {
                                                settingsBackend.audioRecCaptureOutput = !checked;
                                            }
                                        }
                                    }
                                    AbracaRadioButton {
                                        anchors.left: captureOutputColumn.left
                                        anchors.right: captureOutputColumn.right
                                        text: qsTr("Record decoded audio (WAV format)")
                                        wrapMode: Text.WordWrap
                                        elideMode: Text.ElideNone
                                        checked: settingsBackend.audioRecCaptureOutput === true
                                        onCheckedChanged:  {
                                            if (settingsBackend.audioRecCaptureOutput !== checked) {
                                                settingsBackend.audioRecCaptureOutput = checked;
                                            }
                                        }
                                    }
                                }
                                AbracaSwitch {
                                    id: autoStopSwitch
                                    Layout.fillWidth: true
                                    text: qsTr("Do not ask to stop audio recording when service changes")
                                    wrapMode: Text.WordWrap
                                    checked: settingsBackend.audioRecAutoStop
                                    onCheckedChanged: {
                                        if (settingsBackend.audioRecAutoStop !== checked) {
                                            settingsBackend.audioRecAutoStop = checked
                                        }
                                    }
                                }
                                AbracaSwitch {
                                    text: qsTr("Record DL messages")
                                    checked: settingsBackend.audioRecDl
                                    wrapMode: Text.WordWrap
                                    onCheckedChanged: {
                                        if (settingsBackend.audioRecDl !== checked) {
                                            settingsBackend.audioRecDl = checked
                                        }
                                    }
                                }
                                AbracaSwitch {
                                    text: qsTr("Record DAB time for DL messages")
                                    wrapMode: Text.WordWrap
                                    enabled: settingsBackend.audioRecDl
                                    checked: settingsBackend.audioRecDlAbsTime
                                    onCheckedChanged: {
                                        if (settingsBackend.audioRecDlAbsTime !== checked) {
                                            settingsBackend.audioRecDlAbsTime = checked
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
                        id: exportGroupBox
                        title: qsTr("Expert settings")
                        Layout.fillWidth: true
                        Item {
                            id: exportContainer
                            width: exportGroupBox.width
                            implicitHeight: exportLayout.implicitHeight
                            GridLayout {
                                id: exportLayout
                                anchors.fill: parent
                                columns: 4
                                columnSpacing: appUI.isPortraitView ? UI.standardMargin : 50

                                AbracaLabel {
                                    text: qsTr("AAC audio decoder:")
                                }
                                AbracaComboBox {
                                    model: settingsBackend.audioDecoderModel
                                    textRole: "itemName"
                                    currentIndex: settingsBackend.audioDecoderModel.currentIndex
                                    onActivated: {
                                        if (settingsBackend.audioDecoderModel.currentIndex !== currentIndex) {
                                            settingsBackend.audioDecoderModel.currentIndex = currentIndex;
                                        }
                                    }
                                    property int w: -1
                                    Layout.preferredWidth: w > 0 ? w : implicitWidth
                                    onImplicitWidthChanged: {
                                        if (implicitWidth > w) {
                                            w = implicitWidth
                                        }
                                    }
                                }
                                AbracaButton {
                                    text: qsTr("Restart")
                                    onClicked: settingsBackend.requestRestart()
                                    visible: settingsBackend.audioDecoderChanged
                                }
                                Item {
                                    Layout.columnSpan: settingsBackend.audioDecoderChanged ? 1 : 2
                                    Layout.fillWidth: true
                                }
                                AbracaLabel {
                                    id: audioDecoderChangeLabel
                                    Layout.columnSpan: 4
                                    Layout.fillWidth: true
                                    text: qsTr("Audio decoder change will take effect after application restart.")
                                    color: "red"
                                    wrapMode: Text.WordWrap
                                    visible: settingsBackend.audioDecoderChanged
                                }
                                AbracaLabel {
                                    text: qsTr("Audio output framework:")
                                    visible: UI.isAndroid === false
                                }
                                AbracaComboBox {
                                    model: settingsBackend.audioOutputModel
                                    textRole: "itemName"
                                    currentIndex: settingsBackend.audioOutputModel.currentIndex
                                    onActivated: {
                                        if (settingsBackend.audioOutputModel.currentIndex !== currentIndex) {
                                            settingsBackend.audioOutputModel.currentIndex = currentIndex;
                                        }
                                    }
                                    property int w: -1
                                    Layout.preferredWidth: w > 0 ? w : implicitWidth
                                    onImplicitWidthChanged: {
                                        if (implicitWidth > w) {
                                            w = implicitWidth
                                        }
                                    }
                                    visible: UI.isAndroid === false
                                }
                                AbracaButton {
                                    text: qsTr("Restart")
                                    visible: settingsBackend.audioOutputChanged
                                    onClicked: settingsBackend.requestRestart()
                                }
                                Item {
                                    Layout.columnSpan: settingsBackend.audioOutputChanged ? 1 : 2
                                    Layout.fillWidth: true
                                }
                                AbracaLabel {
                                    Layout.columnSpan: 4
                                    Layout.fillWidth: true
                                    text: qsTr("Audio output change will take effect after application restart.")
                                    color: "red"
                                    wrapMode: Text.WordWrap
                                    visible: settingsBackend.audioOutputChanged
                                }
                                Item {
                                    Layout.columnSpan: 4
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: UI.isAndroid ? UI.controlHeight : 0
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
