import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProgrammeGuide
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5

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

    TabBar {
        id: dayTabBar
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        visible: metadataManager.epgDatesList.length > 0
        height: metadataManager.epgDatesList.length > 0 ? implicitHeight : 0
        currentIndex: stackLayoutId.currentIndex
        Repeater {
            model: metadataManager.epgDatesList
            TabButton {
                text: epgTime.currentDateString === modelData ? qsTr("Today") : modelData
                //width: Math.max(100, bar.width / metadataManager.epgDatesList.length)
            }
        }
    }
    Item {
        id: epgViewItem
        anchors {
            top: dayTabBar.bottom
            left: parent.left
            right: parent.right
            bottom: progDetails.top
            topMargin: 10
            bottomMargin: 20
        }
        Flickable {
            id: timelinebox
            height: 20
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                leftMargin: serviceListWidth
            }
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
            anchors {
                top: timelinebox.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            color: "transparent"
            border.color: "lightgray"
            border.width: 1            
            Flickable {
                id: mainView
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                // anchors {
                //     top: timelinebox.bottom
                //     left: parent.left
                //     right: parent.right
                //     bottom: parent.bottom
                // }
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
                            model: slModel
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
                                        slSelectionModel.select(slModel.index(index, 0), ItemSelectionModel.Select | ItemSelectionModel.Current)
                                        slSelectionModel.setCurrentIndex(slModel.index(index, 0), ItemSelectionModel.Select | ItemSelectionModel.Current)
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
                                            model: slModel
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
                                                        anchors {
                                                            top: parent.top
                                                            bottom: parent.bottom
                                                        }
                                                        verticalAlignment: Text.AlignVCenter
                                                        x: epgTable.contentX + 5
                                                        text: qsTr("No data available")
                                                        visible: proxyModel.rowCount === 0
                                                    }

                                                    Repeater {
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
                                            timelinebox.contentX = (Math.floor(epgTime.secSinceMidnight() / 3600) - 1) * 3600 * pointsPerSecond;
                                        }
                                        else {
                                            timelinebox.contentX = 0;
                                        }
                                    }
                                    onContentXChanged: {
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
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
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
