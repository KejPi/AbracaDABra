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
import QtQuick.Layouts
import QtQuick.Controls

import abracaComponents 1.0
Item {
    id: aboutDialog
    property var backend: null
    Flickable {
        id: contentFlickable
        anchors.fill: parent
        // anchors.margins: UI.standardMargin

        boundsBehavior: Flickable.StopAtBounds
        clip: true

        contentWidth: width
        contentHeight: aboutColumn.implicitHeight + 2*UI.standardMargin

        Item {
            width: parent.width
            ColumnLayout {
                id: aboutColumn
                //anchors.fill: parent
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: UI.standardMargin
                // width: parent.width - 2*UI.standardMargin
                spacing: UI.standardMargin

                RowLayout {
                    spacing: UI.standardMargin
                    Layout.fillWidth: true
                    Image {
                        id: appLogo
                        source: "qrc:/qt/qml/abracaComponents/resources/appIcon.png"
                        Layout.preferredHeight: 128
                        Layout.preferredWidth: 128
                        fillMode: Image.PreserveAspectFit
                    }
                    ColumnLayout {
                        AbracaLabel {
                            text: "Abraca DAB radio"
                            font.bold: true
                            font.pointSize: UI.largeFontPointSize
                        }
                        Repeater {
                            model: backend.appInfoStrings
                            AbracaLabel {
                                Layout.fillWidth: true
                                text: modelData
                                wrapMode: Text.WordWrap
                                textFormat: Text.StyledText
                            }
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: libText.implicitHeight
                    AbracaLabel {
                        id: libText
                        anchors.fill: parent
                        text: backend.libraries
                        textFormat: Text.StyledText
                        wrapMode: Text.WordWrap
                    }
                }
                AbracaTextArea {
                    id: disclaimerText
                    //anchors.fill: parent
                    //anchors.margins: UI.standardMargin
                    Layout.fillWidth: true
                    Layout.preferredHeight: disclaimerText.implicitHeight + 2*UI.standardMargin

                    text: backend.disclaimer
                    textFormat: TextEdit.RichText
                    wrapMode: Text.WordWrap
                    readOnly: true
                }
            }
        }
        ScrollIndicator.vertical: AbracaScrollIndicator { }
        ScrollIndicator.horizontal: AbracaScrollIndicator { }
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: parent.width
        topDownDirection: true
        shadowDistance: contentFlickable.contentY
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: parent.width
        topDownDirection: false
        shadowDistance: contentFlickable.contentHeight - contentFlickable.height - contentFlickable.contentY
    }
}
