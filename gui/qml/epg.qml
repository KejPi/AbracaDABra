import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ProgrammeGuide

Item {
    id: mainItemId
    anchors.fill: parent

    property int secPerPoint: 15
    property int currentTimeSec: dabTime.secSinceEpoch
    property int lineHeight: 50
    property int serviceListWidth: 200

    Component.onCompleted: console.log("EPG completed", mainItemId.visible)

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
                width: 24*3600/secPerPoint
                Row {
                    Repeater {
                        model: 24
                        delegate: Text {
                            text: modelData + ":00"
                            width: 60 * 60 / secPerPoint
                            height: 20
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }
            }
        }
        Item {
            id: viewItem
            anchors {
                top: timelinebox.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            Flickable {
                id: mainView
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                anchors.fill: parent
                contentHeight: epgItem.height
                contentWidth: epgItem.width
                clip: true
                Item {
                    id: epgItem
                    height: serviceColumnId.height
                    width: parent.width
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
                        //Component.onCompleted: console.log(slModel.count, slModel.rowCount)
                    }

                    StackLayout {
                        id: stackLayoutId
                        anchors.left: serviceColumnId.right
                        height: serviceColumnId.height
                        width: viewItem.width - serviceColumnId.width
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
                                    //anchors.fill: parent
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
                                                //anchors.right: parent.right
                                                width: 24*3600/secPerPoint
                                                clip: true

                                                EPGProxyModel {
                                                    id: proxyModel
                                                    sourceModel: epgModelRole
                                                    //dayFilter: bar.currentIndex
                                                    dateFilter: metadataManager.epgDate(epgTable.dateIndex)
                                                }

                                                Repeater {
                                                    model: proxyModel
                                                    //delegate: epgDelegate
                                                    delegate: EPGItem {}
                                                }
                                            }
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
}
