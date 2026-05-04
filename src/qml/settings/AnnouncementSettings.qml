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
                        id: alarmGroupBox
                        title: qsTr("Alarm announcements")
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            RowLayout {
                                id: alarmLayout
                                Layout.fillWidth: true
                                uniformCellSizes: true
                                Repeater {
                                    model: AnnouncementsProxyModel {
                                        id: announcementModelAlarms
                                        sourceModel: settingsBackend.announcementModel
                                        alarm: true
                                    }
                                    delegate: AbracaSwitch {
                                        required property int index
                                        required property string itemName
                                        required property bool itemData
                                        Layout.fillWidth: true
                                        text: itemName
                                        checked: itemData
                                        enabled: index !== 0
                                        onCheckedChanged: {
                                            announcementModelAlarms.setChecked(index, checked)
                                        }
                                    }
                                }
                            }
                            AbracaLabel {
                                Layout.fillWidth: true
                                text: qsTr("<br>Note: Alarm announcement cannot be disabled.")
                            }
                            AbracaSwitch {
                                Layout.fillWidth: true
                                visible: UI.isDesktop
                                text: qsTr("Bring window to foreground on alarm announcement")
                                elideMode: Text.ElideNone
                                wrapMode: Text.WordWrap
                                checked: settingsBackend.annBringToForeground
                                onCheckedChanged: {
                                    if (settingsBackend.annBringToForeground !== checked) {
                                        settingsBackend.annBringToForeground = checked;
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
                        title: qsTr("Regular announcements")
                        Layout.fillWidth: true
                        GridLayout {
                            anchors.fill: parent
                            columns: 2
                            uniformCellWidths: true
                            Repeater {
                                model: AnnouncementsProxyModel {
                                    id: announcementModel
                                    sourceModel: settingsBackend.announcementModel
                                    alarm: false
                                }
                                delegate: AbracaSwitch {
                                    required property int index
                                    required property string itemName
                                    required property bool itemData
                                    Layout.fillWidth: true
                                    text: itemName
                                    checked: itemData
                                    onCheckedChanged: {
                                        announcementModel.setChecked(index, checked)
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
        shadowDistance: contentFlickable.contentY - contentFlickable.originY
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: contentItem.width
        topDownDirection: false
        shadowDistance: contentFlickable.contentHeight - contentFlickable.height - (contentFlickable.contentY - contentFlickable.originY)
    }
}
