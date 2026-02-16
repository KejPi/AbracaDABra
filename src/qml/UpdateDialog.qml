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

import abracaComponents

AbracaDialog {
    id: updateDialog

    x: (appWindow.width - width) / 2
    y: (appWindow.height - height) / 2
    width: Math.min(appWindow.width - 2*UI.standardMargin, defaultWidth + 4*updateDialog.leftPadding)
    canEscape: true

    modal: true
    title: qsTr("AbracaDABra update available")

    readonly property int defaultWidth: (doNotShowAgainButton.implicitWidth + goToReleasePageButton.implicitWidth + closeButton.implicitWidth + UI.standardMargin * 3)

    implicitWidth: mainLayout.implicitWidth + 2*updateDialog.leftPadding

    ColumnLayout {
        id: mainLayout
        spacing: UI.standardMargin
        anchors.fill: parent

        RowLayout {
            spacing: UI.standardMargin
            Layout.fillWidth: true
            Image {
                id: appLogo
                source: "qrc:/qt/qml/abracaComponents/resources/appIcon.png"
                Layout.preferredHeight: 64
                Layout.preferredWidth: 64
                fillMode: Image.PreserveAspectFit
            }
            GridLayout {
                columns: 2
                AbracaLabel {
                    text: qsTr("Current version:")
                }
                AbracaLabel {
                    text: appUI.updateVersionInfo[0]
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                AbracaLabel {
                    text: qsTr("Available version:")
                }
                AbracaLabel {
                    text: appUI.updateVersionInfo[1]
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
        AbracaTextArea {
            id: changelogText
            Layout.fillWidth: true
            Layout.preferredHeight: changelogText.implicitHeight + 2*UI.standardMargin
            Layout.minimumHeight: 200
            text: appUI.updateVersionInfo[2]
            textFormat: TextEdit.MarkdownText
            wrapMode: Text.WordWrap
            readOnly: true
        }
        GridLayout {
            id: buttonLayout
            Layout.fillWidth: true
            columnSpacing: UI.standardMargin
            rowSpacing: UI.standardMargin

            columns: defaultWidth > mainLayout.width ? 1 : 4
            AbracaButton {
                id: doNotShowAgainButton
                Layout.fillWidth: buttonLayout.columns === 1
                text: qsTr("Do not show again")
                onClicked: {
                    updateDialog.reject()
                }
            }
            Item { Layout.fillWidth: true; visible: buttonLayout.columns > 1 }
            AbracaButton {
                id: goToReleasePageButton
                Layout.fillWidth: buttonLayout.columns === 1
                text: qsTr("Go to release page")
                onClicked: {
                    application.openLink(("https://github.com/KejPi/AbracaDABra/releases/tag/%1").arg(appUI.updateVersionInfo[1]))
                    updateDialog.close()
                }
            }
            AbracaButton {
                id: closeButton
                Layout.fillWidth: buttonLayout.columns === 1
                buttonRole: UI.ButtonRole.Primary
                text: qsTr("Close")
                onClicked: {
                    updateDialog.accept()
                }
            }
        }
    }
}

