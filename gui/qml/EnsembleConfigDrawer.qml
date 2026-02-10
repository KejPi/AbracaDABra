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
import QtQuick.Layouts

import abracaComponents

AbracaDrawer {
    id: drawer

    property alias ensembleText: ensEdit.text
    property int modelRow: -1
    property bool uploadCsvVisible: false
    property bool uploadCsvEnabled: false
    signal exportCsvRequest(sourceRow: int)
    signal uploadCsvRequest()

    width: parent.width
    height: parent.height * 0.95
    edge: Qt.BottomEdge

    readonly property bool buttonsOnTop: drawer.width > buttonLayout.implicitWidth
                                                + titleLabel.implicitWidth
                                                + closeButton.implicitWidth
                                                + 4*UI.standardMargin

    RowLayout {
        id: buttonLayout
        Layout.fillWidth: true
        spacing: UI.standardMargin
        AbracaButton {
            id: uploadCSVButton
            Layout.fillWidth: drawer.buttonsOnTop === false
            Layout.preferredWidth: Math.max(exportCSVButton.implicitWidth, uploadCSVButton.implicitWidth)
            text: qsTr("Upload to FMLIST")
            onClicked: uploadCsvRequest()
            visible: uploadCsvVisible
            enabled: uploadCsvEnabled
        }
        AbracaButton {
            id: exportCSVButton
            Layout.fillWidth: drawer.buttonsOnTop === false
            Layout.preferredWidth: Math.max(exportCSVButton.implicitWidth, uploadCSVButton.implicitWidth)
            text: qsTr("Export as CSV")
            onClicked: exportCsvRequest(drawer.modelRow)
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.margins: UI.standardMargin
        spacing: UI.standardMargin

        RowLayout {
            id: titleLayout
            Layout.fillWidth: true
            Layout.margins: UI.standardMargin
            Label {
                id: titleLabel
                text: qsTr("Ensemble Information")
                font.bold: true
                font.pointSize: UI.largeFontPointSize
                color: UI.colors.textPrimary
                horizontalAlignment: Text.AlignLeft
            }
            Item {
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                visible: drawer.buttonsOnTop
                target: buttonLayout
            }
            AbracaImgButton {
                id: closeButton
                source: UI.imagesUrl + "close.svg"
                // anchors.verticalCenter: titleLabel.verticalCenter
                // anchors.right: titleLabel.right
                // anchors.rightMargin: -2*UI.standardMargin
                Layout.rightMargin: -2*UI.standardMargin
                colorizationColor: UI.colors.iconInactive
                onClicked: {
                    console.log("Closing ensemble info drawer")
                    close()
                }
            }
        }
        Item {
            id: ensTextItem
            Layout.fillHeight: true
            Layout.fillWidth: true
            AbracaScrollView {
                id: textEditFlickable
                anchors.fill: parent
                AbracaTextArea {
                    id: ensEdit
                    width: textEditFlickable.width
                    textFormat: TextEdit.RichText
                    readOnly: true
                }
            }
        }
        LayoutItemProxy {
            target: buttonLayout
            visible: drawer.buttonsOnTop === false
            Layout.fillWidth: true
        }
    }
}
