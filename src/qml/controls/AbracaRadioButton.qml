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

RadioButton {
    id: control

    text: "RadioButton"
    checked: true

    property int elideMode: Text.ElideRight
    property int wrapMode: Text.NoWrap

    // implicitWidth: UI.controlHeightSmall
    implicitHeight: UI.controlHeight

    indicator: Rectangle {
        id: _radioButton
        height: UI.controlHeightSmall
        width: height
        x: control.leftPadding
        y: (parent.height - height) / 2
        radius: 100
        color: "transparent"
        border.width: 2
        border.color: control.checked ? UI.colors.accent : UI.colors.inactive
        opacity: control.enabled ? 1.0 : 0.5

        states: [
            State {
                when: control.pressed
                name: "pressed"
                PropertyChanges {
                    target: _radioButton
                    scale: 1.2
                }
            },
            State {
                when: control.released
                name: "released"
                PropertyChanges {
                    target: _radioButton
                    scale: 1
                }
            }
        ]

        transitions: [
            //scale elastic animation
            Transition {
                from: "released"
                NumberAnimation {
                    target: _radioButton
                    property: "scale"
                    duration: 200
                    easing.type: Easing.OutQuad
                }
            },
            Transition {
                from: "pressed"
                NumberAnimation {
                    target: _radioButton
                    property: "scale"
                    duration: 1200
                    easing.type: Easing.OutElastic
                }
            }
        ]

        Rectangle {
            id: _innerCircle
            anchors{
                fill: _radioButton
                margins: _radioButton.height * 0.28
            }

            radius: _radioButton.radius
            color: UI.colors.accent
            visible: control.checked
            // opacity: control.enabled ? 1.0 : 0.3
            states: [
                State {
                    when: control.checked
                    name: "checked"
                    PropertyChanges {
                        target: _innerCircle
                        scale: 1
                    }
                },
                State {
                    when: true
                    name: "unchecked"
                    PropertyChanges {
                        target: _innerCircle
                        scale: 0
                    }
                }
            ]

            transitions: [
                //scale elastic animation
                Transition {
                    from: "unchecked"
                    NumberAnimation {
                        target: _innerCircle
                        property: "scale"
                        duration: 150
                        easing.type: Easing.OutQuad
                    }
                },
                Transition {
                    from: "checked"
                    NumberAnimation {
                        target: _innerCircle
                        property: "scale"
                        duration: 150
                        easing.type: Easing.OutQuad
                    }
                }
            ]
        }
    }

    contentItem: AbracaLabel {
        text: control.text
        enabled: control.enabled
        // opacity: enabled ? 1.0 : 0.6
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        elide: control.elideMode
        wrapMode: control.wrapMode
    }
}
