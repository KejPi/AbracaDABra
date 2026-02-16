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

ColumnLayout {
    id: mainItem

    property int barHeight: 20
    property bool showLegend: false

    implicitWidth: 400

    Item {
        id: bar
        Layout.preferredHeight: mainItem.barHeight
        Layout.fillWidth: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height / 5
            color: UI.colors.textDisabled
        }

        Row {
            Repeater {
                model: ensembleInfo.ensembleSubchModel
                delegate: Rectangle {
                    required property bool isEmpty
                    required property bool isAudio
                    required property bool isSelected
                    required property color subchColor
                    required property int subchId
                    required property real relativeWidth
                    required property int index

                    z: 10
                    height: bar.height
                    width: bar.width * relativeWidth
                    color: subchColor

                    border.width: isSelected ? 2 : 1
                    border.color: isEmpty ? "transparent" : (isSelected ? UI.colors.textPrimary : UI.colors.background)

                    Image {
                        visible: !isEmpty && !isAudio
                        anchors.fill: parent
                        anchors.margins: parent.border.width
                        source: UI.imagesUrl + "diagonal-line-pattern.svg"
                        //sourceSize.height: 10
                        //sourceSize.width: 10
                        fillMode: Image.Tile
                        horizontalAlignment: Image.AlignLeft
                        verticalAlignment: Image.AlignTop
                    }

                   AbracaToolTip {
                        text: qsTr("Subchannel ") + subchId
                        hoverMouseArea: mouseArea
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        enabled: showLegend === false && isEmpty === false
                        hoverEnabled: showLegend === false && isEmpty === false
                        acceptedButtons: Qt.LeftButton
                        cursorShape: isEmpty || showLegend ? Qt.ArrowCursor : Qt.PointingHandCursor
                        onClicked: ensembleInfo.ensembleSubchModel.currentRow = index
                    }
                }
            }
        }
    }
    GridLayout {
        id: legend
        visible: showLegend
        Layout.fillWidth: true

        rows: (ensembleInfo.ensembleSubchModel.rowCount + 1) / 2
        flow: GridLayout.TopToBottom
        Repeater {
            id: legendRepeater
            //model: ensembleInfo.ensembleSubchModel
            model: EnsembleNonEmptySubchModel {
                id: nonEmptySubchModel
                sourceModel: ensembleInfo.ensembleSubchModel
            }
            delegate: Rectangle {
                id: legendItem
                Layout.fillWidth: true
                Layout.preferredHeight: UI.controlHeight

                color: isSelected ? UI.colors.highlight : "transparent"
                radius: UI.controlRadius
                border.width: 1
                border.color: isSelected ? UI.colors.accent : UI.colors.inactive

                required property bool isEmpty
                required property bool isSelected
                required property color subchColor
                required property int subchId
                required property int index
                required property string servicesShort

                Rectangle {
                    id: colorBox
                    anchors.left: parent.left
                    anchors.leftMargin: UI.standardMargin
                    anchors.verticalCenter: parent.verticalCenter
                    height: legendItem.height - 2*UI.standardMargin
                    width: height
                    radius: UI.controlRadius
                    color: subchColor
                    visible: isEmpty === false
                    // border.color: UI.colors.textPrimary
                    //border.width: isSelected ? 2 : 0
                }
                AbracaLabel {
                    id: subchLabel
                    anchors.left: colorBox.right
                    anchors.leftMargin: UI.standardMargin
                    anchors.right: subchIdLabel.left
                    anchors.rightMargin: UI.standardMargin
                    anchors.verticalCenter: parent.verticalCenter
                    // text: subchId + ": " + servicesShort
                    text: servicesShort
                    visible: isEmpty === false
                    color: isSelected ? UI.colors.listItemSelected : UI.colors.textPrimary
                    elide: Text.ElideRight
                    font.bold: isSelected
                }
                AbracaLabel {
                    id: subchIdLabel
                    anchors.right: legendItem.right
                    anchors.rightMargin: UI.standardMargin
                    anchors.verticalCenter: parent.verticalCenter
                    // text: subchId + ": " + servicesShort
                    text: subchId
                    visible: isEmpty === false
                    color: UI.colors.textSecondary
                }
                MouseArea {
                    anchors.fill: legendItem
                    enabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor
                    onClicked: legendRepeater.model.selectSubCh(index) // ensembleInfo.ensembleSubchModel.currentRow = index
                }
            }
        }
    }
}


