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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import abracaComponents 1.0

Item {
    id: mainItem
    implicitHeight: contentLayout.implicitHeight + 2*UI.standardMargin
    implicitWidth: contentLayout.implicitWidth + 2*UI.standardMargin
    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 2*UI.standardMargin
        RowLayout {
            Layout.fillWidth: true
            AbracaLabel {
                text: qsTr("IP address:")
            }
            AbracaTextField {
                id: ipAddressField
                Layout.fillWidth: true
                placeholderText: qsTr("IP address of RaRT-TCP server")
                readonly property string ipRange: "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])"
                readonly property regexp ipRegex: new RegExp("^" + ipRange + "(\\." + ipRange + "){3}$")
                validator: RegularExpressionValidator {
                    regularExpression: ipAddressField.ipRegex
                }
                text: settingsBackend.rartTcpIpAddress
                onEditingFinished: if (settingsBackend.rartTcpIpAddress !== text) {
                    settingsBackend.rartTcpIpAddress = text
                }
            }
            AbracaLabel {
                text: qsTr("Port:")
            }
            AbracaSpinBox {
                from: 1000
                to: 99999
                locale: Qt.locale("C")
                value: settingsBackend.rartTcpPort
                onValueChanged: if (settingsBackend.rartTcpPort !== value) {
                    settingsBackend.rartTcpPort = value;
                }
                editable: true
            }
        }
    }
}
