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

    // Tab / Shift+Tab inside a page follows KeyNavigation.tab / backtab declared on every control.
    // Page controls start at the current tab button (entryItem) and end at tabEnd (exitItem).
    // Keys not consumed by KeyNavigation (Shift+Tab on the tab button) leave settings to the main window cycle.
    Keys.onTabPressed: (event) => UI.handleTab("settings", event, true)
    Keys.onBacktabPressed: (event) => UI.handleTab("settings", event, false)

    // Always focusable end of every page, hands focus over to the next part of the main window
    Item {
        id: tabEnd
        width: 0
        height: 0
        onActiveFocusChanged: {
            if (activeFocus) {
                Qt.callLater(() => UI.handleTab("settings", {accepted: false}, true));
            }
        }
    }

    readonly property var pageList: [deviceTab, audioTab, announcementTab, uaTab, tiiTab, otherTab]

    function ensureVisible(item) {
        let f = item.parent;
        while (f && !(f.contentItem && f.contentY !== undefined)) {
            f = f.parent;
        }
        if (!f) {
            return;
        }
        const y = item.mapToItem(f.contentItem, 0, 0).y;
        const margin = UI.standardMargin;
        if (y < f.contentY + margin) {
            f.contentY = Math.max(0, y - margin);
        } else if (y + item.height > f.contentY + f.height - margin) {
            f.contentY = Math.min(f.contentHeight - f.height, y + item.height - f.height + margin);
        }
    }

    Connections {
        target: mainItem.Window.window
        function onActiveFocusItemChanged() {
            const fi = mainItem.Window.window.activeFocusItem;
            if (fi && fi !== bar.currentItem && mainItem.isInside(fi)) {
                mainItem.ensureVisible(fi);
            }
        }
    }

    function isInside(item) {
        for (let it = item; it; it = it.parent) {
            if (it === mainItem) {
                return true;
            }
        }
        return false;
    }

    // Assigned once all pages exist, a binding declared on the button would be evaluated before the pages are created
    Component.onCompleted: {
        UI.registerTabStop("settings", () => bar ? bar.currentItem : null);
        for (let i = 0; i < bar.count; ++i) {
            bar.itemAt(i).KeyNavigation.tab = Qt.binding(() => mainItem.pageList[i].firstItem ?? tabEnd);
        }
    }
    Component.onDestruction: UI.unregisterTabStop("settings")

    AbracaTabBar {
        id: bar
        width: parent.width
        currentIndex: settingsBackend.tabsModel.currentIndex
        onCurrentIndexChanged: {
            if (settingsBackend.tabsModel.currentIndex !== currentIndex) {
                settingsBackend.tabsModel.currentIndex = currentIndex
            }
        }
        Repeater {
            model: settingsBackend.tabsModel
            AbracaTabButton {
                required property int index
                required property string itemName
                width: implicitWidth
                // only the current tab is a tab stop, other tabs are selected by arrow keys
                focusPolicy: checked ? Qt.StrongFocus : Qt.ClickFocus
                index: index
                text: itemName
            }
        }
    }
    StackLayout {
        id: pages
        anchors.top: bar.bottom
        anchors.bottom: mainItem.bottom
        anchors.left: mainItem.left
        anchors.right: mainItem.right

        // anchors.horizontalCenter: mainItem.horizontalCenter
        // width: Math.min(mainItem.width, 700)
        currentIndex: bar.currentIndex
        DeviceSettings {
            id: deviceTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
        AudioSettings {
            id: audioTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
        AnnouncementSettings {
            id: announcementTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
        UaSettings {
            id: uaTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
        TiiSettings {
            id: tiiTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
        OtherSettings {
            id: otherTab
            entryItem: bar.currentItem
            exitItem: tabEnd
        }
    }
}
