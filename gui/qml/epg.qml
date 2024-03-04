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
// import QtQuick.Effects              // not available in Qt < 6.5

Item {
    id: rootItem
    anchors.fill: parent

    Loader {
        anchors.fill: parent        
        active: epgDialog.isVisible
        visible: active
        sourceComponent: mainComponent
    }

    Component {
        id: mainComponent
        Rectangle {
            id: mainItemId            
            color: EPGColors.bgColor
            anchors.fill: parent

            property double pointsPerSecond: 1.0/10   // 10 sec/point
            property int currentTimeSec: epgTime.secSinceEpoch
            property int lineHeight: 50
            property int serviceListWidth: 220
            property int selectedServiceIndex: slProxyModel.mapFromSource(slSelectionModel.currentIndex).row
            property var selectedEpgItemIndex: epgDialog.selectedEpgItem

            property string programName: ""
            property string programStartToEndTime: ""
            property string programDetail: ""
            property bool scheduleRecEnable: false

            //SystemPalette { id: sysPaletteActive; colorGroup: SystemPalette.Active }

            RowLayout {
                id: controlLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 10
                anchors.topMargin: 10
                spacing: 5
                EPGTabBar {
                    id: dayTabBar
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    visible: metadataManager.epgDatesList.length > 0
                    currentIndex: stackLayoutId.currentIndex
                    clip: true
                    Repeater {
                        model: metadataManager.epgDatesList
                        EPGTabButton {
                            text: epgTime.currentDateString === modelData ? qsTr("Today") : modelData
                            implicitHeight: 30
                            width: Math.max(50, (controlLayout.width - controlsItem.width - 25) / metadataManager.epgDatesList.length)
                            Component.onCompleted: {
                                if (epgTime.currentDateString === modelData) {
                                    dayTabBar.currentIndex = index;
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillHeight: true
                    Layout.leftMargin: 5
                    width: 1
                    color: EPGColors.gridColor
                }
                Item {
                    id: controlsItem
                    Layout.alignment: Qt.AlignVCenter
                    width: switchRow.width
                    height: switchRow.height
                    RowLayout {
                        id: switchRow
                        spacing: 10
                        EPGSwitch {
                            id: emptyFilterSwitch
                            text: qsTr("Hide services without program")
                            checked: epgDialog.filterEmptyEpg
                            onToggled: {
                                epgDialog.filterEmptyEpg = checked;
                            }
                        }
                        EPGSwitch {
                            id: ensembleFilterSwitch
                            text: qsTr("Show only current ensemble")
                            checked: epgDialog.filterEnsemble
                            onToggled: {
                                epgDialog.filterEnsemble = checked;
                            }
                        }
                    }
                }
            }
            Item {
                id: epgViewItem
                anchors.top: controlLayout.bottom
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
                    anchors.leftMargin: 14
                    width: implicitWidth
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: EPGColors.textColor
                    text: qsTr("Current time: ") + epgTime.currentTimeString
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
                                        color: EPGColors.fadeTextColor
                                    }
                                    Text {
                                        text: modelData + ":30"
                                        width: 60 * 30 * pointsPerSecond
                                        height: 20
                                        horizontalAlignment: Text.AlignLeft
                                        opacity: 1 - Math.min(timelinebox.contentX  - (modelData+0.5)*3600*pointsPerSecond, 50)/50
                                        color: EPGColors.fadeTextColor
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
                    border.color: EPGColors.gridColor
                    border.width: 1
                    Flickable {
                        id: mainView
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        anchors.fill: parent
                        //anchors.margins: 2
                        contentHeight: epgItem.height
                        contentWidth: epgItem.width
                        clip: true
                        Rectangle {
                            color: "transparent"
                            //border.color: EPGColors.gridColor
                            //border.width: 2
                            id: epgItem
                            height: serviceColumnId.height
                            width: epgViewItem.width
                            Column {
                                id: serviceColumnId
                                Repeater {
                                    id: serviceList
                                    model: slProxyModel
                                    delegate: Rectangle {
                                        color: "transparent"
                                        border.color: EPGColors.gridColor
                                        border.width: 1
                                        height: lineHeight
                                        width: serviceListWidth
                                        Row {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 10
                                            Rectangle {
                                                color: "transparent"
                                                border.width: 2
                                                border.color: selectedServiceIndex == index ? EPGColors.highlightColor : "transparent"
                                                width: logoLoader.width + 8
                                                height: logoLoader.height + 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                Loader {
                                                    id: logoLoader
                                                    anchors.centerIn: parent
                                                    sourceComponent: smallLogoId == 0 ? placeholderComponent : logoComponent
                                                }
                                                Component {
                                                    id: logoComponent
                                                    Image {
                                                        id: logoId
                                                        source: "image://metadata/"  + smallLogoId
                                                        cache: false
                                                    }
                                                }
                                                Component {
                                                    id: placeholderComponent
                                                    Rectangle {
                                                        color: EPGColors.emptyLogoColor
                                                        width: 32
                                                        height: 32
                                                    }
                                                }
                                            }
                                            Text {
                                                id: labelId
                                                text: display
                                                color: EPGColors.textColor
                                                font.bold: selectedServiceIndex == index
                                                anchors.verticalCenter: parent.verticalCenter
                                                verticalAlignment: Text.AlignVCenter
                                                horizontalAlignment: Text.AlignLeft
                                            }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                slSelectionModel.select(slProxyModel.mapToSource(slProxyModel.index(index, 0)), ItemSelectionModel.Select | ItemSelectionModel.Current)
                                                slSelectionModel.setCurrentIndex(slProxyModel.mapToSource(slProxyModel.index(index, 0)), ItemSelectionModel.Select | ItemSelectionModel.Current)
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
                                            property bool needsToSetContentX: true
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
                                                            border.color: EPGColors.gridColor
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
                                                                text: qsTr("No program available")
                                                                color: EPGColors.fadeTextColor
                                                                visible: epgItemRepeater.count === 0
                                                            }
                                                            Repeater {
                                                                id: epgItemRepeater
                                                                model: proxyModel
                                                                delegate: EPGItem {
                                                                    itemHeight: lineHeight
                                                                    pointsPerSec: pointsPerSecond
                                                                    viewX: epgTable.contentX
                                                                    isSelected: selectedEpgItemIndex.valid
                                                                                && (selectedEpgItemIndex.model === proxyModel.mapToSource(proxyModel.index(index, 0)).model)
                                                                                && (selectedEpgItemIndex.row === proxyModel.mapToSource(proxyModel.index(index, 0)).row)
                                                                    onClicked: {
                                                                        //console.log("clicked")
                                                                        programDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                        programName = (longName !== "") ? longName : mediumName;
                                                                        programStartToEndTime = startToEndTimeString;
                                                                        scheduleRecEnable = (startTimeSecSinceEpoch > currentTimeSec);

                                                                        epgDialog.selectedEpgItem = proxyModel.mapToSource(proxyModel.index(index, 0));
                                                                    }
                                                                    Component.onCompleted: {
                                                                        if (selectedEpgItemIndex.valid) {
                                                                            if ((selectedEpgItemIndex.model === proxyModel.mapToSource(proxyModel.index(index, 0)).model)
                                                                                    && (selectedEpgItemIndex.row === proxyModel.mapToSource(proxyModel.index(index, 0)).row))
                                                                            {
                                                                                programDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                                programName = (longName !== "") ? longName : mediumName;
                                                                                programStartToEndTime = startToEndTimeString;
                                                                                scheduleRecEnable = (startTimeSecSinceEpoch > currentTimeSec);
                                                                                console.log(startTimeSecSinceEpoch, currentTimeSec, scheduleRecEnable)
                                                                            }
                                                                        }
                                                                        else {
                                                                            if (epgForService.isCurrentService) {
                                                                                if ((startTimeSecSinceEpoch <= currentTimeSec) && (endTimeSecSinceEpoch > currentTimeSec))
                                                                                {   // if it is ongoing programme select it
                                                                                    epgDialog.selectedEpgItem = proxyModel.mapToSource(proxyModel.index(index, 0));
                                                                                    programDetail = (longDescription !== "") ? longDescription : shortDescription;
                                                                                    programName = (longName !== "") ? longName : mediumName;
                                                                                    programStartToEndTime = startToEndTimeString;
                                                                                    scheduleRecEnable = (startTimeSecSinceEpoch > currentTimeSec);
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
                                            onWidthChanged: {
                                                if (needsToSetContentX && (width !== 0)) {
                                                    if (epgTime.isCurrentDate(metadataManager.epgDate(epgTable.dateIndex))) {
                                                        const w = mainItemId.width - serviceListWidth;
                                                        const cX = Math.max(epgTime.secSinceMidnight() * pointsPerSecond - w/3, 0);
                                                        timelinebox.contentX = Math.min(cX, (3600*24) * pointsPerSecond - w);
                                                    }
                                                    else {
                                                        timelinebox.contentX = 0;
                                                    }
                                                    needsToSetContentX = false;
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
                    EPGHorizontalShadow {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        width: parent.width
                        topDownDirection: true
                        visible: mainView.contentY > 0
                    }
                    EPGHorizontalShadow {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: parent.width
                        topDownDirection: false
                        visible: mainView.contentY < (mainView.contentHeight - mainView.height)
                    }
                }
            }
            Rectangle {
                id: progDetails
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.bottomMargin: 14
                color: "transparent"
                height: 150
                ColumnLayout {
                    id: progDetailsLayout
                    anchors.fill: parent
                    spacing: 5                    
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text {
                                id: programNameLabel
                                Layout.fillWidth: true
                                text: programName
                                font.bold: true
                                font.pointSize: 16
                                color: EPGColors.textColor
                            }
                            Text {
                                visible: programStartToEndTime
                                Layout.fillWidth: true
                                Layout.bottomMargin: 3
                                text: programStartToEndTime
                                verticalAlignment: Text.AlignVCenter
                                color: EPGColors.fadeTextColor
                            }
                        }
                        EPGButton {
                            id: scheduleRecButton
                            Layout.alignment: Qt.AlignTop
                            text: qsTr("Schedule audio recording")
                            visible: programStartToEndTime !== "" && scheduleRecEnable
                            onClicked: epgDialog.scheduleRecording()
                        }

                    }
                    EPGScrollableText {
                        id: serviceScrollViewId
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: programDetail
                    }
                }
            }
        }
    }
}
