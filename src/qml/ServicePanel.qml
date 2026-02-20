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

import abracaComponents 1.0

Item {
    id: mainItem
    // color: "red"

    readonly property int marginSize: UI.standardMargin
    property bool showDockingControls: false
    property bool pageIsUndocked: false
    signal controlClicked()

    implicitHeight: servicePanelLayout.implicitHeight + 2 * marginSize
    implicitWidth: mainLayout.implicitWidth

    RowLayout {
        id: mainLayout
        anchors.fill: parent
        // anchors.leftMargin: marginSize
        anchors.rightMargin: mainItem.marginSize
        RowLayout {
            id: servicePanelLayout
            Layout.fillWidth: true
            Layout.margins: mainItem.marginSize
            // Layout.preferredHeight: 32
            Image {
                id: serviceLogoImage
                Layout.preferredWidth: 32
                Layout.preferredHeight: Layout.preferredWidth
                visible: appUI.isServiceLogoVisible
                source: appUI.isServiceLogoVisible ? "image://metadata/logo/" + appUI.ensembleId + "/" + appUI.serviceId : ""
                cache: false
                // onVisibleChanged: {
                //     if (visible) {
                //         // Force reload of the logo when it becomes visible, to reflect possible changes
                //         serviceLogoImage.source = ""
                //         serviceLogoImage.source = "image://metadata/logo/" + appUI.ensembleId + "/" + appUI.serviceId
                //     }
                // }
            }
            ColumnLayout {
                Layout.fillWidth: true
                AbracaLabel {
                    text: appUI.serviceLabel
                    toolTipText: appUI.serviceLabelToolTip
                    font.bold: true
                }
                StackLayout {
                    Layout.fillWidth: true
                    currentIndex: appUI.serviceInstance
                    AbracaLabel {
                        Layout.fillWidth: true
                        text: appUI.dlService
                        elide: Text.ElideRight
                    }
                    AbracaLabel {
                        Layout.fillWidth: true
                        text: appUI.dlAnnouncement
                        elide: Text.ElideRight
                    }
                }

            }
        }

        ColumnLayout {
            AbracaLabel {
                text: appUI.ensembleLabel
                Layout.alignment: Qt.AlignRight
                role: UI.LabelRole.Secondary
            }
            AbracaLabel {
                text: appUI.channelIndex >= 0 ? channelListModel.data(channelListModel.index(appUI.channelIndex, 0), Qt.DisplayRole) : ""
                Layout.alignment: Qt.AlignRight
                role: UI.LabelRole.Secondary
            }
        }
        AbracaImgButton {
            visible: showDockingControls
            onClicked: controlClicked()
            source: pageIsUndocked ? UI.imagesUrl +  "icon-dock.svg" : UI.imagesUrl + "icon-undock.svg"
            iconSize: UI.controlHeight
            colorizationColor: UI.colors.iconInactive
            toolTipText: pageIsUndocked ? qsTr("Dock page back to main window") : qsTr("Undock page to separate window")
        }

    }
}

