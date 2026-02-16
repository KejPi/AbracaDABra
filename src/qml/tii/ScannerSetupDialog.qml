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

    modal: true
    title: qsTr("Scanner configuration")
    canEscape: true

    height: mainLayout.implicitHeight + topPadding + bottomPadding + implicitHeaderHeight

    GridLayout {
        id: mainLayout
        anchors.fill: parent
        columnSpacing: 2*UI.standardMargin
        rowSpacing: UI.standardMargin
        columns: 2

        AbracaLabel {
            text: qsTr("Mode:")
        }

        AbracaComboBox {
            Layout.fillWidth: true
            model: ListModel {
                id: modeModel
                ListElement {
                    text: qsTr("Fast")
                    mode: 1 // ScannerBackend.ModeFast // ListElement: cannot use script for property value
                }
                ListElement {
                    text: qsTr("Normal")
                    mode: 2 // ScannerBackend.ModeNormal // ListElement: cannot use script for property value
                }
                ListElement {
                    text: qsTr("Precise")
                    mode: 4 // ScannerBackend.ModePrecise // ListElement: cannot use script for property value
                }
            }
            textRole: "text"
            valueRole: "mode"
            enabled: !scannerBackend.isScanning
            currentIndex: {
                switch (scannerBackend.mode) {
                    case 1: return 0;
                    case 2: return 1;
                    case 4: return 2;
                    default: return -1;
                }
            }
            onActivated: {
                scannerBackend.mode = valueAt(currentIndex);
            }
        }
        AbracaLabel {
            text: qsTr("Number of cycles:")
        }
        AbracaSpinBox {
            id: cyclesSpinBox
            Layout.fillWidth: true
            value: scannerBackend.numCycles
            onValueChanged: {
                if (scannerBackend.numCycles !== value) {
                    scannerBackend.numCycles = value
                }
            }
            stepSize: 1
            from: 0
            to: 99
            editable: true
            enabled: !scannerBackend.isScanning
            specialValue: 0
            specialValueString: qsTr("Inf")
        }
    }
}
