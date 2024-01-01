import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProgrammeGuide

Item {
    id: mainItemId
    anchors.fill: parent

    property double pointsPerSecond: 1.0/15   // 15 sec/point
    property int currentTimeSec: dabTime.secSinceEpoch
    property int lineHeight: 50
    property int serviceListWidth: 200

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
                text: modelData
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
            bottom: parent.bottom
            topMargin: 10
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
            flickableDirection: Flickable.HorizontalFlick
            clip: true
            Item {
                id: timeline
                height: 20
                width: 24*3600*pointsPerSecond
                Row {
                    Repeater {
                        model: 24
                        delegate: Text {
                            text: modelData + ":00"
                            width: 60 * 60 * pointsPerSecond
                            height: 20
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }
            }
        }
        Flickable {
            id: mainView
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            anchors {
                top: timelinebox.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
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
                            color: "transparent"
                            border.color: "darkgray"
                            border.width: 1
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
                                        Rectangle {
                                            color: "transparent"
                                            border.color: "darkgray"
                                            border.width: 1
                                            height: lineHeight
                                            anchors.left: parent.left
                                            width: 24*3600 * pointsPerSecond
                                            clip: true

                                            EPGProxyModel {
                                                id: proxyModel
                                                sourceModel: epgModelRole
                                                dateFilter: metadataManager.epgDate(epgTable.dateIndex)
                                            }

                                            Repeater {
                                                model: proxyModel
                                                delegate: EPGItem {
                                                    pointsPerSec: pointsPerSecond
                                                }
                                            }
                                        }
                                    }
                                }
                                Component.onCompleted: {
                                    if (dabTime.isCurrentDate(metadataManager.epgDate(epgTable.dateIndex))) {
                                        timelinebox.contentX = (Math.floor(dabTime.secSinceMidnight() / 3600) - 1) * 3600 * pointsPerSecond;
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
