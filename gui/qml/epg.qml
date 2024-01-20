/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
import ProgrammeGuide
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5

Item {
    anchors.fill: parent
    Loader {
        anchors.fill: parent
        active: epgDialog.isVisible
        visible: active
        sourceComponent: mainComponent
    }

    Component {
        id: mainComponent
        Item {
            id: mainItemId
            anchors.fill: parent

            property double pointsPerSecond: 1.0/10   // 10 sec/point
            property int currentTimeSec: epgTime.secSinceEpoch
            property int lineHeight: 50
            property int serviceListWidth: 200
            property int selectedServiceIndex: slSelectionModel.currentIndex.row
            property var selectedEpgItemIndex: epgDialog.selectedEpgItem

            property string serviceName: ""
            property string serviceStartTime: ""
            property string serviceDetail: ""

            //SystemPalette { id: sysPaletteActive; colorGroup: SystemPalette.Active }

            Item {
                id: controlsItem
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: emptyFilterSwitch.implicitHeight
                RowLayout {
                    spacing: 20
                    Switch {
                        id: emptyFilterSwitch
                        text: qsTr("Hide services without programme")
                        checked: epgDialog.filterEmptyEpg
                        onToggled: {
                            epgDialog.filterEmptyEpg = checked;
                        }
                    }
                    Switch {
                        id: ensembleFilterSwitch
                        text: qsTr("Hide services from other ensembles")
                        checked: epgDialog.filterEnsemble
                        onToggled: {
                            epgDialog.filterEnsemble = checked;
                        }
                    }
                }
            }
            TabBar {
                id: dayTabBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: controlsItem.bottom
                anchors.topMargin: 10
                visible: metadataManager.epgDatesList.length > 0
                height: metadataManager.epgDatesList.length > 0 ? implicitHeight : 0
                currentIndex: stackLayoutId.currentIndex
                Repeater {
                    model: metadataManager.epgDatesList
                    TabButton {
                        text: epgTime.currentDateString === modelData ? qsTr("Today") : modelData
                        //width: Math.max(100, bar.width / metadataManager.epgDatesList.length)
                        Component.onCompleted: {
                            if (epgTime.currentDateString === modelData) {
                                dayTabBar.currentIndex = index;
                            }
                        }
                    }
                }
            }
            Item {
                id: epgViewItem
                anchors.top: dayTabBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: progDetails.top
                anchors.topMargin: 10
                anchors.bottomMargin: 20
                Text {
                    id: currentTimeText
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 3
                    width: implicitWidth
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Now: ") + epgTime.currentTimeString
                }

                Flickable {
                    id: timelinebox
                    height: 20
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: serviceListWidth
                    contentHeight: timeline.height
                    contentWidth: timeline.width
                    //contentX: epgTable.contentX
                    boundsBehavior: Flickable.StopAtBounds
                    //boundsBehavior: Flickable.DragAndOvershootBounds
                    flickableDirection: Flickable.HorizontalFlick
                    //clip: true
                    Item {
                        id: timeline
                        height: 20
                        width: 24*3600*pointsPerSecond
                        Row {
                            Repeater {
                                model: 24
                                delegate: Row {
                                    Text {
                                        id: tst
                                        text: modelData + ":00"
                                        width: 60 * 30 * pointsPerSecond
                                        height: 20
                                        horizontalAlignment: Text.AlignLeft
                                        opacity: 1 - Math.min(timelinebox.contentX  - modelData*3600*pointsPerSecond, 50)/50
                                    }
                                    Text {
                                        text: modelData + ":30"
                                        width: 60 * 30 * pointsPerSecond
                                        height: 20
                                        horizontalAlignment: Text.AlignLeft
                                        opacity: 1 - Math.min(timelinebox.contentX  - (modelData+0.5)*3600*pointsPerSecond, 50)/50
                                    }
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.top: timelinebox.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    color: "transparent"
                    border.color: "lightgray"
                    border.width: 1
                    Flickable {
                        id: mainView
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        anchors.fill: parent
                        anchors.margins: 2
                        contentHeight: epgItem.height
                        contentWidth: epgItem.width
                        clip: true
                        Rectangle {
                            color: "transparent"
                            border.color: "darkgray"
                            border.width: 2
                            id: epgItem
                            height: serviceColumnId.height
                            width: epgViewItem.width
                            Column {
                                id: serviceColumnId
                                Repeater {
                                    id: serviceList
                                    model: slProxyModel
                                    delegate: Rectangle {
                                        //property bool isSelected: false
                                        color: selectedServiceIndex == index ? "white" : "transparent"
                                        border.color: selectedServiceIndex == index ? "black" : "darkgray"
                                        border.width: selectedServiceIndex == index ? 3 : 1
                                        height: lineHeight
                                        width: serviceListWidth
                                        Row {
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            spacing: 10
                                            Image {
                                                id: logoId
                                                anchors.verticalCenter: parent.verticalCenter
                                                source: "image://metadata/"  + smallLogoId
                                                cache: false
                                            }
                                            Text {
                                                id: labelId
                                                text: display
                                                anchors.verticalCenter: parent.verticalCenter
                                                verticalAlignment: Text.AlignVCenter
                                                horizontalAlignment: Text.AlignLeft
                                            }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                slSelectionModel.select(slProxyModel.index(index, 0), ItemSelectionModel.Select | ItemSelectionModel.Current)
                                                slSelectionModel.setCurrentIndex(slProxyModel.index(index, 0), ItemSelectionModel.Select | ItemSelectionModel.Current)
                                            }
                                        }
                                    }
                                }
                            }

                            StackLayout {
                                id: stackLayoutId
                                anchors.left: serviceColumnId.right
                                height: serviceColumnId.height
                                width: mainView.width - serviceColumnId.width
                                currentIndex: dayTabBar.currentIndex
                                clip: true

                                Repeater {
                                    id: dateRepeater
                                    model: metadataManager.epgDatesList
                                    Loader {
                                        id: dayLoader
                                        active: dayTabBar.currentIndex == index
                                        sourceComponent: Flickable {
                                            id: epgTable
                                            property int dateIndex: index
                                            contentWidth: colId.width
                                            contentHeight: colId.height
                                            boundsBehavior: Flickable.StopAtBounds
                                            flickableDirection: Flickable.HorizontalFlick
                                            contentX: timelinebox.contentX
                                            clip: true
                                            Column {
                                                id: colId
                                                Repeater {
                                                    model: slProxyModel
                                                    Item {
                                                        height: lineHeight
                                                        anchors.left: parent.left
                                                        width: 24*3600 * pointsPerSecond

                                                        Rectangle {
                                                            id: epgForService
                                                            property bool isCurrentService: selectedServiceIndex == index

                                                            color: "transparent"
                                                            border.color: isCurrentService ? "black" : "darkgray"
                                                            border.width: 1
                                                            anchors.fill: parent
                                                            clip: true

                                                            EPGProxyModel {
                                                                id: proxyModel
                                                                sourceModel: epgModelRole
                                                                dateFilter: metadataManager.epgDate(epgTable.dateIndex)
                                                            }

                                                            Text {
                                                                anchors.top: parent.top
                                                                anchors.bottom: parent.bottom
                                                                verticalAlignment: Text.AlignVCenter
                                                                x: epgTable.contentX + 5
                                                                text: qsTr("No data available")
                                                                visible: epgItemRepeater.count == 0
                                                            }
                                                            Repeater {
                                                                id: epgItemRepeater
                                                                model: proxyModel
                                                                //property var modelIndex: proxyModel.mapToSource(proxyModel.index(index, 0))
                                                                delegate: EPGItem {
                                                                    itemHeight: lineHeight
                                                                    pointsPerSec: pointsPerSecond
                                                                    viewX: epgTable.contentX
                                                                    isSelected: selectedEpgItemIndex.valid
                                                                                && (selectedEpgItemIndex.model === proxyModel.mapToSource(proxyModel.index(index, 0)).model)
                                                                                && (selectedEpgItemIndex.row === proxyModel.mapToSource(proxyModel.index(index, 0)).row)
                                                                    onClicked: {
                                                                        //console.log("clicked")
                                                                        serviceDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                        serviceName = (longName !== "") ? longName : mediumName;
                                                                        serviceStartTime = startTimeString;

                                                                        epgDialog.selectedEpgItem = proxyModel.mapToSource(proxyModel.index(index, 0));
                                                                    }
                                                                    Component.onCompleted: {
                                                                        if (selectedEpgItemIndex.valid) {
                                                                            if ((selectedEpgItemIndex.model === proxyModel.mapToSource(proxyModel.index(index, 0)).model)
                                                                                    && (selectedEpgItemIndex.row === proxyModel.mapToSource(proxyModel.index(index, 0)).row))
                                                                            {
                                                                                serviceDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                                serviceName = (longName !== "") ? longName : mediumName;
                                                                                serviceStartTime = startTimeString;
                                                                            }
                                                                        }
                                                                        else {
                                                                            if (epgForService.isCurrentService) {
                                                                                if ((startTimeSecSinceEpoch <= currentTimeSec) && (endTimeSecSinceEpoch > currentTimeSec))
                                                                                {   // if it is ongoing programme select it
                                                                                    epgDialog.selectedEpgItem = proxyModel.mapToSource(proxyModel.index(index, 0));
                                                                                    serviceDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                                    serviceName = (longName !== "") ? longName : mediumName;
                                                                                    serviceStartTime = startTimeString;
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                        Rectangle {
                                                            visible: selectedServiceIndex == index
                                                            anchors.top: epgForService.top
                                                            anchors.left: epgForService.left
                                                            height: 3
                                                            width: epgForService.width
                                                            color: "black"
                                                        }
                                                        Rectangle {
                                                            visible: selectedServiceIndex == index
                                                            anchors.bottom: epgForService.bottom
                                                            anchors.left: epgForService.left
                                                            height: 3
                                                            width: epgForService.width
                                                            color: "black"
                                                        }
                                                    }
                                                }
                                            }
                                            Component.onCompleted: {
                                                if (epgTime.isCurrentDate(metadataManager.epgDate(epgTable.dateIndex))) {
                                                    //var cX = (Math.round(epgTime.secSinceMidnight() / 3600) - 0.5) * 3600 * pointsPerSecond;
                                                    const w = mainItemId.width - serviceListWidth;
                                                    const cX = Math.max(epgTime.secSinceMidnight() * pointsPerSecond - w/3, 0);
                                                    timelinebox.contentX = Math.min(cX, (3600*24) * pointsPerSecond - w);
                                                    //console.log(cX, (3600*24) * pointsPerSecond-timelinebox.width, timelinebox.width, mainItemId.width)
                                                }
                                                else {
                                                    timelinebox.contentX = 0;
                                                }
                                            }
                                            onContentXChanged: {
                                                //console.log(timelinebox.contentX)
                                                if (timelinebox.contentX != contentX) {
                                                    timelinebox.contentX = contentX;
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
            Rectangle {
                id: progDetails
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                color: "transparent"
                height: 100
                ColumnLayout {
                    anchors.fill: parent
                    Text {
                        Layout.fillWidth: true
                        text: serviceName
                        font.bold: true
                        font.pointSize: 20
                    }
                    Text {
                        visible: serviceStartTime
                        Layout.fillWidth: true
                        text: qsTr("Starts at ") + serviceStartTime
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: serviceDetail
                        wrapMode: Text.WordWrap
                        clip: true
                    }
                }
            }
        }
    }
}
