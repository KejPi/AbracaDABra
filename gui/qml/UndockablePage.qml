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
    id: root

    property string pageId: ""
    property string title: "Undockable Page"
    property bool isUndocked: false
    property alias container: pageContainer
    property var content: null
    property int minWidth: 640
    property int minHeight: 480

    anchors.fill: parent

    Component.onCompleted: {
        //if (appUI.isPortraitView === false) {
            isUndocked = isUndockedPage(root.pageId)
            if (!isUndocked && content) {
                content.parent = pageContainer
            }
        //}
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ServicePanel {
            id: servicePanel
            showDockingControls: UI.isAndroid === false
            pageIsUndocked: isUndocked

            visible: UI.isMobile === false && appUI.isPortraitView === false
            Layout.preferredHeight: implicitHeight
            Layout.fillWidth: true

            onControlClicked: {
                if (isUndocked) {
                    dockPage(root.pageId);
                } else {
                    undockPage(root.pageId);
                }
            }
        }


        // Content area or undocked message
        Item {
            id: pageContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Undocked placeholder
            Rectangle {
                anchors.margins: 10
                anchors.fill: parent
                visible: root.isUndocked
                color: UI.colors.backgroundLight // "#f5f5f5"
                border.color: UI.colors.textDisabled // "#ddd"
                border.width: 2
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 15

                    AbracaColorizedImage {
                        Layout.alignment: Qt.AlignHCenter
                        source: UI.imagesUrl + "placeholder-undocked.svg"
                        colorizationColor: UI.colors.textDisabled
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 160
                        sourceSize.width: width
                        sourceSize.height: height
                        opacity: 0.5
                    }
                    AbracaLabel {
                        text: qsTr("Page is currently undocked")
                        color: UI.colors.textSecondary
                        font.pixelSize: UI.largeFontPointSize
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter                    
                    }

                    AbracaLabel {
                        text: qsTr("The content is displayed in a separate window.\nUse the 'Dock Page' button above to bring it back.")
                        // color: UI.colors.textDisabled
                        color: UI.colors.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }
    }
}
