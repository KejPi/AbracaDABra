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
 * The above copyright nortice and this permission notice shall be included in all
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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import abracaComponents 1.0

Item {
    id: root

    // Public properties
    property date selectedDate: new Date()
    property string label: qsTr("Date")
    property color backgroundColor: UI.colors.inputBackground
    property color expandedBackgroundColor: UI.colors.backgroundLight
    property color borderColor: UI.colors.inactive
    property color activeDateColor: UI.colors.textPrimary
    property color inactiveDateColor: UI.colors.textDisabled
    property color selectedDateBackgroundColor: UI.colors.accent
    property color todayBorderColor: UI.colors.inactive

    // Signals
    signal dateChanged(date newDate)

    // Internal properties
    property bool expanded: false
    property int collapsedHeight: UI.controlHeight
    property int expandedHeight: 340
    property date today: new Date(new Date().getFullYear(), new Date().getMonth(), new Date().getDate())

    implicitWidth: 320
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

        // Header - Label and Date Display (always visible)
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
                    Layout.fillWidth: true
                }

                Text {
                    id: dateDisplay
                    text: Qt.formatDate(root.selectedDate, Qt.DefaultLocaleLongDate)
                    font.bold: true
                    color: UI.colors.textPrimary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Calendar View (visible when expanded)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 2*UI.standardMargin
            Layout.rightMargin: 2*UI.standardMargin
            Layout.bottomMargin: UI.standardMargin
            visible: expanded
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: UI.standardMargin

                // Month/Year Navigation
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        id: prevMonthButton
                        Layout.preferredWidth: UI.controlHeight
                        Layout.preferredHeight: UI.controlHeight

                        icon {
                            source: UI.imagesUrl + "chevron-left.svg"
                            height: UI.iconSize
                            color: UI.colors.buttonTextNeutral
                        }

                        background: Rectangle {
                            color: prevMonthButton.pressed ? UI.colors.buttonNeutralClicked :
                                   prevMonthButton.hovered ? UI.colors.buttonNeutralHover :
                                   UI.colors.buttonNeutral
                            radius: UI.controlRadius

                            Behavior on color {
                                ColorAnimation { duration: 100 }
                            }
                        }

                        onClicked: {
                            var newDate = new Date(monthGrid.year, monthGrid.month - 1, 1)
                            monthGrid.month = newDate.getMonth()
                            monthGrid.year = newDate.getFullYear()
                        }
                    }

                    AbracaLabel {
                        id: monthYearLabel
                        text: {
                            var date = new Date(monthGrid.year, monthGrid.month, 1)
                            return Qt.formatDate(date, "MMMM yyyy")
                        }
                        font.bold: true
                        role: UI.LabelRole.Secondary
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    Button {
                        id: nextMonthButton
                        Layout.preferredWidth: UI.controlHeight
                        Layout.preferredHeight: UI.controlHeight

                        icon {
                            source: UI.imagesUrl + "chevron-right.svg"
                            height: UI.iconSize
                            color: UI.colors.buttonTextNeutral
                        }

                        background: Rectangle {
                            color: nextMonthButton.pressed ? UI.colors.buttonNeutralClicked :
                                   nextMonthButton.hovered ? UI.colors.buttonNeutralHover :
                                   UI.colors.buttonNeutral
                            radius: UI.controlRadius

                            Behavior on color {
                                ColorAnimation { duration: 100 }
                            }
                        }

                        onClicked: {
                            var newDate = new Date(monthGrid.year, monthGrid.month + 1, 1)
                            monthGrid.month = newDate.getMonth()
                            monthGrid.year = newDate.getFullYear()
                        }
                    }
                }

                // Day of Week Row
                DayOfWeekRow {
                    Layout.fillWidth: true
                    locale: Qt.locale()

                    delegate: Text {
                        required property var model
                        text: model.shortName
                        font.bold: true
                        color: UI.colors.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Month Grid
                MonthGrid {
                    id: monthGrid
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    month: root.selectedDate.getMonth()
                    year: root.selectedDate.getFullYear()
                    locale: Qt.locale()

                    delegate: Rectangle {
                        required property var model

                        color: {
                            if (model.month !== monthGrid.month) return "transparent"
                            if (isSameDate(model.date, root.selectedDate)) {
                                return root.selectedDateBackgroundColor
                            }
                            if (delegateMouseArea.containsMouse && !isPastDate(model.date)) {
                                return UI.colors.listItemHovered
                            }
                            return "transparent"
                        }
                        radius: 6
                        border.width: isSameDate(model.date, root.today) ? 1 : 0
                        border.color: isSameDate(model.date, root.today) ? root.selectedDateBackgroundColor : root.todayBorderColor
                        AbracaLabel {
                            anchors.centerIn: parent
                            text: model.day
                            // font.pixelSize: 14
                            color: {
                                if (model.month !== monthGrid.month) return "transparent"
                                if (isSameDate(model.date, root.selectedDate)) {
                                    return "white"
                                }
                                if (isPastDate(model.date)) {
                                    return root.inactiveDateColor
                                }
                                return UI.colors.textPrimary
                            }
                            font.bold: isSameDate(model.date, root.selectedDate)
                        }

                        MouseArea {
                            id: delegateMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: model.month === monthGrid.month && !isPastDate(model.date)
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                            onClicked: {
                                if (model.month === monthGrid.month && !isPastDate(model.date)) {
                                    root.selectedDate = model.date
                                    root.dateChanged(model.date)
                                    root.expanded = false
                                    root.focus = false
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

    // Functions
    function isPastDate(date) {
        if (!date) return false
        var checkDate = new Date(date.getFullYear(), date.getMonth(), date.getDate())
        return checkDate < root.today
    }

    function isSameDate(date1, date2) {
        if (!date1 || !date2) return false
        return date1.getFullYear() === date2.getFullYear() &&
               date1.getMonth() === date2.getMonth() &&
               date1.getDate() === date2.getDate()
    }

    function setDate(date) {
        root.selectedDate = date
        monthGrid.month = date.getMonth()
        monthGrid.year = date.getFullYear()
    }

    // Watch for selected date changes to update display month
    onSelectedDateChanged: {
        if (selectedDate.getMonth() !== monthGrid.month ||
            selectedDate.getFullYear() !== monthGrid.year) {
            monthGrid.month = selectedDate.getMonth()
            monthGrid.year = selectedDate.getFullYear()
        }
    }
}

