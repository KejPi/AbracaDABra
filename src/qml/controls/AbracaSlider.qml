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

Slider {
    id: control

    implicitHeight: UI.controlHeight

    layer.enabled: !control.enabled    // for correct opacity handling
    opacity: control.enabled ? 1.0 : 0.5

    onPressedChanged: if (pressed) { forceActiveFocus(); }

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 6
        width: control.availableWidth
        height: implicitHeight
        radius: 3
        color: UI.colors.inactive

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            color: UI.colors.accent
            radius: 3
        }
    }

    handle: Item {
        id: _handle
        x: -(width - _handleButton.width)/2 + control.leftPadding + control.visualPosition * (control.availableWidth + (width - _handleButton.width) - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: UI.controlHeight
        implicitHeight: UI.controlHeight

        Rectangle {
            id: _highlight

            anchors.centerIn: _handle

            height: _handle.height/2
            width: _handle.height/2

            radius: height / 2
            color: UI.colors.accent
            opacity: 0.1

            states: [
                State {
                    when: control.pressed
                    name: "pressed"
                    PropertyChanges {
                        target: _highlight
                        scale: 1.5
                    }
                },
                State {
                    when: true
                    name: "released"
                    PropertyChanges {
                        target: _highlight
                        scale: 1
                    }
                }
            ]

            transitions: [
                Transition {
                    from: "released"
                    NumberAnimation {
                        target: _highlight
                        property: "scale"
                        duration: 150
                        easing.type: Easing.OutQuad
                    }
                },
                Transition {
                    from: "pressed"
                    NumberAnimation {
                        target: _highlight
                        property: "scale"
                        duration: 1200
                        easing.type: Easing.OutElastic
                    }
                }
            ]
        }
        Rectangle {
            id: _handleButton
            height: _handle.height/2
            width: _handle.height/2
            radius: 100
            anchors.centerIn: _handle
            color: UI.colors.accent // control.pressed ? "#f0f0f0" : "#f6f6f6"
            //border.color: "#bdbebf"
        }
    }
}
