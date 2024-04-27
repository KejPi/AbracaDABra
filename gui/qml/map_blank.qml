/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

import QtCore
import QtQuick
import QtQuick.Layouts
import app.qmlcomponents 1.0

Item {
    anchors.fill: parent

    Loader {
        anchors.fill: parent
        active: tii.isVisible
        visible: active
        sourceComponent: mainComponent        
    }

    Component {
        id: mainComponent
        Item {
            Image {
                source: "qrc:/resources/Europe_blank_laea_location_map.svg"
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }
            Rectangle {
                color: "white"
                anchors.centerIn: parent
                width: warningText.width + 20
                height: warningText.height + 20
                Text {
                    id: warningText
                    anchors.centerIn: parent
                    font.bold: true
                    font.pointSize: 20
                    text: qsTr("Map requires application to be built with Qt version 6.5.0 or newer.")
                }
            }
        }
    }
}
