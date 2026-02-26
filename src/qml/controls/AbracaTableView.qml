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
import QtQuick.Controls.Basic
import QtQuick.Layouts

import abracaComponents

Item {
    id: tableItem

    objectName: "tableItem"
    property alias model: tableView.model
    property alias selectionModel: tableView.selectionModel
    property alias contextMenuModel: contextMenu.menuModel
    property int shrinkColumnIndex: -1
    property int minColumnWidth: 30
    property int maxColumnWidth: 0
    property bool sortingEnabled: false
    property bool cellsLeftAligned: true
    property int preferedWidth: 100

    signal doubleClickedRow(int row)
    signal populateContextMenu(int row)

    FontMetrics {
        id: fontMetrics
    }
    readonly property int rowHeight: fontMetrics.font.pointSize + 10
    readonly property bool contextMenuEnabled: contextMenuModel !== undefined

    property var columnWidths: []
    property var minColumnWidths: []

    // column widths are computed automatically by autoAdjustColumns()
    // keep sort state on root to avoid delegate scoping issues
    property int sortIndicatorColumn: 0
    property int sortIndicatorOrder: Qt.AscendingOrder

    // when width changes, columns are adjusted automatically
    onWidthChanged: {
        tableItem.autoAdjustColumns();
    }
    onHeightChanged: {
        // scrollbar can disappear/appear when height changes
        tableItem.autoAdjustColumns();
    }

    function calculatePreferedWidth() {
        // compute preferred widths from headers/content
        var totalPref = 0;          // sum of preferred widths

        var cols = tableView.model ? tableView.model.columnCount() : 0;

        if (cols === 0)
            return;

        for (var c = 0; c < cols; ++c) {
            // header width
            var header = tableView.model.headerData(c, Qt.Horizontal, Qt.DisplayRole);
            var hdrw = Math.ceil(fontMetrics.boundingRect(header).width);
            // define minimum width per column as header width + small padding
            var minw = Math.max(tableItem.minColumnWidth, hdrw + 2*UI.standardMargin + fontMetrics.font.pointSize * 0.5);
            var maxw = tableView.maxColumnWidth > 0 ? Math.min(hdrw, tableView.maxColumnWidth) : hdrw;
            // measure content up to sampleLimit rows
            var rows = tableView.model.rowCount;
            var sampleLimit = Math.min(rows, 200);
            for (var r = 0; r < sampleLimit; ++r) {
                var idx = tableView.model.index(r, c);
                var val = tableView.model.data ? tableView.model.data(idx, Qt.DisplayRole) : undefined;
                if (val !== undefined && val !== null) {
                    var s = String(val);
                    var w = Math.ceil(fontMetrics.boundingRect(s).width);
                    if (tableView.maxColumnWidth > 0) {
                        w = Math.min(w, tableView.maxColumnWidth)
                    }
                    if (w > maxw) {
                        maxw = w;
                    }
                }
            }
            // add padding depending on column (first columns may need less)
            totalPref += Math.max(minw, maxw + 2*UI.standardMargin);
        }
        if (tableItem.preferedWidth !== totalPref) {
            tableItem.preferedWidth = totalPref;
        }
    }

    // Call from C++: find the QML object and invoke this method to auto-adjust columns
    function autoAdjustColumns() {
        // compute preferred widths from headers/content
        var totalPref = 0;          // sum of preferred widths
        var pref = [];              // array of preferred widths for each column (these are calculated from header and contents)
        var totalMin = 0;           // sum of header widths
        var minColumnWidths = [];   // array of minimum widths for the columns defined by header

        var cols = tableView.model ? tableView.model.columnCount() : 0;

        if (cols === 0)
            return;

        for (var c = 0; c < cols; ++c) {
            // header width
            var header = tableView.model.headerData(c, Qt.Horizontal, Qt.DisplayRole);
            var hdrw = Math.ceil(fontMetrics.boundingRect(header).width);
            // define minimum width per column as header width + small padding
            var minw = Math.max(tableItem.minColumnWidth, hdrw + 2*UI.standardMargin + fontMetrics.font.pointSize * 0.5);
            minColumnWidths[c] = minw;
            totalMin += minw;
            var maxw = tableView.maxColumnWidth > 0 ? Math.min(hdrw, tableView.maxColumnWidth) : hdrw;
            // measure content up to sampleLimit rows
            var rows = tableView.model.rowCount;
            var sampleLimit = Math.min(rows, 200);
            for (var r = 0; r < sampleLimit; ++r) {
                var idx = tableView.model.index(r, c);
                var val = tableView.model.data ? tableView.model.data(idx, Qt.DisplayRole) : undefined;
                if (val !== undefined && val !== null) {
                    var s = String(val);
                    var w = Math.ceil(fontMetrics.boundingRect(s).width);
                    if (tableView.maxColumnWidth > 0) {
                        w = Math.min(w, tableView.maxColumnWidth)
                    }
                    if (w > maxw) {
                        maxw = w;
                    }
                }
            }
            // add padding depending on column (first columns may need less)
            pref[c] = Math.max(minw, maxw + 2*UI.standardMargin);
            totalPref += pref[c];
        }

        // If a vertical scrollbar is visible, reduce available width accordingly
        var sbw = 0;
        if (tableView && tableView.verticalScrollBarWidth !== undefined && tableView.contentHeight > tableView.height) {
            sbw = tableView.verticalScrollBarWidth;
        }
        // var avail = Math.max(0, contentArea.width - sbw);
        var avail = Math.max(0, tableItem.width - sbw);

        // If preferred total fits, use pref (but expand proportionally if extra space)
        tableItem.columnWidths = [];
        var scale = avail / totalPref;
        if (totalPref >= avail) {
            if (tableItem.shrinkColumnIndex >= 0 && tableItem.shrinkColumnIndex < cols) {
                // valid shrinkColumnIndex ==> trying to shrink that column
                for (var cc = 0; cc < cols; ++cc) {
                    tableItem.columnWidths[cc] = pref[cc];
                }

                // check is that column can be shrinked enough to fit
                if ((totalPref - avail) <= (pref[tableItem.shrinkColumnIndex] - minColumnWidths[tableItem.shrinkColumnIndex])) {
                    // we can shrink selected column
                    tableItem.columnWidths[tableItem.shrinkColumnIndex] -= (totalPref - avail);
                }
                else {
                    // here we cannot fit => we set selected column to mimum width
                    tableItem.columnWidths[tableItem.shrinkColumnIndex] = minColumnWidths[tableItem.shrinkColumnIndex];
                }
            }
            else {
                if (totalMin >= avail) {
                    // set all to minColumnWidth
                    for (var ccc = 0; ccc < cols; ++ccc) {
                        tableItem.columnWidths[ccc] = minColumnWidths[ccc];
                    }
                }
                else {
                    // in this case we can shink to fit
                    var sum = 0;
                    for (var i = 0; i < cols; ++i) {
                        tableItem.columnWidths[i] = Math.max(minColumnWidths[i], Math.floor(pref[i] * scale));
                        sum += tableItem.columnWidths[i];
                    }
                    // adjust rounding difference
                    var ii = 0;
                    while (sum < avail) {
                        tableItem.columnWidths[ii++ % cols]++;
                        sum++;
                    }
                    while (sum > avail) {
                        var cIdx = ii % cols;
                        if (tableItem.columnWidths[cIdx] > minColumnWidths[cIdx]) {
                            tableItem.columnWidths[cIdx]--;
                            sum--;
                        }
                        ii++;
                    }
                }
            }
        } else {
            // preffered fits
            // distribute extra space proportionally
            var totalWidthScaled = 0;
            for (var j = 0; j < (cols - 1); ++j) {
                tableItem.columnWidths[j] = Math.round(pref[j] * scale);
                totalWidthScaled += tableItem.columnWidths[j];
            }

            // assign available space to last column
            tableItem.columnWidths[cols - 1] = avail - totalWidthScaled;
        }


        tableView.forceLayout();
        // compute total content width from columns and expose it so scrollbars can use it
        var totalContent = 0;
        for (var t = 0; t < tableItem.columnWidths.length; ++t) {
            totalContent += tableItem.columnWidths[t];
        }

        tableView.contentWidth = totalContent + sbw;
    }

    // column widths are computed automatically by autoAdjustColumns()

    Component.onCompleted: {
        // initialize column widths from header texts if model is available
        if (!tableView || !tableView.model)
            return;
        var cols = tableView.model.columnCount();
        tableItem.columnWidths = [];
        for (var n = 0; n < cols; ++n) {
            var hdr = tableView.model.headerData(n, Qt.Horizontal, Qt.DisplayRole);
            tableItem.columnWidths[n] = Math.max(minColumnWidth, Math.ceil(fontMetrics.boundingRect(hdr).width) + UI.standardMargin);
        }
        tableView.forceLayout();
        tableItem.calculatePreferedWidth()
        // ensure sizes account for content and scrollbars
        tableItem.autoAdjustColumns();
    }

    // Header and content area
    HorizontalHeaderView {
        id: headerView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        // width: tableView.width
        syncView: tableView
        clip: true
        // header sort state is stored on the root item (tableItem)
        delegate: Rectangle {
            color: UI.colors.buttonNeutral
            implicitHeight: tableItem.rowHeight
            implicitWidth: 10
            // Header content: label left, sort indicator right
            required property string display
            required property int index

            Row {
                spacing: 2
                anchors.centerIn: parent
                Text {
                    id: colLabel
                    text: display
                    color: UI.colors.textPrimary
                }
                AbracaColorizedImage {
                    id: sortIndicator
                    anchors.verticalCenter: colLabel.verticalCenter
                    source: UI.imagesUrl + (tableItem.sortIndicatorOrder === Qt.AscendingOrder ? "sort-up.svg" : "sort-down.svg")
                    colorizationColor: UI.colors.icon
                    sourceSize.width: colLabel.height * 0.5
                    fillMode: Image.PreserveAspectFit
                    visible: tableItem.sortingEnabled && (index === tableItem.sortIndicatorColumn)
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: function () {
                    if (tableItem.sortingEnabled) {
                        if (index === tableItem.sortIndicatorColumn) {
                            tableItem.sortIndicatorOrder = (tableItem.sortIndicatorOrder === Qt.AscendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder);
                        } else {
                            tableItem.sortIndicatorOrder = Qt.AscendingOrder;
                            tableItem.sortIndicatorColumn = index;
                        }
                        if (tableView && tableView.model && tableView.model.sort) {
                            tableView.model.sort(index, tableItem.sortIndicatorOrder);
                        }
                    }
                }
            }

            // (interactive resize removed; columns are sized automatically)
        }
        Component.onCompleted: {
            // Trigger initial sort so header indicator becomes visible
            if (tableItem.sortingEnabled) {
                tableView.model.sort(tableItem.sortIndicatorColumn, tableItem.sortIndicatorOrder);
            }
        }
    }

    Item {
        id: contentArea
        anchors.top: headerView.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        TableView {
            id: tableView
            anchors.fill: parent
            // anchors.rightMargin: vbar.visible ? vbar.width : 0
            anchors.bottomMargin: hbar.visible ? hbar.height : 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            selectionMode: TableView.ExtendedSelection

            property int anchorRow: -1

            delegate: Rectangle {
                required property string display
                required property bool selected
                required property bool isActive
                required property int row
                required property int textAlignment

                implicitHeight: tableItem.rowHeight
                color: selected ? UI.colors.highlight : UI.colors.background
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: UI.standardMargin
                    text: display
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: tableItem.cellsLeftAligned ? Text.AlignLeft : (textAlignment == 0 ? Text.AlignLeft : (textAlignment == 1 ? Text.AlignHCenter : Text.AlignRight))
                    elide: Text.ElideRight
                    color: isActive ? UI.colors.textPrimary : UI.colors.textSecondary
                    font.italic: !isActive
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked:  (mouse)=> {
                        if (mouse.button === Qt.LeftButton) {
                            // tableView.selectionModel.select(tableView.model.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current | ItemSelectionModel.Rows);

                            let flags = ItemSelectionModel.Current | ItemSelectionModel.Rows;

                            if (mouse.modifiers & Qt.ControlModifier) {
                                let modelIndex = tableView.model.index(row, 0);
                                let isSelected = tableView.selectionModel.isRowSelected(row, modelIndex.parent);
                                if (isSelected) {
                                    tableView.selectionModel.select(modelIndex, ItemSelectionModel.Deselect | ItemSelectionModel.Rows);
                                } else {
                                    tableView.selectionModel.select(modelIndex, ItemSelectionModel.Select | ItemSelectionModel.Rows);
                                }
                            } else if (mouse.modifiers & Qt.ShiftModifier && tableView.anchorRow >= 0) {
                                // Select range from anchor to current row
                                let startRow = Math.min(tableView.anchorRow, row);
                                let endRow = Math.max(tableView.anchorRow, row);

                                tableView.selectionModel.clear();
                                for (let i = startRow; i <= endRow; i++) {
                                    tableView.selectionModel.select(
                                        tableView.model.index(i, 0),
                                        ItemSelectionModel.Select | ItemSelectionModel.Rows
                                    );
                                }
                                return;
                            } else {
                                flags |= ItemSelectionModel.ClearAndSelect;
                                tableView.anchorRow = row;
                            }

                            tableView.selectionModel.select(
                                tableView.model.index(row, 0),
                                flags
                            );
                        }
                        else if (mouse.button === Qt.RightButton && tableItem.contextMenuEnabled) {
                            let modelIndex = tableView.model.index(row, 0);
                            if (!tableView.selectionModel.isRowSelected(row, modelIndex.parent)) {
                                tableView.selectionModel.select(tableView.model.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current | ItemSelectionModel.Rows);
                            }
                            populateContextMenu(row)
                            contextMenu.popup()
                        }
                    }
                    onPressAndHold: (mouse) => {
                        if (tableItem.contextMenuEnabled) {
                            tableView.selectionModel.select(tableView.model.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current | ItemSelectionModel.Rows);
                            populateContextMenu(row)
                            contextMenu.popup()
                        }
                    }
                    onDoubleClicked: {
                        tableView.selectionModel.select(tableView.model.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Current | ItemSelectionModel.Rows);
                        doubleClickedRow(row)
                    }
                }
            }

            columnWidthProvider: function (column) {
                return tableItem.columnWidths && tableItem.columnWidths[column] ? tableItem.columnWidths[column] : tableItem.minColumnWidth;
            }
            ScrollBar.vertical: AbracaScrollBar {
                id: vbar
                policy: tableView.contentHeight > tableView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            ScrollBar.horizontal: AbracaScrollBar {
                 id: hbar
                parent: tableView.parent
                anchors.top: tableView.bottom
                anchors.left: tableView.left
                anchors.right: tableView.right
                anchors.rightMargin: vbar.visible ? vbar.width : 0
                policy: tableView.contentWidth > tableView.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            // expose vertical scrollbar width for layout calculations
            property int verticalScrollBarWidth: vbar ? vbar.implicitWidth : 0

            Connections {
                target: tableItem.selectionModel
                function onCurrentChanged(current, previous) {
                    if (current && current.row !== undefined)
                        tableView.positionViewAtRow(current.row, TableView.Contain);
                }
            }

            // React to model updates: resize columns automatically when rows change
            Connections {
                // target the underlying model object exposed to the TableView
                target: tableView.model
                function onRowCountChanged() {
                    tableItem.calculatePreferedWidth();
                    tableItem.autoAdjustColumns();                    
                }
            }

            AbracaContextMenu {
                id: contextMenu
                menuModel: undefined
            }
        }
    }    
}
