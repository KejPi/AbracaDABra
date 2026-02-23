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
import QtQuick.Layouts
import QtQuick.Effects

import abracaComponents

FocusScope {
    id: mainItem    
    signal functionalityClicked(funcId: int)
    signal functionalityUndockRequested(funcId: int)

    readonly property bool isSmallNavigation: Math.max(width, height) < 9*UI.navigationButtonSize
    readonly property int buttonSize: isSmallNavigation ? UI.navigationSmallButtonSize : UI.navigationButtonSize

    Component.onCompleted: {
        navigationProxyModel.setNavigationId(NavigationModel.Service);
    }

    implicitHeight: buttonSize
    implicitWidth: buttonSize

    // Helper function to find first enabled item index in a list
    function findFirstEnabledIndex(listView) {
        for (let i = 0; i < listView.count; i++) {
            let item = listView.itemAtIndex(i);
            if (item && item.isEnabled) {
                return i;
            }
        }
        return -1;
    }

    // Helper function to find last enabled item index in a list
    function findLastEnabledIndex(listView) {
        for (let i = listView.count - 1; i >= 0; i--) {
            let item = listView.itemAtIndex(i);
            if (item && item.isEnabled) {
                return i;
            }
        }
        return -1;
    }

    // Helper function to find next enabled item index
    function findNextEnabledIndex(listView, fromIndex) {
        for (let i = fromIndex + 1; i < listView.count; i++) {
            let item = listView.itemAtIndex(i);
            if (item && item.isEnabled) {
                return i;
            }
        }
        return -1;
    }

    // Helper function to find previous enabled item index
    function findPrevEnabledIndex(listView, fromIndex) {
        for (let i = fromIndex - 1; i >= 0; i--) {
            let item = listView.itemAtIndex(i);
            if (item && item.isEnabled) {
                return i;
            }
        }
        return -1;
    }

    Component {
        id: horizotalButtonComponent
        MouseArea {
            id: mouseArea
            readonly property ListView listview: ListView.view
            required property string iconName
            required property string shortLabel            
            required property var model
            required property int functionalityId
            required property bool isEnabled
            required property int index

            readonly property bool isSelectedItem: index === listview.selectedRow

            height: buttonSize
            width: buttonSize
            enabled: isEnabled
            hoverEnabled: true
            Item {
                id: rect_button
                anchors.fill: parent
                anchors.margins: 5
                AbracaColorizedImage {
                    image.source: UI.imagesUrl + iconName
                    height: parent.height - text_label.height - 4
                    width: height
                    sourceSize: Qt.size(height, height)
                    anchors.horizontalCenter: parent.horizontalCenter
                    colorizationColor: isEnabled ? (isSelectedItem ? UI.colors.listItemSelected : UI.colors.icon) : UI.colors.iconDisabled
                }
                AbracaLabel {
                    id: text_label
                    text: shortLabel
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    enabled: isEnabled                    
                    font.pointSize: UI.smallFontPointSize
                    color: isSelectedItem ? UI.colors.listItemSelected : (isEnabled ? UI.colors.textPrimary : UI.colors.textDisabled)
                    visible: appUI.currentView !== ApplicationUI.PortraitNarrowView
                    height: appUI.currentView !== ApplicationUI.PortraitNarrowView ? 15 /*implicitHeight*/ : 0
                }
            }
            onClicked: {
                listview.currentIndex = index;
                functionalityClicked(functionalityId);
                if (functionalityId !== NavigationModel.ShowMore) {
                    navigationProxyModel.setNavigationId(functionalityId);
                }
                listview.forceActiveFocus();
            }
            Rectangle {
                id: hoverRect
                z: -2
                anchors.fill: parent
                color: UI.colors.listItemHovered
                visible: mouseArea.containsMouse && isEnabled && !isSelectedItem
            }
        }
    }

    Component {
        id: verticalButtonComponent

        MouseArea {
            id: mouseArea
            readonly property ListView listview: ListView.view
            required property string iconName
            required property string shortLabel
            required property string longLabel
            required property var model
            required property int functionalityId
            required property bool isEnabled
            required property int index

            readonly property bool isSelectedItem: index === listview.selectedRow

            enabled: isEnabled
            height: buttonSize
            width: buttonSize
            acceptedButtons: Qt.LeftButton //  | Qt.RightButton
            hoverEnabled: true
            Item {
                id: rect_button
                anchors.fill: parent
                anchors.margins: 5
                AbracaColorizedImage {
                    image.source: UI.imagesUrl + iconName
                    height: parent.height - (text_label.visible ? (text_label.height + 4) : 8)
                    width: height
                    sourceSize: Qt.size(height, height)
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: rect_button.top
                    anchors.topMargin: text_label.visible ? 0 : (rect_button.height - height)/2
                    colorizationColor: isEnabled ? (isSelectedItem ? UI.colors.listItemSelected : UI.colors.icon) : UI.colors.iconDisabled
                }
                AbracaLabel {
                    id: text_label
                    text: shortLabel
                    anchors.bottom: parent.bottom
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    enabled: isEnabled
                    elide: Text.ElideRight
                    font.pointSize: UI.smallFontPointSize
                    color: isSelectedItem ? UI.colors.listItemSelected : (isEnabled ? UI.colors.textPrimary : UI.colors.textDisabled)
                    visible: appUI.isCompact === false
                }
            }
            onClicked: mouse => {
                if (mouse.button === Qt.LeftButton) {
                    if (mouse.modifiers & Qt.ControlModifier) {
                        // undock functionality (Ctrl+Click on Win/Linux, Cmd+Click on macOS)
                        functionalityUndockRequested(functionalityId);
                    } else {
                        selectItem();
                    }
                }
            }
            Rectangle {
                id: hoverRect
                z: -2
                anchors.fill: parent
                color: UI.colors.listItemHovered
                visible: mouseArea.containsMouse && isEnabled && !isSelectedItem
            }
            AbracaToolTip {
                text: longLabel
                hoverMouseArea: mouseArea
            }

            function selectItem() {
                listview.currentIndex = index;
                functionalityClicked(functionalityId);
                if (functionalityId !== NavigationModel.ShowMore) {
                    navigationProxyModel.setNavigationId(functionalityId);
                }
                listview.forceActiveFocus();
            }
        }
    }

    Component {
        id: horizotalSmallButtonComponent
        MouseArea {
            id: mouseArea
            readonly property ListView listview: ListView.view
            required property string iconName
            required property string shortLabel
            required property var model
            required property int functionalityId
            required property bool isEnabled
            required property int index

            readonly property bool isSelectedItem: index === listview.selectedRow

            height: buttonSize
            width: buttonSize
            enabled: isEnabled
            hoverEnabled: true
            Item {
                id: rect_button
                anchors.fill: parent
                anchors.margins: 5
                AbracaColorizedImage {
                    image.source: UI.imagesUrl + iconName
                    height: parent.height - 4
                    width: height
                    sourceSize: Qt.size(height, height)
                    anchors.horizontalCenter: parent.horizontalCenter
                    colorizationColor: isEnabled ? (isSelectedItem ? UI.colors.listItemSelected : UI.colors.icon) : UI.colors.iconDisabled
                }
            }
            onClicked: {
                listview.currentIndex = index;
                functionalityClicked(functionalityId);
                if (functionalityId !== NavigationModel.ShowMore) {
                    navigationProxyModel.setNavigationId(functionalityId);
                }
                listview.forceActiveFocus();
            }
            Rectangle {
                id: hoverRect
                z: -2
                anchors.fill: parent
                color: UI.colors.listItemHovered
                visible: mouseArea.containsMouse && isEnabled && !isSelectedItem
            }
        }
    }

    Component {
        id: verticalSmallButtonComponent

        MouseArea {
            id: mouseArea
            readonly property ListView listview: ListView.view
            required property string iconName
            required property string shortLabel
            required property var model
            required property int functionalityId
            required property bool isEnabled
            required property int index

            readonly property bool isSelectedItem: index === listview.selectedRow

            enabled: isEnabled
            height: buttonSize
            width: buttonSize
            acceptedButtons: Qt.LeftButton //  | Qt.RightButton
            hoverEnabled: true
            Item {
                id: rect_button
                anchors.fill: parent
                anchors.margins: 5
                AbracaColorizedImage {
                    image.source: UI.imagesUrl + iconName
                    height: parent.height - 4
                    width: height
                    sourceSize: Qt.size(height, height)
                    anchors.horizontalCenter: parent.horizontalCenter
                    colorizationColor: isEnabled ? (isSelectedItem ? UI.colors.listItemSelected : UI.colors.icon) : UI.colors.iconDisabled
                }
            }
            onClicked: mouse => {
                if (mouse.button === Qt.LeftButton) {
                    if (mouse.modifiers & Qt.ControlModifier) {
                        // undock functionality (Ctrl+Click on Win/Linux, Cmd+Click on macOS)
                        functionalityUndockRequested(functionalityId);
                    } else {
                        selectItem();
                    }
                }
            }
            Rectangle {
                id: hoverRect
                z: -2
                anchors.fill: parent
                color: UI.colors.listItemHovered
                visible: mouseArea.containsMouse && isEnabled && !isSelectedItem
            }
            function selectItem() {
                listview.currentIndex = index;
                functionalityClicked(functionalityId);
                if (functionalityId !== NavigationModel.ShowMore) {
                    navigationProxyModel.setNavigationId(functionalityId);
                }
                listview.forceActiveFocus();
            }
        }
    }

    NavigationProxyModel {
        id: navigationProxyModel
        sourceModel: navigationModel
        filterFlags: isSmallNavigation ? (appUI.isPortraitView ? NavigationModel.PortraitSmallOption : NavigationModel.LandscapeSmallOption)
                                       : (appUI.isPortraitView ? NavigationModel.PortraitOption : NavigationModel.Landscape1Option)
    }

    ListView {
        id: navigationList
        anchors.fill: parent
        anchors.bottomMargin: appUI.isPortraitView || isSmallNavigation ? 0 : landscape2Loader.implicitHeight
        model: navigationProxyModel
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: true
        highlightFollowsCurrentItem: appUI.isPortraitView
        delegate: isSmallNavigation ? (appUI.isPortraitView ? horizotalSmallButtonComponent : verticalSmallButtonComponent)
                                    : (appUI.isPortraitView ? horizotalButtonComponent : verticalButtonComponent)
        orientation: appUI.isPortraitView ? ListView.Horizontal : ListView.Vertical
        spacing: {
            if (appUI.isPortraitView && count > 0) {
                return (width - (mainItem.buttonSize * count)) / (count - 1);
            } else {
                if (mainItem.isSmallNavigation && count > 0) {
                    return (height - (mainItem.buttonSize * count)) / (count - 1);
                }
                return 0;
            }
        }

        // Track selected functionality row (from model) separately from keyboard currentIndex
        property int selectedRow: navigationProxyModel.currentRow
        property int previousCurrentIndex: -1
        property bool shouldAnimate: false

        // Initialize currentIndex to selected row
        Component.onCompleted: {
            currentIndex = selectedRow;
            previousCurrentIndex = selectedRow;
        }

        onCurrentIndexChanged: {
            // Animate only if moving by exactly 1 position
            let diff = Math.abs(currentIndex - previousCurrentIndex);
            shouldAnimate = (diff === 1);
            previousCurrentIndex = currentIndex;
            // Restart fade animation on navigation
            if (activeFocus) {
                mainItem.restartKeyboardHighlightFade();
            }
        }

        // When selected row changes (e.g., from click), sync currentIndex
        onSelectedRowChanged: {
            if (activeFocus) {
                currentIndex = selectedRow;
            }
        }

        // When focus is lost, align currentIndex with selectedRow
        onActiveFocusChanged: {
            if (activeFocus) {
                // Restart fade animation on focus gain
                mainItem.restartKeyboardHighlightFade();
            } else {
                if (selectedRow >= 0) {
                    previousCurrentIndex = selectedRow;  // Prevent animation
                    currentIndex = selectedRow;
                }
            }
        }

        activeFocusOnTab: true
        keyNavigationEnabled: false
        Keys.onPressed: event => {
            const isHorizontal = appUI.isPortraitView;
            const nextKey = isHorizontal ? Qt.Key_Right : Qt.Key_Down;
            const prevKey = isHorizontal ? Qt.Key_Left : Qt.Key_Up;

            if (event.key === nextKey) {
                event.accepted = true;
                // Find next enabled item in this list
                let nextIdx = findNextEnabledIndex(navigationList, currentIndex);
                if (nextIdx >= 0) {
                    currentIndex = nextIdx;
                } else if (!appUI.isPortraitView && landscape2Loader.active && landscape2Loader.item) {
                    // Move to landscapeSepListview
                    let sepList = landscape2Loader.item;
                    let firstIdx = findFirstEnabledIndex(sepList);
                    if (firstIdx >= 0) {
                        sepList.currentIndex = firstIdx;
                        sepList.forceActiveFocus();
                    }
                }
            } else if (event.key === prevKey) {
                event.accepted = true;
                // Find previous enabled item
                let prevIdx = findPrevEnabledIndex(navigationList, currentIndex);
                if (prevIdx >= 0) {
                    currentIndex = prevIdx;
                }
            } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                event.accepted = true;
                let item = itemAtIndex(currentIndex);
                if (item && item.isEnabled) {
                    item.selectItem();
                }
            }
        }

        // Highlight for selected functionality (manually positioned based on selectedRow)
        highlight: Item {
            visible: navigationList.selectedRow >= 0
            x: {
                if (appUI.isPortraitView) {
                    let item = navigationList.itemAtIndex(navigationList.selectedRow);
                    return item ? item.x : 0;
                }
                return 0;
            }
            y: {
                if (!appUI.isPortraitView) {
                    let item = navigationList.itemAtIndex(navigationList.selectedRow);
                    return item ? item.y : 0;
                }
                return 0;
            }
            width: buttonSize
            height: buttonSize

            Rectangle {
                anchors.fill: parent
                color: UI.colors.highlight
            }
            Rectangle {
                id: accentBar
                height: appUI.isPortraitView ? 4 : parent.height
                width: appUI.isPortraitView ? parent.width : 4
                anchors.top: parent.top
                color: UI.colors.accent
            }
        }
    }

    Loader {
        id: landscape2Loader
        active: isSmallNavigation === false && appUI.isPortraitView === false
        anchors.bottom: parent.bottom
        width: parent.width
        sourceComponent: ListView {
            id: landscapeSepListview
            width: parent.width
            height: contentHeight
            model: NavigationProxyModel {
                id: navigationProxyModelLandSep
                sourceModel: navigationModel
                filterFlags: NavigationModel.Landscape2Option
            }

            clip: true
            interactive: false
            delegate: verticalButtonComponent
            orientation: ListView.Vertical
            highlightFollowsCurrentItem: false

            // Track selected functionality row (from model) separately from keyboard currentIndex
            property int selectedRow: navigationProxyModelLandSep.currentRow
            property int previousCurrentIndex: -1
            property bool shouldAnimate: false

            // Initialize currentIndex - start at first item for keyboard nav, but no selection highlight initially
            Component.onCompleted: {
                currentIndex = selectedRow >= 0 ? selectedRow : 0;
                previousCurrentIndex = currentIndex;
            }

            onCurrentIndexChanged: {
                // Animate only if moving by exactly 1 position
                let diff = Math.abs(currentIndex - previousCurrentIndex);
                shouldAnimate = (diff === 1);
                previousCurrentIndex = currentIndex;
                // Restart fade animation on navigation
                if (activeFocus) {
                    mainItem.restartKeyboardHighlightFade();
                }
            }

            // When selected row changes (e.g., from click), sync currentIndex
            onSelectedRowChanged: {
                if (activeFocus && selectedRow >= 0) {
                    currentIndex = selectedRow;
                }
            }

            // When focus is lost, align currentIndex with selectedRow
            onActiveFocusChanged: {
                if (activeFocus) {
                    // Restart fade animation on focus gain
                    mainItem.restartKeyboardHighlightFade();
                } else {
                    if (selectedRow >= 0) {
                        previousCurrentIndex = selectedRow;  // Prevent animation
                        currentIndex = selectedRow;
                    }
                }
            }

            activeFocusOnTab: false  // Tab handled by FocusScope - treat as one component
            keyNavigationEnabled: false
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down) {
                    event.accepted = true;
                    // Find next enabled item in this list
                    let nextIdx = findNextEnabledIndex(landscapeSepListview, currentIndex);
                    if (nextIdx >= 0) {
                        currentIndex = nextIdx;
                    }
                } else if (event.key === Qt.Key_Up) {
                    event.accepted = true;
                    // Find previous enabled item in this list
                    let prevIdx = findPrevEnabledIndex(landscapeSepListview, currentIndex);
                    if (prevIdx >= 0) {
                        currentIndex = prevIdx;
                    } else {
                        // Move back to navigationList
                        let lastIdx = findLastEnabledIndex(navigationList);
                        if (lastIdx >= 0) {
                            navigationList.currentIndex = lastIdx;
                            navigationList.forceActiveFocus();
                        }
                    }
                } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                    event.accepted = true;
                    let item = itemAtIndex(currentIndex);
                    if (item && item.isEnabled) {
                        item.selectItem();
                    }
                }
            }

            // Highlight for selected functionality (manually positioned based on selectedRow)
            highlight: Item {
                visible: landscapeSepListview.selectedRow >= 0
                y: {
                    let item = landscapeSepListview.itemAtIndex(landscapeSepListview.selectedRow);
                    return item ? item.y : 0;
                }
                width: buttonSize
                height: buttonSize

                Rectangle {
                    anchors.fill: parent
                    color: UI.colors.highlight
                }
                Rectangle {
                    height: parent.height
                    width: 4
                    anchors.top: parent.top
                    color: UI.colors.accent
                }
            }
        }
    }

    // Single keyboard navigation highlight for landscape mode (moves between both lists)
    Rectangle {
        id: keyboardHighlight
        visible: !appUI.isPortraitView && (navigationList.activeFocus || (landscape2Loader.item && landscape2Loader.item.activeFocus))
        z: 100
        color: "transparent"
        radius: {
            // No radius when current item equals selected item (functionality ID)
            if (navigationList.activeFocus) {
                return navigationList.currentIndex === navigationList.selectedRow ? 0 : UI.controlRadius;
            } else if (landscape2Loader.item && landscape2Loader.item.activeFocus) {
                return landscape2Loader.item.currentIndex === landscape2Loader.item.selectedRow ? 0 : UI.controlRadius;
            }
            return UI.controlRadius;
        }
        border.color: UI.colors.accent
        border.width: 1
        width: buttonSize
        height: buttonSize
        y: {
            if (navigationList.activeFocus && navigationList.currentItem) {
                return navigationList.currentItem.y;
            } else if (landscape2Loader.item && landscape2Loader.item.activeFocus && landscape2Loader.item.currentItem) {
                return landscape2Loader.y + landscape2Loader.item.currentItem.y;
            }
            return 0;
        }
        Behavior on y {
            enabled: (navigationList.activeFocus) || (landscape2Loader.item && landscape2Loader.item.activeFocus)
            SmoothedAnimation {
                duration: 200
            }
        }
        opacity: 1.0
        SequentialAnimation on opacity {
            id: keyboardHighlightFadeAnim
            running: !appUI.isPortraitView && (navigationList.activeFocus || (landscape2Loader.item && landscape2Loader.item.activeFocus))
            loops: 1
            PauseAnimation {
                duration: 2000
            }
            NumberAnimation {
                to: 0.2
                duration: 300
            }
        }
    }

    // Helper function to restart keyboard highlight fade animation
    function restartKeyboardHighlightFade() {
        keyboardHighlight.opacity = 1.0;
        keyboardHighlightFadeAnim.restart();
    }
}
