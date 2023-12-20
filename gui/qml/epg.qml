import QtQuick
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5
// import QtQuick.Effects              // not available in Qt < 6.5

// Loader {
//     active: true //epgDialog.visible
//     sourceComponent:
    Item {
        id: mainItemId
        anchors.fill: parent

        property int secPerPoint: 15
        property int currentTimeSec: 45*60
        property int lineHeight: 50
        property int serviceListWidth: 200

        Component.onCompleted: console.log("EPG completed", mainItemId.visible)

        Component {
            id: epgDelegate
            Item {
                //Component.onCompleted: console.log(width, height)
                id: progItem
                height: 50
                // width: rect.width //+ shadowItem.width
                clip: true
                width: mouseAreaId.containsMouse ? Math.max(textId.width + 10, durationSec / secPerPoint) : durationSec / secPerPoint
                x:  startTimeSec / secPerPoint
                z: mouseAreaId.containsMouse ? 1000 : index

                Rectangle {
                    id: rect
                    property bool textFits: (textId.width + 10) <= width
                    color: {
                        if (endTimeSec <= currentTimeSec) return "lightgrey";
                        if (startTimeSec >= currentTimeSec) return "white";
                        return "lemonchiffon";
                    }
                    border.color: "darkgray"
                    border.width: 1
                    //height: parent.height
                    //width: durationSec / secPerPoint
                    anchors.fill: parent
                    x:  startTimeSec / secPerPoint
                    // z: mouseAreaId.containsMouse ? 1000 : index

                    Rectangle {
                        id: remainingTime
                        visible: (startTimeSec < currentTimeSec) && (endTimeSec > currentTimeSec)
                        color: "gold"
                        anchors {
                            left: parent.left
                            top: parent.top
                            bottom: parent.bottom
                            topMargin: 1
                            bottomMargin: 1
                            leftMargin: 1
                        }
                        width: (currentTimeSec - startTimeSec) / secPerPoint
                    }
                    Text {
                        id: textId
                        anchors {
                            left: parent.left
                            leftMargin: 5
                            top: parent.top
                            bottom: parent.bottom
                        }
                        text: name
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignLeft
                    }
                    clip: true
                    visible: true
                }

                Rectangle {
                    id: shadowItem
                    color: "black"
                    anchors.left: rect.right
                    anchors.top: rect.top
                    anchors.bottom: rect.bottom
                    width: 100
                    visible: false
                }
                // Qt >= 6.5
                /*
            MultiEffect {
                source: shadowItem
                anchors.fill: shadowItem
                autoPaddingEnabled: true
                shadowBlur: 1.0
                shadowColor: 'black'
                shadowEnabled: true
                shadowOpacity: 0.5
                shadowHorizontalOffset: -2 // rect.shadowSize
                shadowVerticalOffset: 0
                visible: !rect.textFits
            }
            */
                DropShadow {
                    id: shadowId
                    anchors.fill: shadowItem
                    source: shadowItem
                    samples: 21
                    color: "black"
                    enabled: true
                    opacity: 0.5
                    horizontalOffset: -2
                    verticalOffset: 0
                    visible: !rect.textFits
                }
                MouseArea {
                    id: mouseAreaId
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
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
            contentX: epgTable.contentX
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
                    //anchors.fill: parent
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
                                        source: "image://metadata/"  + serviceId
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
                    // working
                    Item {
                        //anchors.fill: parent
                        anchors.left: serviceColumnId.right
                        height: serviceColumnId.height
                        width: viewItem.width - serviceColumnId.width
                        Component.onCompleted: console.log(width, height)
                        Flickable {
                            id: epgTable
                            anchors.fill: parent
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
                                        // border.color: "black"
                                        // border.width: 1
                                        height: lineHeight
                                        anchors.left: parent.left
                                        //anchors.right: parent.right
                                        width: 24*3600/secPerPoint
                                        clip: true
                                        Repeater {
                                            model: epgModelRole
                                            delegate: epgDelegate
                                        }
                                    }
                                }
                            }

                            Timer {
                                id: currentTimerTimer
                                interval: 100
                                repeat: true
                                onTriggered: {
                                    currentTimeSec = (currentTimeSec + 60) % (3600*24)
                                }
                            }
                            Component.onCompleted: currentTimerTimer.start()
                        }
                    }
                }
            }
        }
    }
//}

