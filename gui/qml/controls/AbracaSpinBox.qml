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
import abracaComponents

SpinBox {
    id: control

    property string suffix: ""
    property int specialValue: 0
    property string specialValueString: ""
    locale: Qt.locale("C")

    validator: RegularExpressionValidator {
        regularExpression: control.numberExtractionRegExp
    }

    up.onPressedChanged: if (up.pressed) { forceActiveFocus(); }
    down.onPressedChanged: if (down.pressed) { forceActiveFocus(); }

    // Hint for numeric input on virtual keyboards.
    inputMethodHints: Qt.ImhDigitsOnly

    readonly property color buttonIconColor: UI.colors.textSecondary
    readonly property regexp numberExtractionRegExp: /\D*?(-?\d*\.?\d*)\D*$/
    readonly property int symbolLineWidth: 2
    readonly property int symbolLineLen: Math.floor(font.pointSize* 1.2 / 2) * 2 // UI.controlHeightSmall - 4

    textFromValue: function (value, locale) {
        if (control.specialValueString.length > 0 && value === control.specialValue) {
            return control.specialValueString;
        }
        return Number(value).toLocaleString(locale, 'f', 0); // + control.suffix
        //return Number(value).toLocaleString(locale, 'f', 0)
    }

    valueFromText: function (text, locale) {
        var n = Number.fromLocaleString(locale, control.numberExtractionRegExp.exec(text)[1]);
        // Clamp to allowed range
        if (n < control.from) {
            n = control.from;
        }
        if (n > control.to) {
            n = control.to;
        }
        return n;
    }

    readonly property bool suffixVisible: control.suffix.length > 0 && !(control.specialValueString.length > 0 && value === control.specialValue)

    FontMetrics {
        id: fontMetrics
        font: control.font
    }
    implicitWidth: 2*UI.controlHeight + 2*UI.standardMargin +
                   Math.max(fontMetrics.boundingRect(specialValueString).width,
                            Math.max(fontMetrics.boundingRect(textFromValue(from, locale)).width, fontMetrics.boundingRect(textFromValue(to, locale)).width) + fontMetrics.boundingRect(suffix).width)

    rightPadding: UI.standardMargin + 2 * control.height + (suffixVisible ? fontMetrics.boundingRect(control.suffix).width : 0)

    contentItem: Item {
        id: contentItem
        anchors.fill: control
        anchors.rightMargin: 2 * UI.controlHeight
        property alias text: textInput.text
        Row {
            id: inputRow
            //anchors.centerIn: contentItem
            anchors.right: contentItem.right
            anchors.verticalCenter: contentItem.verticalCenter
            TextInput {
                id: textInput
                z: 2
                text: control.displayText // control.textFromValue(control.value, control.locale)

                font: control.font
                color: UI.colors.textPrimary
                selectionColor: UI.colors.selectionColor
                selectedTextColor: UI.colors.textSelected
                horizontalAlignment: control.suffixVisible ? Qt.AlignRight : Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter

                readOnly: !control.editable
                validator: control.validator
                inputMethodHints: control.inputMethodHints
                clip: width < implicitWidth
                opacity: control.enabled ? 1.0 : 0.5
                Behavior on text {
                    SequentialAnimation {
                        NumberAnimation { target: textInput; property: "scale"; to: 0.8; duration: 70 }
                        NumberAnimation { target: textInput; property: "scale"; to: 1; duration: 120; easing.type: Easing.OutQuad }
                    }
                }
            }
            Text {
                text: control.suffix
                z: 1
                color: UI.colors.textSecondary
                visible: control.suffixVisible
                opacity: control.enabled ? 1.0 : 0.5
            }
        }
    }

    up.indicator: Rectangle {
        id: upIndicator
        x: control.mirrored ? parent.width - 2 * width : parent.width - width
        height: parent.height
        width: height
        implicitWidth: UI.controlHeight
        implicitHeight: UI.controlHeight
        color: "transparent" // control.up.pressed ? "#e4e4e4" : "#f6f6f6"

        Rectangle {
            color: control.buttonIconColor
            anchors.centerIn: parent
            width: control.symbolLineLen
            height: control.symbolLineWidth
            opacity: control.enabled ? 1.0 : 0.5
        }
        Rectangle {
            color: control.buttonIconColor
            anchors.centerIn: parent
            height: control.symbolLineLen
            width: control.symbolLineWidth
            opacity: control.enabled ? 1.0 : 0.5
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : parent.width - 2 * width
        height: parent.height
        width: height
        implicitWidth: UI.controlHeight
        implicitHeight: UI.controlHeight
        color: "transparent" //control.down.pressed ? "#e4e4e4" : "#f6f6f6"
        Rectangle {
            color: control.buttonIconColor
            anchors.centerIn: parent
            width: control.symbolLineLen
            height: control.symbolLineWidth
            opacity: control.enabled ? 1.0 : 0.5
        }
    }

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: UI.controlHeight
        color: UI.colors.inputBackground
        border.color: control.enabled && (control.hovered || control.activeFocus) ? UI.colors.accent : UI.colors.inactive
        border.width: 1
        radius: UI.controlRadius
        opacity: control.enabled ? 1.0 : 0.5
    }

}
