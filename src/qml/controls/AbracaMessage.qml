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
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Effects

import abracaComponents

ToolTip {
    id: control
    y: parent.height - height - 60

    timeout: messageType === UI.InfoMessageType.Negative ? 4000 : 2000
    property int messageType: UI.InfoMessageType.Neutral

    property color textColor: messageType === UI.InfoMessageType.Positive ?  UI.colors.buttonTextPositive
                                    : (messageType === UI.InfoMessageType.Negative ? UI.colors.buttonTextNegative
                                                                                   : UI.colors.textSecondary)

    property color backgroundColor: messageType === UI.InfoMessageType.Positive ?  UI.colors.buttonPositive
                                : (messageType === UI.InfoMessageType.Negative ? UI.colors.buttonNegative
                                                                               : UI.colors.backgroundLight)

    visible: false
    delay: Application.styleHints.mousePressAndHoldInterval

    leftPadding: UI.standardMargin
    rightPadding: UI.standardMargin
    topPadding: UI.standardMargin
    bottomPadding: UI.standardMargin

    topInset: -10
    bottomInset: -10
    rightInset: -10
    leftInset: -10

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.textColor
        wrapMode: Text.WordWrap
    }


    background: Item {
        id: backgroundItem
        Rectangle {
            id: backgroundRect
            anchors.margins: 10
            anchors.fill: backgroundItem
            radius: UI.controlRadius
            color: control.backgroundColor
            border.width: UI.colors.background < "#808080" ? 1 : 0
            border.color: UI.colors.divider
        }
        MultiEffect {
            source: backgroundRect
            anchors.fill: backgroundRect
            autoPaddingEnabled: true
            shadowBlur: 1.0
            shadowColor: 'black'
            shadowEnabled: true
            shadowOpacity: 0.5
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 5
            visible: true
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutQuad; duration: 500 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.InQuad; duration: 500 }
    }

}
