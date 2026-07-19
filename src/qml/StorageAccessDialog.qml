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
import QtQuick.Controls
import QtQuick.Layouts

import abracaComponents

// Explanatory step shown before the system folder picker when a write to the data storage
// folder fails because Android SAF permission has not been granted yet. Accepting opens the
// folder picker (handled by the caller); rejecting cancels the whole flow.
AbracaDialog {
    id: dialog

    modal: true
    canEscape: true
    title: qsTr("Storage access needed")

    width: Math.min(appWindow.width - 2*UI.standardMargin, defaultWidth + 4*dialog.leftPadding)
    height: Math.min(implicitHeight, parent.height * 0.9)

    readonly property int defaultWidth: (cancelButton.implicitWidth + confirmButton.implicitWidth + UI.standardMargin * 2)

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        spacing: UI.standardMargin

        AbracaLabel {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("AbracaDABra needs permission to store data.\nPlease select data storage folder and grant permissions.")
        }

        RowLayout {
            id: buttonRow
            Layout.fillWidth: true
            Layout.topMargin: UI.standardMargin
            spacing: UI.standardMargin
            Item { Layout.fillWidth: true }
            AbracaButton {
                id: cancelButton
                text: qsTr("Cancel")
                onClicked: dialog.reject()
            }
            AbracaButton {
                id: confirmButton
                buttonRole: UI.ButtonRole.Primary
                text: qsTr("Choose folder...")
                onClicked: dialog.accept()
            }
        }
    }
}
