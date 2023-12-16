import QtQuick
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5
// import QtQuick.Effects              // not available in Qt < 6.5

Item {
    // width: 640
    // height: 480
    anchors.fill: parent
    visible: true
    //color: "transparent"
    //title: qsTr("Hello World")


    // Rectangle {
    //     color: "green"
    //     width: parent.width/2
    //     height: parent.height/2
    //     anchors.centerIn: parent
    // }

    property int secPerPoint: 15
    property int currentTimeSec: 45*60

    Component {
        id: epgDelegateTest
        Rectangle {
            width: length * 60 / secPerPoint
            color: itemCcolor
            border.color: "black"
            border.width: 1
            height: parent.height
            Text {
                id: textId
                anchors.fill: parent
                anchors.margins: 4
                text: itemText
                verticalAlignment: Text.AlignVCenter
                elide: Qt.ElideRight
            }
        }
    }
    Component {
        id: epgDelegateOld
        Rectangle {
            width: durationSec / secPerPoint
            color: "transparent"
            border.color: "darkgray"
            border.width: 1
            height: parent.height
            Text {
                id: textId
                anchors.fill: parent
                anchors.margins: 4
                text: name
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Qt.ElideRight
            }
        }
    }

    Component {
        id: epgDelegate
        Item {
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
        id: epgTable
        //anchors.left: parent.left
        //anchors.right: parent.right
        //anchors.top: listViewId.bottom
        //anchors.topMargin: 10
        //anchors.bottom: parent.bottom
        anchors.fill: parent
        contentWidth: colId.width
        contentHeight: colId.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Column {
            id: colId
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
            Rectangle {
                color: "transparent"
                // border.color: "black"
                // border.width: 1
                height: 50
                anchors.left: parent.left
                anchors.right: parent.right
                clip: true
                Repeater {
                    model: epgModel
                    delegate: epgDelegate
                }
            }

            Rectangle {
                color: "transparent"
                // border.color: "black"
                // border.width: 1
                height: 50
                anchors.left: parent.left
                anchors.right: parent.right
                clip: true
                Repeater {
                    model: epgModel
                    delegate: epgDelegate
                }
            }
        }

        Timer {
            id: currentTimerTimer
            interval: 100
            repeat: true
            onTriggered: {
                currentTimeSec = (currentTimeSec + 2*60) % (24*3600)
                //epgTable.contentX = (currentTimeSec - 40*60)/secPerPoint
            }
        }
        Component.onCompleted: {
            currentTimerTimer.start()
        }
        //Component.onVis

        // Rectangle {
        //     x: currentTimeSec / secPerPoint
        //     z: 10
        //     width: 2
        //     anchors.top: colId.top
        //     anchors.topMargin: 20-5
        //     anchors.bottom: colId.bottom
        //     color: "blue"
        // }
        //Component.onCompleted: epgTable.contentX = 240+20
    }
}

