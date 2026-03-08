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
import abracaComponents

Item {
    id: mainItem

    property bool showDockingControls: false
    property bool pageIsUndocked: false
    readonly property int marginSize: UI.standardMargin

    signal controlClicked()

    implicitHeight: 32 + 2 * marginSize
    implicitWidth: mainLayout.implicitWidth

    GridLayout {
        id: mainLayout
        rows: 2
        flow: GridLayout.TopToBottom
        anchors.verticalCenter: mainItem.verticalCenter
        anchors.left: mainItem.left
        anchors.right: mainItem.right
        anchors.leftMargin: mainItem.marginSize
        anchors.rightMargin: mainItem.marginSize + (mainItem.showDockingControls ? dockingControlButton.width : 0) // extra space for docking control button
        rowSpacing: 2 // mainItem.showDockingControls ? 4 : 0
        columnSpacing: mainItem.marginSize

        Image {
            id: serviceLogoImage
            Layout.rowSpan: 2
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
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

        AbracaLabel {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            verticalAlignment: Text.AlignTop
            text: appUI.serviceLabel
            toolTipText: appUI.serviceLabelToolTip
            font.bold: true
        }
        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: appUI.serviceInstance
            AbracaLabel {
                verticalAlignment: Text.AlignBottom
                text: appUI.dlService
                elide: Text.ElideRight
            }
            AbracaLabel {
                verticalAlignment: Text.AlignBottom
                text: appUI.dlAnnouncement
                elide: Text.ElideRight
            }
        }

        AbracaLabel {
            Layout.alignment: Qt.AlignRight | Qt.AlignTop
            text: appUI.ensembleLabel
            role: UI.LabelRole.Secondary
        }
        AbracaLabel {
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            text: appUI.channelIndex >= 0 ? channelListModel.data(channelListModel.index(appUI.channelIndex, 0), Qt.DisplayRole) : ""
            role: UI.LabelRole.Secondary
        }
    }
    AbracaImgButton {
        id: dockingControlButton
        anchors.verticalCenter: mainItem.verticalCenter
        anchors.right: mainItem.right
        visible: showDockingControls
        onClicked: controlClicked()
        source: pageIsUndocked ? UI.imagesUrl +  "icon-dock.svg" : UI.imagesUrl + "icon-undock.svg"
        iconSize: UI.controlHeight
        colorizationColor: UI.colors.iconInactive
        toolTipText: pageIsUndocked ? qsTr("Dock page back to main window") : qsTr("Undock page to separate window")
    }
}

