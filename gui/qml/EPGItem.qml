import QtQuick
import Qt5Compat.GraphicalEffects   // required for Qt < 6.5
// import QtQuick.Effects              // not available in Qt < 6.5


Item {
    id: progItem

    property double pointsPerSec: 0.0
    //property bool isCurrentService: false

    signal clicked(int index)

    height: 50
    clip: true
    width: mouseAreaId.containsMouse ? Math.max(textId.width + 10, durationSec * pointsPerSec) : durationSec * pointsPerSec
    x:  startTimeSec * pointsPerSec
    z: mouseAreaId.containsMouse ? 1000 : index

    Rectangle {
        id: rect
        property bool textFits: (textId.width + 10) <= width
        color: {
            if (endTimeSecSinceEpoch <= currentTimeSec) return "lightgrey";
            if (startTimeSecSinceEpoch >= currentTimeSec) return "white";
            return "lemonchiffon";
        }
        border.color: "darkgray" // isCurrentService ? "black" : "darkgray"
        border.width: 1
        anchors.fill: parent
        x:  startTimeSec * pointsPerSec

        Rectangle {
            id: remainingTime
            visible: (startTimeSecSinceEpoch < currentTimeSec) && (endTimeSecSinceEpoch > currentTimeSec)
            color: "gold"
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                topMargin: 1
                bottomMargin: 1
                leftMargin: 1
            }
            width: (currentTimeSec - startTimeSecSinceEpoch) * pointsPerSec
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
        onClicked: {
            progItem.clicked(index)
        }
    }
}
