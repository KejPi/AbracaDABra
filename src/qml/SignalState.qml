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

Item {
    id: mainItem
    property bool showSnrLabel: false
    implicitHeight: snrLabel.implicitHeight
    implicitWidth: sigStatusLayout.implicitWidth
    RowLayout {
        id: sigStatusLayout
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        spacing: 10
        Rectangle {
            readonly property color sigColor: {
                switch (appUI.signalQualityLevel) {
                    case 1:
                    case 4:
                        return UI.colors.lowSignal
                    case 2:
                    case 5:
                        return UI.colors.midSignal
                    case 3:
                    case 6:
                        return UI.colors.goodSignal
                    default: return UI.colors.noSignal
                }
            }

            color: appUI.signalQualityLevel > 3 ? sigColor : UI.colors.noSignal
            Layout.preferredHeight: mainItem.height
            Layout.preferredWidth: mainItem.height
            Layout.alignment: Qt.AlignVCenter
            radius: mainItem.height / 2
            border.width: 3
            border.color: sigColor

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                hoverEnabled: true
            }
            AbracaToolTip {
                text: appUI.signalQualityLevel === 0 ?
                        qsTr("DAB signal not detected<br>Looking for signal...")
                            : (appUI.signalQualityLevel > 3 ?
                                  qsTr("Synchronized to DAB signal")
                                : qsTr("Found DAB signal,<br>trying to synchronize...")
                            )
                hoverMouseArea: hoverArea
            }
        }
        AbracaLabel {
            text: qsTr("SNR")
            role: UI.LabelRole.Secondary
            visible: mainItem.showSnrLabel
        }
        AbracaLabel {
            id: snrLabel
            text: appUI.snrLabel
            toolTipText: qsTr("DAB signal SNR")
            //visible: appUI.signalQualityLevel > 0
            Layout.preferredWidth: metrics.width
            horizontalAlignment: Text.AlignRight
            TextMetrics {
                id: metrics
                font: snrLabel.font
                text: "36.0 dB"
            }
        }
    }
}
