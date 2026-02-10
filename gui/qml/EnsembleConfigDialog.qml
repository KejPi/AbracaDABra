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

AbracaDialog {
    id: dialog

    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    parent: Overlay.overlay
    width: parent.width * 0.9
    height: parent.height * 0.9

    modal: true
    title: qsTr("Ensemble Information")
    canEscape: true

    property alias ensembleText: ensEdit.text
    property int modelRow: -1
    property bool uploadCsvVisible: false
    property bool uploadCsvEnabled: false
    signal exportCsvRequest(sourceRow: int)
    signal uploadCsvRequest()

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        spacing: UI.standardMargin
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
        RowLayout {
            id: buttonLayout
            Layout.fillWidth: true
            spacing: UI.standardMargin
            Item { Layout.fillWidth: true }
            AbracaButton {
                id: uploadCSVButton
                Layout.fillWidth: buttonLayout.columns === 1
                text: qsTr("Upload to FMLIST")
                onClicked: uploadCsvRequest()
                visible: uploadCsvVisible
                enabled: uploadCsvEnabled
            }
            AbracaButton {
                id: exportCSVButton
                Layout.fillWidth: buttonLayout.columns === 1
                text: qsTr("Export as CSV")
                onClicked: exportCsvRequest(dialog.modelRow)
            }
        }
    }
}
