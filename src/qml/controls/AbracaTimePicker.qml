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
import QtQuick.Controls.Basic
import QtQuick.Layouts

import abracaComponents

Item {
    id: root

    // Public properties
    property date selectedTime: new Date()
    property string label: qsTr("Time")
    property color backgroundColor: UI.colors.inputBackground
    property color expandedBackgroundColor: UI.colors.backgroundLight
    property color borderColor: UI.colors.inactive
    property int labelFontSize: 12
    property int timeFontSize: 16
    property bool use24HourFormat: Qt.locale().timeFormat(Locale.ShortFormat).indexOf("AP") === -1 &&
                                    Qt.locale().timeFormat(Locale.ShortFormat).indexOf("ap") === -1

    // Signals
    signal timeChanged(date newTime)

    // Internal properties
    property bool expanded: false
    property int collapsedHeight: UI.controlHeight
    property int expandedHeight: 240

    implicitWidth: 200
    implicitHeight: expanded ? expandedHeight : collapsedHeight

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    // Background
    Rectangle {
        anchors.fill: parent
        color: expanded ? expandedBackgroundColor : backgroundColor
        border.color: borderColor
        border.width: 1
        radius: UI.controlRadius

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: UI.standardMargin

        // Header - Label and Time Display (always visible)
        MouseArea {
            Layout.fillWidth: true
            Layout.preferredHeight: root.collapsedHeight
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (!expanded) {
                    root.focus = true
                    root.forceActiveFocus()
                } else {
                    root.expanded = false
                    root.focus = false
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 2*UI.standardMargin
                anchors.rightMargin: 2*UI.standardMargin

                Text {
                    id: labelText
                    text: root.label
                    color: UI.colors.textPrimary
                }

                Text {
                    id: timeDisplay
                    text: formatTime(root.selectedTime)
                    // font.pixelSize: root.timeFontSize
                    color: UI.colors.textPrimary
                    font.bold: true
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Tumblers (visible when expanded)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: expanded
            clip: true

            Rectangle {
                color: backgroundColor
                height: 2*separatorLabel.height
                radius: 8
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: UI.standardMargin
                anchors.rightMargin: UI.standardMargin
                //anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: (hourLabel.height + 4)/ 2
            }

            Row {
                anchors.centerIn: parent
                spacing: 8

                // Hour Tumbler
                Column {
                    spacing: 4

                    AbracaLabel {
                        id: hourLabel
                        text: qsTr("Hour")
                        anchors.horizontalCenter: parent.horizontalCenter
                        role: UI.LabelRole.Secondary
                    }

                    Item {
                        width: use24HourFormat ? 70 : 60
                        height: 150

                        Tumbler {
                            id: hourTumbler
                            anchors.fill: parent
                            visibleItemCount: 5

                            model: use24HourFormat ? 24 : 12

                            delegate: AbracaLabel {
                                text: use24HourFormat ?
                                      String(modelData).padStart(2, '0') :
                                      (modelData === 0 ? "12" : String(modelData).padStart(2, '0'))
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)

                                TapHandler {
                                    onTapped: hourTumbler.currentIndex = index
                                }
                            }

                            onCurrentIndexChanged: updateTimeFromTumblers()

                            Component.onCompleted: {
                                var hours = root.selectedTime.getHours()
                                if (use24HourFormat) {
                                    currentIndex = hours
                                } else {
                                    var hour12 = hours % 12
                                    currentIndex = hour12
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: (wheel) => {
                                if (wheel.angleDelta.y > 0) {
                                    hourTumbler.currentIndex = (hourTumbler.currentIndex - 1 + hourTumbler.count) % hourTumbler.count
                                } else if (wheel.angleDelta.y < 0) {
                                    hourTumbler.currentIndex = (hourTumbler.currentIndex + 1) % hourTumbler.count
                                }
                            }
                        }
                    }
                }

                // Separator
                AbracaLabel {
                    id: separatorLabel
                    text: ":"
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: (hourLabel.height + 4)/ 2
                }

                // Minute Tumbler
                Column {
                    spacing: 4

                    AbracaLabel {
                        text: qsTr("Minute")
                        anchors.horizontalCenter: parent.horizontalCenter
                        role: UI.LabelRole.Secondary
                    }

                    Item {
                        width: 60
                        height: 150

                        Tumbler {
                            id: minuteTumbler
                            anchors.fill: parent
                            visibleItemCount: 5

                            model: 60

                            delegate: AbracaLabel {
                                text: String(modelData).padStart(2, '0')
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)

                                TapHandler {
                                    onTapped: minuteTumbler.currentIndex = index
                                }
                            }

                            onCurrentIndexChanged: updateTimeFromTumblers()

                            Component.onCompleted: {
                                currentIndex = root.selectedTime.getMinutes()
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: (wheel) => {
                                if (wheel.angleDelta.y > 0) {
                                    minuteTumbler.currentIndex = (minuteTumbler.currentIndex - 1 + minuteTumbler.count) % minuteTumbler.count
                                } else if (wheel.angleDelta.y < 0) {
                                    minuteTumbler.currentIndex = (minuteTumbler.currentIndex + 1) % minuteTumbler.count
                                }
                            }
                        }
                    }
                }

                // AM/PM Tumbler (only for 12-hour format)
                Column {
                    spacing: 4
                    visible: !use24HourFormat

                    AbracaLabel {
                        text: qsTr("Period")
                        font.pixelSize: 10
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Item {
                        width: 60
                        height: 150

                        Tumbler {
                            id: amPmTumbler
                            anchors.fill: parent
                            visibleItemCount: 5

                            model: ["AM", "PM"]

                            delegate: AbracaLabel {
                                text: modelData
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)

                                TapHandler {
                                    onTapped: amPmTumbler.currentIndex = index
                                }
                            }

                            onCurrentIndexChanged: updateTimeFromTumblers()

                            Component.onCompleted: {
                                currentIndex = root.selectedTime.getHours() >= 12 ? 1 : 0
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            onWheel: (wheel) => {
                                if (wheel.angleDelta.y > 0) {
                                    amPmTumbler.currentIndex = (amPmTumbler.currentIndex - 1 + amPmTumbler.count) % amPmTumbler.count
                                } else if (wheel.angleDelta.y < 0) {
                                    amPmTumbler.currentIndex = (amPmTumbler.currentIndex + 1) % amPmTumbler.count
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Focus handling
    focus: false

    onFocusChanged: {
        if (focus && !expanded) {
            expanded = true
        } else if (!focus && expanded) {
            expanded = false
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            expanded = false
            focus = false
        }
    }

    // Close on outside click
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        enabled: expanded
        z: -1

        onPressed: (mouse) => {
            mouse.accepted = false
        }
    }

    // Functions
    function formatTime(time) {
        if (use24HourFormat) {
            return Qt.formatTime(time, "HH:mm")
        } else {
            return Qt.formatTime(time, "hh:mm AP")
        }
    }

    function updateTimeFromTumblers() {
        if (!expanded) return

        var newTime = new Date(root.selectedTime)
        var hours = hourTumbler.currentIndex

        if (!use24HourFormat) {
            // Convert 12-hour format to 24-hour
            if (hours === 0) hours = 12 // 12 AM/PM case
            if (amPmTumbler.currentIndex === 1 && hours !== 12) {
                hours += 12 // PM hours (except 12 PM)
            } else if (amPmTumbler.currentIndex === 0 && hours === 12) {
                hours = 0 // 12 AM = 0 hours
            }
        }

        newTime.setHours(hours)
        newTime.setMinutes(minuteTumbler.currentIndex)
        newTime.setSeconds(0)
        newTime.setMilliseconds(0)

        root.selectedTime = newTime
        root.timeChanged(newTime)
    }

    function setTime(time) {
        root.selectedTime = time

        if (expanded) {
            var hours = time.getHours()

            if (use24HourFormat) {
                hourTumbler.currentIndex = hours
            } else {
                var hour12 = hours % 12
                hourTumbler.currentIndex = hour12
                amPmTumbler.currentIndex = hours >= 12 ? 1 : 0
            }

            minuteTumbler.currentIndex = time.getMinutes()
        }
    }

    // Watch for external time changes
    onSelectedTimeChanged: {
        if (expanded) {
            var hours = selectedTime.getHours()

            if (use24HourFormat) {
                if (hourTumbler.currentIndex !== hours) {
                    hourTumbler.currentIndex = hours
                }
            } else {
                var hour12 = hours % 12
                var isPM = hours >= 12 ? 1 : 0

                if (hourTumbler.currentIndex !== hour12) {
                    hourTumbler.currentIndex = hour12
                }
                if (amPmTumbler.currentIndex !== isPM) {
                    amPmTumbler.currentIndex = isPM
                }
            }

            if (minuteTumbler.currentIndex !== selectedTime.getMinutes()) {
                minuteTumbler.currentIndex = selectedTime.getMinutes()
            }
        }
    }
}
