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
import abracaComponents 1.0


Item {
    id: mainItem
    // readonly property int minWidth: 320 /* slsView */ +
    readonly property int minWidth: 320 /* slsView */ + controlRow.implicitWidth + landscapeLayout.spacing + 2*UI.standardMargin
    readonly property int minHeight: 240 /* slsView */ + 2*UI.standardMargin

    property var catSlsBackend: application.catSlsBackend

    SLSView {
        id: slsView
        backend: catSlsBackend.slsBackend
        slsSource: "image://catSLS/" + catSlsBackend.slsBackend.slidePath
    }

    ListView {
        id: categoriesList

        clip: true
        model: catSlsBackend.categoriesModel
        currentIndex: catSlsBackend.categoriesModel.currentIndex

        delegate: AbracaItemDelegate {
            required property string catName
            required property int index

            implicitWidth: categoriesList.width
            height: 30
            text: catName
            highlighted: ListView.isCurrentItem
            onClicked: catSlsBackend.onCategoryViewClicked(index)
        }
    }

    RowLayout {
        id: controlRow

        AbracaImgButton {
            id: backButton
            Layout.preferredWidth: 50
            source: UI.imagesUrl + "chevron-left.svg"
            onClicked: catSlsBackend.onBackButtonClicked()
            enabled: catSlsBackend.isBackEnabled
        }

        AbracaLabel {
            text: catSlsBackend.slideCount
            horizontalAlignment: Text.AlignHCenter
            Layout.minimumWidth: 50
        }
        AbracaImgButton {
            id: fwdButton
            Layout.preferredWidth: 50
            source: UI.imagesUrl + "chevron-right.svg"
            onClicked: catSlsBackend.onFwdButtonClicked()
            enabled: catSlsBackend.isFwdEnabled
        }
    }

    //-------------------------------------------------------------------------
    // Page layout

    RowLayout {
        id: landscapeLayout
        anchors.fill: parent
        anchors.margins: UI.standardMargin

        LayoutItemProxy {
            target: slsView

            Layout.fillWidth: true
            Layout.preferredHeight: width / 4 * 3
            Layout.maximumWidth: 400
            Layout.minimumWidth: 320
            Layout.alignment: Qt.AlignCenter
            Layout.horizontalStretchFactor: 1
        }
        ColumnLayout {
            LayoutItemProxy {
                target: categoriesList
                Layout.preferredWidth: 200
                Layout.fillWidth:  true
                Layout.fillHeight: true
                Layout.minimumWidth: 150
                Layout.maximumWidth: 300
                Layout.horizontalStretchFactor: 1000
            }
            LayoutItemProxy {
                target: controlRow
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignCenter
                Layout.preferredHeight: controlRow.implicitHeight
            }
        }
    }

    ColumnLayout {
        id: portraitLayout
        anchors.fill: parent
        LayoutItemProxy {
            target: slsView
            Layout.fillWidth: true
            Layout.preferredHeight: width / 4 * 3
            Layout.maximumWidth: 400
            Layout.maximumHeight: 300
            Layout.alignment: Qt.AlignCenter
            //Layout.fillHeight: true
        }
        LayoutItemProxy {
            target: controlRow
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignCenter
            Layout.preferredHeight: controlRow.implicitHeight
        }
        LayoutItemProxy {
            target: categoriesList
            Layout.fillWidth:  true
            Layout.fillHeight: true
            Layout.preferredHeight: 200
        }
    }

    function setFittingLayout() {
        if (appUI.isPortraitView) {
            landscapeLayout.visible = false
            portraitLayout.visible = true
        } else {
            portraitLayout.visible = false
            landscapeLayout.visible = true
        }
    }
    onWidthChanged: setFittingLayout()
    Component.onCompleted: setFittingLayout()
}
