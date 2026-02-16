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
import QtQuick.Effects

import abracaComponents

Item {
    anchors.fill: parent
    property var tiiBackend: application.tiiBackend

    function storeSplitterState() {
        if (viewLoader.item) {
            tiiBackend.splitterState = viewLoader.item.saveState();
        }
    }
    Component.onCompleted: {
        if (viewLoader.item) {
            viewLoader.item.restoreState(tiiBackend.splitterState);
        }
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

    Loader {
        id: viewLoader
        anchors.fill: parent
        sourceComponent: UI.isMobile ? mobileView : desktopView
    }

    Component {
        id: desktopView
        AbracaSplitView {
            id: splitView
            anchors.fill: parent
            orientation: Qt.Vertical
            snapThreshold: 100
            minVisibleSize: 120

            TIIMap {
                id: mapView
                backend: tiiBackend
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumHeight: 250
            }
            ChartView {
                id: tiiSpectrumView
                SplitView.fillWidth: true
                SplitView.preferredHeight: 150
                SplitView.minimumHeight: 0
                visible: tiiBackend.showSpetrumPlot
                historyCapacity: 192
                dataMode: "replace"
                followTail: false
                majorTickStepX: 8
                minorSectionsX: 4
                yTickLabelWidth: 20
                majorTickStepY: 0.2
                minorSectionsY: 4
                showButton: true

                Component.onCompleted: {
                    tiiBackend.registerTiiSpectrumPlot(tiiSpectrumView.chart);
                }
                Component.onDestruction: {
                    tiiBackend.registerTiiSpectrumPlot(null);
                }
            }
        }
    }
    Component {
        id: mobileView
        AbracaSplitView {
            id: splitView
            anchors.fill: parent
            orientation: Qt.Vertical

            snapThreshold: 120
            minVisibleSize: 150

            handle: Rectangle {
                id: handleItem
                implicitWidth: splitView.handleWidth > 0 ? splitView.handleWidth : UI.standardMargin * 1.5
                implicitHeight: splitView.handleWidth > 0 ? splitView.handleWidth : UI.standardMargin * 1.5
                color: UI.colors.background

                readonly property color indicatorColor: UI.colors.inactive

                Rectangle {
                    id: topBorder
                    radius: 3*UI.controlRadius
                    width: handleItem.width
                    height: 1.5*radius
                    color: UI.colors.background
                    y: -height / 2
                    z: -1
                }
                MultiEffect {
                    z: -1
                    source: topBorder
                    anchors.fill: topBorder
                    autoPaddingEnabled: false
                    shadowBlur: 1.0
                    shadowColor: 'black'
                    shadowEnabled: true
                    shadowOpacity: 0.5
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: -3
                    visible: true
                    paddingRect: Qt.rect(30, 30, width, 0)
                }

                Rectangle {
                    width: UI.controlHeight
                    height: UI.standardMargin / 2
                    anchors.centerIn: topBorder
                    radius: UI.controlRadius
                    color: handleItem.indicatorColor
                }
                containmentMask: Item {
                    x: 0
                    y: (handleItem.height - height) / 2
                    width: handleItem.width
                    height: UI.controlHeight
                }

                SplitHandle.onPressedChanged: {
                    if (!SplitHandle.pressed) {
                        splitView.applySnapToZero()
                    }
                }
            }

            Item {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                SplitView.minimumHeight: 300
                TIIMap {
                    id: mapView
                    anchors.fill: parent
                    anchors.bottomMargin: 3*UI.controlRadius
                    backend: tiiBackend
                    showTable: false
                }
            }

            Rectangle {
                color: UI.colors.background
                SplitView.fillWidth: true
                SplitView.preferredHeight: UI.controlHeight/4
                SplitView.minimumHeight: UI.controlHeight/4

                SwipeView {
                    id: view
                    currentIndex: 0
                    anchors.fill: parent
                    anchors.topMargin: UI.controlHeight/4

                    property var tiiSpectrumPage: null
                    function addTiiSpectrumPage() {
                        if (!tiiSpectrumPage) {
                            tiiSpectrumPage = tiiSpectrumPageComponent.createObject(view.contentItem, { })
                            view.addItem(tiiSpectrumPage);
                        }
                    }
                    function removeTiiSpectrumPage() {
                        if (tiiSpectrumPage) {
                            view.removeItem(tiiSpectrumPage);
                            tiiSpectrumPage = null;
                        }
                    }
                    Component.onCompleted: {
                        if (tiiBackend.showSpetrumPlot) {
                            addTiiSpectrumPage();
                        }
                    }
                    Connections {
                        target: tiiBackend
                        function onShowSpetrumPlotChanged() {
                            if (tiiBackend.showSpetrumPlot) {
                                if (view.count === 1) {
                                    addSpectrumPage();
                                }
                            } else {
                                if (view.count === 2) {
                                    removeSpectrumPage();
                                }
                            }
                        }
                    }

                    AbracaTableView {
                        model: tiiBackend.tableModel
                        selectionModel: tiiBackend.tableSelectionModel
                        sortingEnabled: true
                        sortIndicatorColumn: 1
                        sortIndicatorOrder: Qt.DescendingOrder
                        visible: height > 90
                        width: SwipeView.view.width
                        height: SwipeView.view.height - pageIndicator.height - UI.standardMargin
                        Behavior on height {
                            SmoothedAnimation {
                                velocity: 1000
                            }
                        }
                    }
                    Component {
                        id: tiiSpectrumPageComponent
                        Item {
                            ChartView {
                                id: tiiSpectrumView
                                anchors.fill: parent
                                anchors.bottomMargin: pageIndicator.height + UI.standardMargin
                                visible: tiiBackend.showSpetrumPlot && height > 90
                                historyCapacity: 192
                                dataMode: "replace"
                                followTail: false
                                majorTickStepX: 8
                                minorSectionsX: 4
                                yTickLabelWidth: 20
                                majorTickStepY: 0.2
                                minorSectionsY: 4
                                showButton: true

                                Component.onCompleted: {
                                    tiiBackend.registerTiiSpectrumPlot(tiiSpectrumView.chart);
                                }
                                Component.onDestruction: {
                                    tiiBackend.registerTiiSpectrumPlot(null);
                                }
                            }
                        }
                    }
                }
                PageIndicator {
                    id: pageIndicator
                    count: view.count
                    currentIndex: view.currentIndex
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: view.height > 2*UI.controlHeight
                }
            }
        }
    }
}
