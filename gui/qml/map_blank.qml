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
import Qt5Compat.GraphicalEffects
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
                id: blankMap
                source: "resources/Europe_blank_laea_location_map.svg"
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                visible: false
            }
            Desaturate {
                anchors.fill: blankMap
                source: blankMap
                desaturation: 0.8
                opacity: 0.40
            }
            Rectangle {
                color: "white"
                anchors.centerIn: parent
                width: warningText.width + 20
                height: warningText.height + 20
                Text {
                    id: warningText
                    anchors.centerIn: parent
                    //font.bold: true
                    font.pointSize: 20
                    text: qsTr("Displaying transmitters' locations requires Qt version 6.5.0 or newer.")
                }
            }
            Rectangle {
                id: infoBox

                HoverHandler {
                    id: infoHoverHandler
                }

                color: "white"
                opacity: infoHoverHandler.hovered ? 1.0 : 0.6
                width: infoLayout.width + 10
                height: infoLayout.height + 10
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 10
                anchors.topMargin: 10
                visible: tii.ensembleInfo[0] !== ""
                z: 3

                ColumnLayout {
                    id: infoLayout
                    anchors.centerIn: parent
                    Repeater {
                        model: tii.ensembleInfo
                        delegate: Text {
                            Layout.fillWidth: true
                            text: modelData
                        }
                    }
                }
            }

            Rectangle {
                id: txInfoBox

                HoverHandler {
                    id: txInfoHoverHandler
                }

                color: "white"
                opacity: txInfoHoverHandler.hovered ? 1.0 : 0.6
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 10
                anchors.bottomMargin: 10
                width: txInfoLayout.width + 10
                height: txInfoLayout.height + 10

                visible: tii.txInfo.length > 0
                z: 3
                ColumnLayout {
                    id: txInfoLayout
                    anchors.centerIn: parent
                    Repeater {
                        model: tii.txInfo
                        delegate: Text {
                            Layout.fillWidth: true
                            text: modelData
                        }
                    }
                }
            }
        }
    }
}
