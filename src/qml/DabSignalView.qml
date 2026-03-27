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

Rectangle {
    id: mainItem
    color: signalSpectrumView.backgroundColor
    anchors.fill: parent

    property var signalBackend: application.signalBackend

    function storeSplitterState() {
        signalBackend.splitterState = splitView.saveState();
    }
    Component.onCompleted: {
        splitView.restoreState(signalBackend.splitterState);
    }
    Component.onDestruction: {
        storeSplitterState();
    }
    Connections {
        target: appWindow
        function onClosing() {
            storeSplitterState();
        }
    }

    AbracaSplitView {
        id: splitView
        anchors.top: parent.top
        anchors.bottom: signalInfo.top
        anchors.left: parent.left
        anchors.right: parent.right

        orientation: Qt.Vertical
        snapThreshold: 100
        minVisibleSize: 120
        ChartView {
            id: signalSpectrumView

            SplitView.fillWidth: true
            SplitView.fillHeight: true
            SplitView.minimumHeight: 200

            historyCapacity: 2048
            dataMode: "replace"
            followTail: false

            majorTickStepX: 0.2
            minorSectionsX: 4
            majorTickStepY: 10
            minorSectionsY: 5

            // Hard limits: 0 to -150 dB
            chart.defaultYMin: -170  // Bottom of chart
            chart.defaultYMax: 0     // Top of chart
            chart.maxYSpan: 210
            chart.minYSpan: 10

            showButton: true

            Component.onCompleted: {
                mainItem.signalBackend.registerSpectrumPlot(signalSpectrumView.chart);
            }
            Component.onDestruction: {
                mainItem.signalBackend.registerSpectrumPlot(null);
            }
        }
        RowLayout {
            id: snrLayout
            SplitView.fillWidth: true
            SplitView.preferredHeight: 150
            SplitView.minimumHeight: 0
            ChartView {
                id: snrPlot
                Layout.fillWidth: true
                Layout.fillHeight: true

                dataMode: "append"
                followTail: false
                showButton: true
                Component.onCompleted: {
                    mainItem.signalBackend.registerSnrPlot(snrPlot.chart);
                }
                Component.onDestruction: {
                    mainItem.signalBackend.registerSnrPlot(null);
                }
            }
            AbracaLabel {
                id: snrValueLabel
                // Layout.minimumWidth: snrMetrics.boundingRect(" 36 dB").width
                Layout.preferredWidth: textMetrics.width * 1.5
                text: signalBackend.snrValue
                font.pointSize: UI.largeFontPointSize * 3
                font.bold: true
                color: signalSpectrumView.labelTextColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                visible: signalBackend.showSNR
                TextMetrics {
                    id: textMetrics
                    font: snrValueLabel.font
                    text: " 36 dB"
                }
            }
        }
    }
    Rectangle {
        id: signalInfo
        color: UI.colors.background
        height: signalInfoRowUndocked.visible ? signalInfoRowUndocked.implicitHeight
                                              : (signalInfoRow.visible ? signalInfoRow.implicitHeight
                                                                       : (signalInfoRowNoGain.visible ? signalInfoRowNoGain.implicitHeight
                                                                                                      : signalInfoGrid.implicitHeight + UI.standardMargin))
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property int minRowWidth: rfLevelLayout.implicitWidth + gainLayout.implicitWidth
                                            + frequencyLayout.implicitWidth + offsetLayout.implicitWidth + menuButton.implicitWidth
                                            + 8*UI.standardMargin

        function setFittingLayout() {
            if (signalBackend.isUndocked) {
                signalInfoRowUndocked.visible = true
                signalInfoRow.visible = false
                signalInfoGrid.visible = false
                signalInfoRowNoGain.visible = false
            }
            else {
                if (signalBackend.isRfLevelVisible === false) {
                    signalInfoRowUndocked.visible = false
                    signalInfoRow.visible = false
                    signalInfoGrid.visible = false
                    signalInfoRowNoGain.visible = true
                }
                else if (signalInfo.width > minRowWidth) {
                    signalInfoRowUndocked.visible = false
                    signalInfoRow.visible = true
                    signalInfoGrid.visible = false
                    signalInfoRowNoGain.visible = false
                }
                else {
                    signalInfoRowUndocked.visible = false
                    signalInfoRow.visible = false
                    signalInfoGrid.visible = true
                    signalInfoRowNoGain.visible = false
                }
            }
        }
        Component.onCompleted: Qt.callLater(signalInfo.setFittingLayout)
        onWidthChanged: Qt.callLater(signalInfo.setFittingLayout)
        Connections {
            target: signalBackend
            function onIsUndockedChanged() {
                Qt.callLater(signalInfo.setFittingLayout)
            }
             function onIsRfLevelVisibleChanged() {
                Qt.callLater(signalInfo.setFittingLayout)
            }
        }

        FontMetrics {
            id: fontMetrics
            font: frequencyLabel.font
        }

        // items
        SignalState {
            id: signalState
            showSnrLabel: true
        }
        RowLayout {
            id: rfLevelLayout
            AbracaLabel {
                id: rfLevelLabel
                text: qsTr("RF level ")
                toolTipText: qsTr("Estimated RF level")
                role: UI.LabelRole.Secondary
                Layout.fillWidth: true
                visible: signalBackend.isRfLevelVisible
            }
            AbracaLabel {
                Layout.preferredWidth: fontMetrics.boundingRect("-100.0 dBm").width
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: signalBackend.rfLevelLabel
                toolTipText: rfLevelLabel.toolTipText
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            id: gainLayout
            AbracaLabel {
                id: gainLabel
                text: qsTr("Gain ")
                toolTipText: qsTr("Tuner gain")
                Layout.fillWidth: true
                role: UI.LabelRole.Secondary
                visible: signalBackend.isRfLevelVisible
            }
            AbracaLabel {
                Layout.preferredWidth: fontMetrics.boundingRect(" 88 dB").width
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: signalBackend.gainLabel
                toolTipText: gainLabel.toolTipText
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            id: frequencyLayout
            AbracaLabel {
                id: frequencyLabel
                text: qsTr("Frequency ")
                toolTipText: qsTr("Tuned frequency")
                Layout.fillWidth: true
                role: UI.LabelRole.Secondary
            }
            AbracaLabel {
                Layout.preferredWidth: fontMetrics.boundingRect(" 227360 kHz").width
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: signalBackend.frequencyLabel
                toolTipText: frequencyLabel.toolTipText
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            id: offsetLayout
            AbracaLabel {
                id: offsetLabel
                role: UI.LabelRole.Secondary
                text: qsTr("Offset ")
                toolTipText: qsTr("Estimated frequency offset")
                Layout.fillWidth: true
            }
            AbracaLabel {
                id: offset
                Layout.preferredWidth: fontMetrics.boundingRect("-12345.5 Hz").width
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: signalBackend.frequencyOffsetLabel
                toolTipText: offsetLabel.toolTipText
                horizontalAlignment: Text.AlignRight
            }
        }

        AbracaImgButton {
            id: menuButton
            source: UI.imagesUrl + "icon-config.svg"
            onClicked: optionsMenu.open()

            AbracaMenu {
                id: optionsMenu
                y: menuButton.height
                width: {
                    var result = 0;
                    var padding = 0;
                    for (var i = 0; i < count; ++i) {
                        var item = itemAt(i);
                        result = Math.max(item.contentItem.implicitWidth, result);
                        padding = Math.max(item.padding, padding);
                    }
                    return result + padding * 2;
                }
                AbracaMenuItem {
                    text: qsTr("Show NULL spectrum")
                    checkable: true
                    checked: signalBackend.showNULL
                    onTriggered: signalBackend.showNULL = checked
                }
                AbracaMenuSeparator {}
                AbracaMenuItem {
                    text: qsTr("Slow update (1 sec)")
                    checkable: true
                    checked: signalBackend.spectrumUpdate === SignalBackend.SpectrumUpdateSlow
                    onTriggered: signalBackend.spectrumUpdate = SignalBackend.SpectrumUpdateSlow
                }
                AbracaMenuItem {
                    text: qsTr("Normal update (600 msec)")
                    checkable: true
                    checked: signalBackend.spectrumUpdate === SignalBackend.SpectrumUpdateNormal
                    onTriggered: signalBackend.spectrumUpdate = SignalBackend.SpectrumUpdateNormal
                }
                AbracaMenuItem {
                    text: qsTr("Fast update (400 msec)")
                    checkable: true
                    checked: signalBackend.spectrumUpdate === SignalBackend.SpectrumUpdateFast
                    onTriggered: signalBackend.spectrumUpdate = SignalBackend.SpectrumUpdateFast
                }
                AbracaMenuItem {
                    text: qsTr("Very fast update (200 msec)")
                    checkable: true
                    checked: signalBackend.spectrumUpdate === SignalBackend.SpectrumUpdateVeryFast
                    onTriggered: signalBackend.spectrumUpdate = SignalBackend.SpectrumUpdateVeryFast
                }
                AbracaMenuSeparator {}
                AbracaMenuItem {
                    text: qsTr("Show SNR value")
                    checkable: true
                    checked: signalBackend.showSNR
                    onTriggered: signalBackend.showSNR = checked
                }
            }
        }

        GridLayout {
            id: signalInfoGrid  // narrow view (not in undocked state)
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: UI.standardMargin
            anchors.rightMargin: UI.standardMargin
            rowSpacing: 0
            rows: 2
            flow: GridLayout.TopToBottom

            LayoutItemProxy {
                target: rfLevelLayout
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                target: gainLayout
                Layout.fillWidth: true
            }
            Item {
                Layout.fillWidth: true
                Layout.rowSpan: 2
                Layout.horizontalStretchFactor: 1000
            }
            LayoutItemProxy {
                target: frequencyLayout
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                target: offsetLayout
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                Layout.rowSpan: 2
                target: menuButton
            }
        }
        RowLayout {
            id: signalInfoRow
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: UI.standardMargin
            anchors.rightMargin: UI.standardMargin

            LayoutItemProxy {
                target: rfLevelLayout
                Layout.rightMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                Layout.leftMargin: UI.standardMargin / 2
                target: gainLayout
            }
            Item {
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                target: frequencyLayout
                Layout.leftMargin: UI.standardMargin / 2
                Layout.rightMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: offsetLayout
                Layout.leftMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: menuButton
                Layout.leftMargin: 0
            }
        }
        RowLayout {
            id: signalInfoRowNoGain
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: UI.standardMargin
            anchors.rightMargin: UI.standardMargin

            Item {
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                target: frequencyLayout
                Layout.leftMargin: UI.standardMargin / 2
                Layout.rightMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: offsetLayout
                Layout.leftMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: menuButton
                Layout.leftMargin: 0
            }
        }
        RowLayout {
            id: signalInfoRowUndocked
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: UI.standardMargin
            anchors.rightMargin: UI.standardMargin

            LayoutItemProxy {
                target: signalState
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: UI.standardMargin
            }
            LayoutItemProxy {
                target: rfLevelLayout
                Layout.rightMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                Layout.leftMargin: UI.standardMargin / 2
                target: gainLayout
            }
            Item {
                Layout.fillWidth: true
            }
            LayoutItemProxy {
                target: frequencyLayout
                Layout.leftMargin: UI.standardMargin / 2
                Layout.rightMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: offsetLayout
                Layout.leftMargin: UI.standardMargin / 2
            }
            LayoutItemProxy {
                target: menuButton
                Layout.leftMargin: 0
            }
        }
    }
}
