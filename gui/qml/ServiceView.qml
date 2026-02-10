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

import abracaComponents 1.0

Item {
    readonly property bool isPortrait: appUI.isPortraitView
    readonly property int itemsLeftMargin: isPortrait ? UI.standardMargin : 0
    readonly property int itemsRightMargin: isPortrait ? UI.standardMargin : 0
    readonly property int itemsTopMargin: UI.standardMargin
    readonly property int itemsBottomMargin: UI.standardMargin

    // Set initial focus
    Component.onCompleted: {
        serviceList.forceActiveFocus()
    }

    RowLayout {
        id: mainLayout
        anchors.leftMargin: isPortrait ? 0 : UI.standardMargin
        anchors.rightMargin: isPortrait ? 0 : UI.standardMargin
        anchors.fill: parent
        spacing: UI.standardMargin

        Flickable {
            id: serviceMainItem
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: 320
            Layout.minimumWidth: 320
            Layout.topMargin: isPortrait ? 0 : UI.standardMargin
            Layout.bottomMargin: isPortrait ? 0 : UI.standardMargin

            Layout.horizontalStretchFactor: 1

            implicitHeight: colLayout.implicitHeight
            implicitWidth: colLayout.implicitWidth

            contentWidth: width
            contentHeight: colLayout.implicitHeight

            clip: true
            ColumnLayout {
                id: colLayout
                anchors.fill: parent
                spacing: UI.standardMargin

                AbracaLabel {
                    text: "[ "+ appUI.ensembleLabel + " ]"
                    Layout.fillWidth: true
                    //Layout.topMargin: 5
                    horizontalAlignment: Text.AlignHCenter
                    color: UI.colors.inactive
                    visible: smallLayout.visible
                }

                RowLayout {
                    id: titleRowLayout
                    Layout.fillWidth: true
                    Layout.leftMargin: itemsLeftMargin
                    Layout.rightMargin: itemsRightMargin
                    Layout.preferredHeight: UI.controlHeight
                    spacing: 0
                    Image {
                        id: serviceLogoImage
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.rightMargin: UI.standardMargin
                        visible: appUI.isServiceLogoVisible
                        onVisibleChanged: {
                            if (visible) {
                                // Force reload of the logo when it becomes visible, to reflect possible changes
                                serviceLogoImage.source = ""
                                serviceLogoImage.source = "image://metadata/logo/"  + appUI.serviceId
                            }
                        }
                        source: "image://metadata/logo/"  + appUI.serviceId
                        cache: false
                    }
                    AbracaLabel {
                        //Layout.rightMargin: UI.standardMargin
                        text: appUI.serviceLabel
                        toolTipText: appUI.serviceLabelToolTip
                        font.bold: true
                        font.pointSize: UI.largeFontPointSize
                    }

                    AbracaImgButton {
                        id: announcementIcon
                        checkable: true
                        checked: appUI.isAnnouncementActive
                        sourceChecked: UI.imagesUrl + "announcement-active.svg"
                        sourceUnchecked: UI.imagesUrl + "announcement-suspended.svg"
                        toolTipChecked: appUI.announcementButtonTooltip
                        toolTipUnchecked: appUI.announcementButtonTooltip
                        enabled: appUI.isAnnouncementButtonEnabled
                        visible: appUI.isAnnouncementOngoing
                        onClicked: application.onAnnouncementClicked()
                        Connections {
                            target: appUI
                            function isAnnouncementActiveChanged() {
                                if (announcementIcon.checked !== appUI.isAnnouncementActive) {
                                    announcementIcon.checked = appUI.isAnnouncementActive
                                }
                            }
                        }
                    }
                    Item {
                        Layout.fillWidth: true;
                    }
                    AbracaImgButton {
                        id: favoriteIcon
                        colorizationEnabled: checked === false
                        colorizationColor: UI.colors.inactive
                        checkable: true
                        checked: appUI.isServiceFavorite
                        sourceChecked: UI.imagesUrl + "star.svg"
                        sourceUnchecked: UI.imagesUrl + "starEmpty.svg"                        
                        toolTipChecked: qsTr("Remove service from favorites")
                        toolTipUnchecked: qsTr("Add service to favorites")
                        enabled: appUI.isServiceSelected
                        onToggled: application.setCurrentServiceFavorite(checked)
                        Connections {
                            target: appUI
                            function onIsServiceFavoriteChanged() {
                                if (favoriteIcon.checked !== appUI.isServiceFavorite) {
                                    favoriteIcon.checked = appUI.isServiceFavorite
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    id: infoRowLayout
                    Layout.fillWidth: true
                    Layout.leftMargin: itemsLeftMargin
                    Layout.rightMargin: itemsRightMargin
                    Layout.preferredHeight: UI.controlHeight
                    Item {
                        Layout.preferredHeight: countryFlagImage.height
                        Layout.preferredWidth: countryFlagImage.width
                        visible: appUI.isServiceFlagVisible
                        Image {
                            id: countryFlagImage
                            height: 16
                            fillMode: Image.PreserveAspectFit
                            source: "image://metadata/flag/" + appUI.serviceId
                            cache: false
                        }
                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                        AbracaToolTip {
                            text: appUI.serviceFlagToolTip
                            hoverMouseArea: hoverArea
                        }
                    }
                    AbracaLabel {
                        text: appUI.programTypeLabel
                        toolTipText: appUI.programTypeLabelToolTip
                        elide: Text.ElideRight
                    }
                    Item {
                        Layout.fillWidth: true;
                    }
                    SPIProgress {
                        id: spiProgress
                        Layout.preferredHeight: UI.iconSize
                        value: appUI.spiProgressService
                        visible: appUI.isSpiProgressServiceVisible
                        toolTipText: appUI.spiProgressServiceToolTip
                    }
                    AbracaImgButton {
                        id: switchSourceIcon
                        checkable: false
                        visible: application.serviceSourcesMenuModel.rowCount > 1
                        source: UI.imagesUrl + "icon-multi-ens.svg"
                        toolTipText: qsTr("Switch to another ensemble")
                        onClicked: contextMenu.popup(switchSourceIcon.width/2, switchSourceIcon.height)

                        AbracaContextMenu {
                            id: contextMenu
                            menuModel: application.serviceSourcesMenuModel
                        }
                    }
                }
                RowLayout {
                    id: audioParamsLayout
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 400
                    Layout.leftMargin: itemsLeftMargin
                    Layout.rightMargin: itemsRightMargin
                    // Layout.topMargin: itemsTopMargin
                    Layout.bottomMargin: itemsBottomMargin
                    spacing: UI.standardMargin/2
                    Repeater {
                        id: audioParamRepeater
                        readonly property var labels:  [appUI.audioBitrateLabel,
                                                        appUI.stereoLabel,
                                                        appUI.audioEncodingLabel,
                                                        appUI.protectionLabel]
                        readonly property var toolTips: [appUI.audioBitrateLabelToolTip,
                                                        appUI.stereoLabelToolTip,
                                                        appUI.audioEncodingLabelToolTip,
                                                        appUI.protectionLabelToolTip]
                        model: 4
                        delegate: Rectangle {                        
                            required property int index
                            color: UI.colors.highlight
                            height: UI.controlHeightSmaller
                            radius: UI.controlRadius
                            Layout.fillWidth: true
                            AbracaLabel {
                                text: audioParamRepeater.labels[index]
                                toolTipText: audioParamRepeater.toolTips[index]
                                anchors.centerIn: parent
                                font.bold: true
                                color: UI.colors.textSecondary
                            }
                        }
                    }
                }
                StackLayout {
                    id: serviceStackLayout
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: appUI.serviceInstance

                    Repeater {
                        model: 2
                        ColumnLayout {
                            id: serviceLayout
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            readonly property var sls: index === 0 ? slsService : slsAnnouncement
                            readonly property var slsPath: (index == 0 ? "image://slsService/" : "image://slsAnnouncement/") + sls.slidePath
                            readonly property string dl: index == 0 ? appUI.dlService : appUI.dlAnnouncement
                            SLSView {
                                id: slsView
                                Layout.fillWidth: true
                                Layout.preferredHeight: width / 4 * 3
                                Layout.maximumWidth: 400
                                Layout.alignment: Qt.AlignHCenter
                                backend: sls
                                slsSource: slsPath
                            }

                            AbracaLabel {
                                Layout.fillWidth: true
                                Layout.leftMargin: itemsLeftMargin
                                Layout.rightMargin: itemsRightMargin
                                Layout.topMargin: itemsTopMargin
                                Layout.bottomMargin: itemsBottomMargin
                                text: dl
                                wrapMode: Text.WordWrap
                                horizontalAlignment: implicitWidth < width ? Text.AlignHCenter : Text.AlignLeft
                                toolTipText: qsTr("Right click to copy Dynamic label")
                                onRightClick: application.copyDlToClipboard()
                            }

                            GridLayout {
                                id: serviceDlLayout
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: itemsLeftMargin
                                Layout.rightMargin: itemsRightMargin
                                Layout.topMargin: itemsTopMargin
                                Layout.bottomMargin: itemsBottomMargin

                                readonly property var dlPlusModel: index === 0 ? dlPlusModelService : dlPlusModelAnnouncement

                                rows: serviceDlLayout.dlPlusModel.rowCount
                                flow: GridLayout.TopToBottom

                                Repeater {
                                    model: serviceDlLayout.dlPlusModel
                                    delegate: AbracaLabel {
                                        Layout.alignment: Qt.AlignTop
                                        required property string tag
                                        text: tag + ":"
                                        font.bold: true
                                        toolTipText: qsTr("Right click to copy Dynamic label +")
                                        onRightClick: application.copyDlPlusToClipboard()
                                    }
                                }
                                Repeater {
                                    model: serviceDlLayout.dlPlusModel
                                    delegate: AbracaLabel {
                                        Layout.fillWidth: true

                                        required property string content
                                        required property string tooltip
                                        text: content
                                        toolTipText: tooltip.length > 0 ? tooltip : qsTr("Right click to copy Dynamic label +")
                                        wrapMode: Text.WordWrap
                                        onRightClick: application.copyDlPlusToClipboard()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        AbracaLine {
            visible: !isPortrait
            Layout.fillHeight: true
            isVertical: true
            Layout.topMargin: UI.standardMargin
            Layout.bottomMargin: UI.standardMargin
        }

        ServiceList {
            id: serviceList
            visible: !isPortrait
            Layout.minimumWidth: serviceList.minWidth
            Layout.maximumWidth: serviceList.maxWidth
            Layout.preferredWidth: serviceList.implicitWidth
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Layout.topMargin: isPortrait ? 0 : -UI.standardMargin  // negative margin is for shadow
            // Layout.bottomMargin: isPortrait ? 0 : -UI.standardMargin  // negative margin is for shadow
            Layout.horizontalStretchFactor: 1000
            onMinWidthChanged: appUI.serviceViewLandscapeWidth = minWidth + 320 + 2*mainLayout.spacing
        }
    }
}
