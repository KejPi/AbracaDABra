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
import QtQuick.Controls.Basic

import abracaComponents

Item {
    id: mainItem
    anchors.fill: parent
    anchors.margins: UI.standardMargin

    ColumnLayout {
        anchors.fill: parent
        spacing: UI.standardMargin
        ServicePanel {
            id: servicePanel
            showDockingControls: false
            visible: UI.isMobile === false && appUI.isPortraitView === false
            Layout.fillWidth: true
        }
        ListView {
            id: scheduleList
            model: audioRecording.scheduleModel
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            highlightMoveDuration: 200
            highlightFollowsCurrentItem: true
            currentIndex: audioRecording.scheduleModel.currentIndex

            delegate: AbracaItemDelegate {
                id: itemDelegate
                //height: itemHeight
                highlighted: ListView.isCurrentItem
                width: scheduleList.width
                height: contentItem.implicitHeight + 2*UI.standardMargin
                text: label
                onClicked: if (audioRecording.scheduleModel.currentIndex !== index) {
                               audioRecording.scheduleModel.currentIndex = index
                           }

                contentItem: ColumnLayout {
                    required property int index
                    required property string label
                    required property string startTime
                    required property string endTime
                    required property string duration
                    required property string service
                    required property string stateIcon

                    anchors.fill: parent
                    anchors.margins: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Image {
                            visible: stateIcon !== ""
                            source: stateIcon !== "" ? UI.imagesUrl + stateIcon : ""
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            sourceSize.width: 20
                            sourceSize.height: 20
                            fillMode: Image.PreserveAspectFit
                        }
                        AbracaLabel {
                            text: label
                            font.bold: true                            
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            color: itemDelegate.highlighted ? UI.colors.listItemSelected : UI.colors.textPrimary
                        }
                    }
                    AbracaLabel {
                        text: startTime + " - " + endTime + " (" + duration + ")"
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.fillWidth: true
                    }
                    AbracaLabel {
                        text: service
                        enabled: false
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignVCenter
                        Layout.fillWidth: true
                    }
                }
            }
            AbracaLabel {
                anchors.centerIn: parent
                text: qsTr("No scheduled recordings.\n\nClick 'Add' to create a new recording.")
                horizontalAlignment: Text.AlignHCenter
                visible: audioRecording.scheduleModel.rowCount === 0
            }
            ScrollIndicator.vertical: AbracaScrollIndicator {}
            AbracaHorizontalShadow {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: parent.width
                topDownDirection: true
                shadowDistance: scheduleList.contentY - scheduleList.originY
            }
            AbracaHorizontalShadow {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: parent.width
                topDownDirection: false
                shadowDistance: scheduleList.contentHeight - scheduleList.height - (scheduleList.contentY - scheduleList.originY)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: UI.standardMargin
            AbracaButton {
                text: qsTr("Add")
                buttonRole: UI.ButtonRole.Primary
                onClicked: audioRecording.requestItemDialog(false)
            }
            AbracaButton {
                text: qsTr("Edit")
                enabled: scheduleList.currentIndex >= 0 // audioRecording.tableSelectionModel.hasSelection
                onClicked: audioRecording.requestItemDialog(true)
            }
            AbracaButton {
                text: qsTr("Delete")
                enabled: scheduleList.currentIndex >= 0 // audioRecording.tableSelectionModel.hasSelection
                onClicked: {
                    audioRecording.removeItem()
                }

            }
            Item { Layout.fillWidth: true}
            AbracaButton {
                text: qsTr("Delete all")
                enabled: audioRecording.scheduleModel.rowCount > 0
                onClicked: {
                    audioRecording.deleteAll()
                }
            }
        }
    }
}
