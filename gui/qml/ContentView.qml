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
import QtQuick.Dialogs

import abracaComponents

Item {
    id: mainItem

    property int currentFunctionalityId: NavigationModel.Service
    property var detachedWindows: ({})
    property bool isSmallNavigation: false

    NavigationProxyModel {
        id: navigationProxyModel
        sourceModel: navigationModel
        filterFlags: isSmallNavigation ? (appUI.isPortraitView ? NavigationModel.PortraitSmallOption | NavigationModel.PortraitSmallOthersOption :
                                                                NavigationModel.LandscapeSmallOption | NavigationModel.LandscapeSmallOthersOption)
                                       : (appUI.isPortraitView ? NavigationModel.PortraitOption | NavigationModel.PortraitOthersOption :
                                                                 NavigationModel.Landscape1Option | NavigationModel.Landscape2Option | NavigationModel.LandscapeOthersOption)
    }

    function handleFunctionalityClicked(funcId) {
        if (funcId === NavigationModel.ShowMore) {
            if (appUI.isPortraitView || UI.isMobile) {
                drawer.open();
            } else {
                if (menuLoader.item) {
                    menuLoader.item.open();
                }
            }
        }
    }

    function handleFunctionalityUndocking(funcId) {
        //console.log("handleFunctionalityUndocking: ", funcId, navigationProxyModel.getFunctionalityItemData(funcId).canUndock);
        var canUndock = navigationProxyModel.getFunctionalityItemData(funcId).canUndock;
        if (canUndock === false || appUI.isPortraitView)
            return;

        // item can be undocked - toggle undock/dock state
        for (var i = 0; i < contentRepeater.count; i++) {
            var loader = contentRepeater.itemAt(i);
            if (loader && loader.funcId === funcId) {
                if (loader.item) {
                    if (loader.isUndockedPage(loader.item.pageId)) {
                        // item is undocked -> bring to front
                        detachedWindows[loader.item.pageId].window.raise();
                    } else {
                        // item is not undocked
                        loader.undockPage(loader.item.pageId);
                    }
                } else {
                    // this means that item is not undocked yet, so we can undock it directly
                    loader.undockOnLoad = true;
                    loader.active = true;
                }
                return;
            }
        }
    }

    function showPage(funcId) {
        // this method navigates to page if not undocked or brings undocked window to front
        for (var i = 0; i < contentRepeater.count; i++) {
            var loader = contentRepeater.itemAt(i);
            if (loader && loader.funcId === funcId) {
                if (loader.item) {
                    if (loader.isUndockedPage(loader.item.pageId)) {
                        // item is undocked -> bring to front
                        detachedWindows[loader.item.pageId].window.raise();
                    } else {
                        // item is not undocked -> navigate to it
                        navigationProxyModel.setNavigationId(funcId);
                        handleFunctionalityClicked(funcId);
                    }
                } else {
                    // item not loaded yet -> navigate to it
                    navigationProxyModel.setNavigationId(funcId);
                    handleFunctionalityClicked(funcId);
                }
                return;
            }
        }
    }

    function dockAll() {
        for (var i = 0; i < contentRepeater.count; i++) {
            var loader = contentRepeater.itemAt(i);
            if (loader && loader.active) {
                if (loader.item) {
                    if (loader.isUndockedPage(loader.item.pageId)) {
                        // item is undocked -> dock it
                        loader.dockPage(loader.item.pageId);
                    }
                }
            }
        }
    }

    Connections {
        target: application
        function onUndockPageRequest(funcId) {
            handleFunctionalityUndocking(funcId);
        }
    }

    Connections {
        target: appUI
        function onIsPortraitViewChanged() {
            // force docking
            if (appUI.isPortraitView) {
                for (var pageId in detachedWindows) {
                    detachedWindows[pageId].item.isUndocked = false;
                    detachedWindows[pageId].item.content.parent = detachedWindows[pageId].dockedParent;
                    detachedWindows[pageId].window.destroy();
                    delete detachedWindows[pageId];
                }
            } else {
                drawer.close();
            }
        }
    }

    // SwipeView {
    StackLayout {
        id: swipeView
        anchors.fill: parent

        clip: true
        // interactive: false
        // orientation: appUI.isPortraitView ? Qt.Horizontal : Qt.Vertical

        currentIndex: navigationProxyModel.currentRow

        Repeater {
            id: contentRepeater
            model: navigationProxyModel
            Loader {
                id: contentLoader
                // active: isUndocked || SwipeView.isCurrentItem || SwipeView.isNextItem || SwipeView.isPreviousItem
                active: isUndocked || Math.abs(swipeView.currentIndex - index) <= 1
                // asynchronous: true
                source: qmlComponentPath
                readonly property int funcId: functionalityId
                property bool undockOnLoad: false
                onLoaded: {
                    item.visible = Qt.binding(function() { return isUndocked || swipeView.currentIndex === index; });
                    if (undockOnLoad) {
                        undockOnLoad = false;
                        undockPage(item.pageId);
                    }
                }
                function undockPage(pageId) {
                    if (appUI.isPortraitView || !canUndock || !isEnabled)
                        return;
                    if (detachedWindows[pageId]) {
                        // Page is already undocked, just bring window to front
                        detachedWindows[pageId].window.raise();
                        detachedWindows[pageId].window.requestActivate();
                        return;
                    }

                    // Create detached window with content component only
                    var component = Qt.createComponent("DetachedWindow.qml");
                    if (component.status === Component.Ready) {
                        var window = component.createObject(mainItem, {
                            "pageId": pageId,
                            "windowTitle": item.title,
                            "parentItem": contentLoader,
                            "width": Math.max(undockedWidth, item.minWidth),
                            "height": Math.max(undockedHeight, item.minHeight),
                            "x": undockedX,
                            "y": undockedY,
                            "minimumWidth": item.minWidth,
                            "minimumHeight": item.minHeight,
                            "isPageEnabled": Qt.binding(function () {
                                return model.isEnabled;
                            })
                        });

                        if (window) {
                            detachedWindows[pageId] = {
                                "window": window,
                                "item": item,
                                "dockedParent": item.content.parent
                            };
                            item.content.parent = window.container;
                            window.show();
                            item.isUndocked = true;
                            model.isUndocked = true;
                        }
                    }
                }

                function dockPage(pageId) {
                    if (detachedWindows[pageId]) {
                        // store current state to model
                        undockedWidth = detachedWindows[pageId].window.width;
                        undockedHeight = detachedWindows[pageId].window.height;
                        undockedX = detachedWindows[pageId].window.x;
                        undockedY = detachedWindows[pageId].window.y;
                        isUndocked = false;

                        detachedWindows[pageId].item.isUndocked = false;
                        detachedWindows[pageId].item.content.parent = detachedWindows[pageId].dockedParent;
                        detachedWindows[pageId].window.destroy();
                        delete detachedWindows[pageId];

                        if (Math.abs(swipeView.currentIndex - index) > 1) {
                            // active property bindindg does not work ==> force deactivation and restore binding later
                            active = false;
                            Qt.callLater(function () {
                                active = Qt.binding(function () {
                                    return isUndocked || Math.abs(swipeView.currentIndex - index) <= 1;
                                });
                            });
                        }
                    }
                }

                function isUndockedPage(pageId) {
                    if (detachedWindows[pageId]) {
                        return true;
                    }
                    return false;
                }
            }
        }
    }

    Loader {
        id: portraitDrawerLoader
        active: appUI.isPortraitView || UI.isMobile
        AbracaDrawer {
            id: drawer
            width: appUI.isPortraitView ? parent.width : drawerGrid.implicitWidth + 4 * UI.standardMargin
            //height: Math.min(parent.height * 0.75, UI.controlHeight * navigationProxyModelDrawer.rowCount() + UI.controlHeight)
            height: appUI.isPortraitView ?  Math.min(parent.height * 0.75, drawerGrid.implicitHeight + 4 * UI.standardMargin)
                                            : parent.height
            edge: appUI.isPortraitView ? Qt.BottomEdge : Qt.LeftEdge
            interactive: true

            property int currentFunctionalityId: NavigationModel.Service

            NavigationProxyModel {
                id: navigationProxyModelDrawer
                sourceModel: navigationModel
                filterFlags: mainItem.isSmallNavigation ? (appUI.isPortraitView ? NavigationModel.PortraitSmallOthersOption : NavigationModel.LandscapeSmallOthersOption)
                                                        : NavigationModel.PortraitOthersOption
            }
            GridLayout {
                id: drawerGrid
                anchors.fill: parent
                anchors.margins: 2*UI.standardMargin
                anchors.bottomMargin: 4*UI.standardMargin
                columns: appUI.isPortraitView ? 3 : Math.ceil(drawerRepeater.count / 3.0)
                rowSpacing: UI.standardMargin
                columnSpacing: UI.standardMargin
                FontMetrics {
                    id: fontMetrics
                    font.pointSize: UI.smallFontPointSize
                }
                readonly property int labelHeight: fontMetrics.height * 2 + UI.standardMargin
                readonly property int iconHeight: 1.5*UI.navigationButtonSize - 2*UI.standardMargin - labelHeight

                Repeater {
                    id: drawerRepeater
                    model: navigationProxyModelDrawer
                    MouseArea {
                        id: mouseArea
                        required property string iconName
                        required property string longLabel
                        required property string qmlComponentPath
                        required property var model
                        required property int functionalityId
                        required property bool isEnabled

                        readonly property bool isSelectedItem: functionalityId === navigationModel.currentId

                        Layout.preferredWidth: appUI.isPortraitView ? drawerGrid.width / 3 - 2 *drawerGrid.columnSpacing
                                                                    : Screen.height / 3 - 2 *drawerGrid.rowSpacing
                        Layout.preferredHeight: appUI.isPortraitView ? UI.iconSize + drawerGrid.labelHeight + 2*UI.standardMargin
                                                                     : drawerGrid.height / 3 - 2 *drawerGrid.rowSpacing
                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        enabled: isEnabled
                        hoverEnabled: true
                        Item {
                            id: rect_button
                            anchors.fill: parent
                            anchors.margins: UI.standardMargin
                            AbracaColorizedImage {
                                id: image
                                anchors.top: parent.top
                                anchors.topMargin: UI.standardMargin /2
                                image.source: UI.imagesUrl + iconName
                                height: UI.iconSize
                                width: height
                                sourceSize: Qt.size(height, height)
                                anchors.horizontalCenter: parent.horizontalCenter
                                colorizationColor: isEnabled ? (isSelectedItem ? UI.colors.listItemSelected : UI.colors.icon) : UI.colors.iconDisabled
                            }
                            AbracaLabel {
                                id: text_label
                                text: longLabel
                                anchors.top: parent.bottom
                                anchors.topMargin: -(drawerGrid.labelHeight - UI.standardMargin)
                                anchors.horizontalCenter: parent.horizontalCenter
                                enabled: isEnabled
                                font.pointSize: UI.smallFontPointSize
                                color: isSelectedItem ? UI.colors.listItemSelected : (isEnabled ? UI.colors.textPrimary : UI.colors.textDisabled)
                                height: implicitHeight
                                width: parent.width
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                        onClicked: {
                            if (isEnabled) {
                                if (qmlComponentPath) {
                                    navigationProxyModel.setNavigationId(functionalityId);
                                } else {
                                    mainItem.handleMenuItemTriggered(functionalityId);
                                }
                                drawer.close();
                            }
                        }
                        Rectangle {
                            id: hoverRect
                            z: -2
                            anchors.fill: parent
                            //anchors.margins: UI.standardMargin / 2
                            color: UI.colors.highlight
                            visible: isSelectedItem

                            radius: UI.controlRadius
                            // opacity: mouseArea.containsMouse && isEnabled && !isSelectedItem ? 0.
                        }
                    }
                }
            }
        }
    }
    Loader {
        id: menuLoader
        active: !appUI.isPortraitView && !UI.isMobile
        sourceComponent:  AbracaMenu {
            id: optionsMenu
            y: mainItem.height - height
            transformOrigin: Menu.BottomLeft
            width: {
                var result = 0;
                var padding = 0;
                for (var i = 0; i < count; ++i) {
                    var item = itemAt(i);
                    if (item.contentItem) {
                        result = Math.max(item.contentItem.implicitWidth, result);
                        padding = Math.max(item.padding, padding);
                    }
                }
                return result + padding * 2;
            }

            AbracaContextMenu {
                title: qsTr("Audio output")
                menuModel: application.audioOutputMenuModel
            }
            delegate: AbracaMenuItem {
                iconSource: UI.imagesUrl + "icon-audio.svg"
                visible: UI.isAndroid === false
                height: visible ? implicitHeight : 0
            }

            Instantiator {
                model: NavigationProxyModel {
                    id: menuModel
                    sourceModel: navigationModel
                    filterFlags: NavigationModel.LandscapeOthersOption
                }
                delegate: Item {
                    id: menuItem
                    required property int functionalityId
                    required property int index
                    required property string longLabel
                    required property string qmlComponentPath
                    required property bool isEnabled
                    required property bool isSeparator
                    required property string iconName

                    Component.onCompleted: {
                        if (isSeparator) {
                            optionsMenu.addItem(menuSeparatorComponent.createObject(menuItem));
                        } else {
                            var item = menuItemComponent.createObject(menuItem, {
                                "functionalityId": functionalityId,
                                "longLabel": longLabel,
                                "qmlComponentPath": qmlComponentPath,
                            });
                            item.enabled = Qt.binding(function () {
                                return menuItem.isEnabled;
                            });
                            item.longLabel = Qt.binding(function () {
                                return menuItem.longLabel;
                            });
                            item.iconName = Qt.binding(function () {
                                return menuItem.iconName;
                            });
                            optionsMenu.addItem(item);
                        }
                    }
                }

                onObjectAdded: function (index, object) {
                    optionsMenu.insertItem(index, object);
                }
                onObjectRemoved: function (index, object) {
                    optionsMenu.removeItem(object);
                }
            }
        }
    }

    Component {
        id: menuItemComponent
        AbracaMenuItem {
            property string longLabel
            property int functionalityId
            property string qmlComponentPath
            property bool isEnabled
            property string iconName

            text: longLabel
            enabled: isEnabled
            iconSource: iconName ? UI.imagesUrl + iconName : ""
            onTriggered: {
                if (qmlComponentPath) {
                    navigationProxyModel.setNavigationId(functionalityId);
                } else {
                    mainItem.handleMenuItemTriggered(functionalityId);
                }
            }
        }
    }
    Component {
        id: menuSeparatorComponent
        AbracaMenuSeparator {}
    }

    function handleMenuItemTriggered(id: int) {
        console.log("handleMenuItemTriggered: ", id);

        switch (id) {
        case NavigationModel.BandScan:
            appWindow.startBandScan();
            break;
        case NavigationModel.AudioRecording:
            application.audioRecordingToggle();
            break;
        case NavigationModel.ExportServiceList:
            application.exportServiceList();
            break;
        case NavigationModel.ClearServiceList:
            application.clearServiceList();
            break;
        case NavigationModel.Quit:
            appWindow.quitApplication();
            break;
        }
    }
}
