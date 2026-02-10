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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Basic

import abracaComponents

Button {
    id: control

    property int buttonRole: UI.ButtonRole.Neutral
    property int buttonType: UI.ButtonType.Normal
    property bool hasBackground: true
    property string iconSource: ""

    hoverEnabled: true

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onSpacePressed: clicked()

    //-----
    readonly property color bgColor: {
        if (hasBackground) {
            switch (buttonRole) {
            case UI.ButtonRole.Primary:
                return UI.colors.buttonPrimary;
            case UI.ButtonRole.Positive:
                return UI.colors.buttonPositive;
            case UI.ButtonRole.Negative:
                return UI.colors.buttonNegative;
            case UI.ButtonRole.Neutral:
            default:
                return UI.colors.buttonNeutral;
            }
        }
        return "transparent";
    }
    readonly property color textColor: {
        if (hasBackground) {
            switch (buttonRole) {
            case UI.ButtonRole.Primary:
                return UI.colors.buttonTextPrimary;
            case UI.ButtonRole.Positive:
                return UI.colors.buttonTextPositive;
            case UI.ButtonRole.Negative:
                return UI.colors.buttonTextNegative;
            case UI.ButtonRole.Neutral:
            default:
                return UI.colors.buttonTextNeutral;
            }
        }
        switch (buttonRole) {
        case UI.ButtonRole.Primary:
            return UI.colors.buttonPrimary;
        case UI.ButtonRole.Positive:
            return UI.colors.buttonPositive;
        case UI.ButtonRole.Negative:
            return UI.colors.buttonNegative;
        case UI.ButtonRole.Neutral:
        default:
            return UI.colors.buttonNeutral;
        }
    }

    readonly property color hoverColor: {
        if (hasBackground) {
            switch (buttonRole) {
            case UI.ButtonRole.Primary:
                return UI.colors.buttonPrimaryHover;
            case UI.ButtonRole.Positive:
                return UI.colors.buttonPositiveHover;
            case UI.ButtonRole.Negative:
                return UI.colors.buttonNegativeHover;
            case UI.ButtonRole.Neutral:
            default:
                return UI.colors.buttonNeutralHover;
            }
        }
        return "transparent";
    }
    readonly property color clickedColor: {
        if (hasBackground) {
            switch (buttonRole) {
            case UI.ButtonRole.Primary:
                return UI.colors.buttonPrimaryClicked;
            case UI.ButtonRole.Positive:
                return UI.colors.buttonPositiveClicked;
            case UI.ButtonRole.Negative:
                return UI.colors.buttonNegativeClicked;
            case UI.ButtonRole.Neutral:
            default:
                return UI.colors.buttonNeutralClicked;
            }
        }
        return "transparent";
    }

    // this is for corerct opacity handling
    layer.enabled: !control.enabled
    opacity: control.enabled ? 1.0 : 0.5

    background: Rectangle {
        implicitWidth: Math.max(UI.controlWidth, contentItem.implicitWidth + 4*UI.standardMargin)
        implicitHeight: UI.controlHeight
        radius: UI.controlRadius
        color: control.enabled ?
                control.pressed ? control.clickedColor :
                control.hovered ? control.hoverColor :
                                  control.bgColor :
                control.bgColor
        Behavior on color {
            ColorAnimation { duration: 100 }
        }
    }

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text

        font.pointSize: control.font.pointSize
        font.family: control.font.family
        font.weight: Font.Medium
        color: control.textColor
    }
    // Text {
    //     id: textItem
    //     anchors.verticalCenter: parent.verticalCenter
    //     anchors.right: parent.right
    //     text: control.text
    //     //font: control.font
    //     font.pointSize: control.font.pointSize
    //     font.family: control.font.family
    //     //font.bold: true
    //     font.weight: Font.Medium
    //     color: control.textColor
    //     horizontalAlignment: Text.AlignHCenter
    //     verticalAlignment: Text.AlignVCenter
    //     elide: Text.ElideRight
    // }
}

