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

Item {
    id: mainItem
    property bool isPortrait: appUI.isPortraitView
    property int itemHeight: appUI.isCompact ? UI.controlHeight : 32 + 2 * UI.standardMargin
    property int listWidth: minWidth

    readonly property int minWidth: channelCombo.width + channelUpDownRow.width
    readonly property int maxWidth: 3 * minWidth

    Item {
        id: ensembleInfo
        Layout.fillWidth: true
        Layout.preferredHeight: UI.controlHeight

        RowLayout {
            id: ensembleInfoRow
            anchors.fill: parent
            anchors.leftMargin: isPortrait ? UI.standardMargin : 0
            anchors.rightMargin: UI.standardMargin
            height: 32
            Image {
                id: ensembleLogoImage
                Layout.preferredWidth: 32
                Layout.preferredHeight: Layout.preferredWidth
                visible: appUI.isEnsembleLogoVisible
                source: "image://metadata/logo/" + appUI.ensembleId + "/0"
                cache: false
                onVisibleChanged: {
                    if (visible) {
                        // Force reload of the logo when it becomes visible, to reflect possible changes
                        ensembleLogoImage.source = ""
                        ensembleLogoImage.source = "image://metadata/logo/" + appUI.ensembleId + "/0"
                    }
                }
            }
            AbracaLabel {
                text: appUI.ensembleLabel
                toolTipText: appUI.ensembleLabelToolTip
                font.bold: true
                font.pointSize: UI.largeFontPointSize
            }
            Item {
                Layout.preferredHeight: countryFlagImage.height
                Layout.preferredWidth: countryFlagImage.width
                visible: appUI.isEnsembleFlagVisible
                onVisibleChanged: {
                    if (visible) {
                        // Force reload flag
                        countryFlagImage.source = ""
                        countryFlagImage.source = "image://metadata/flag/" + appUI.ensembleId
                    }
                }
                Image {
                    id: countryFlagImage
                    height: 16
                    fillMode: Image.PreserveAspectFit
                    source: "image://metadata/flag/" + appUI.ensembleId
                    cache: false
                }
                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
                AbracaToolTip {
                    text: appUI.ensembleFlagToolTip
                    hoverMouseArea: hoverArea
                }
            }
            Item {
                Layout.fillWidth: true
            }
            SPIProgress {
                id: spiProgress
                value: appUI.spiProgressEns
                visible: appUI.isSpiProgressEnsVisible
                toolTipText: appUI.spiProgressEnsToolTip
            }
        }
    }
    Item {
        id: channelControl
        // Layout.preferredHeight: slStacked.visible ? channelCombo.implicitHeight :
        //                                             channelCombo.implicitHeight + UI.controlHeightSmaller + UI.standardMargin
        //                                             //  * (slStacked.visible ? 1.0 : 1.5)
        Layout.preferredHeight: channelCombo.implicitHeight
        Layout.preferredWidth: channelControlRow.implicitWidth
        Layout.fillWidth: slStacked.visible === false || (mainItem.width < 1.5 * mainItem.minWidth)
        RowLayout {
            id: channelControlRow
            anchors.left: parent.left
            anchors.leftMargin: isPortrait ? UI.standardMargin : 0
            anchors.right: parent.right
            anchors.rightMargin: isPortrait ? UI.standardMargin : 0
            spacing: 0
            AbracaComboBox {
                id: channelCombo
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: implicitWidth * 0.7
                Layout.rightMargin: 5
                model: channelListModel
                textRole: "display"
                valueRole: "frequency"
                currentIndex: appUI.channelIndex
                onCurrentIndexChanged: application.setChannelIndex(currentIndex) // appUI.channelIndex = currentIndex
                enabled: appUI.tuneEnabled
                delegateCount: 0
            }
            RowLayout {
                id: channelUpDownRow
                //Layout.fillWidth: true
                Layout.alignment: Qt.AlignCenter
                AbracaImgButton {
                    id: channelDown
                    source: UI.imagesUrl + "chevron-left.svg"
                    toolTipText: appUI.channelDownToolTip
                    enabled: appUI.tuneEnabled
                    onClicked: application.onChannelDownClicked()
                }
                AbracaLabel {
                    id: frequencyLabel
                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                    text: appUI.frequencyLabel
                    Layout.preferredWidth: metrics.width
                    horizontalAlignment: Qt.AlignHCenter
                    TextMetrics {
                        id: metrics
                        text: "000.000 MHz"
                        font: frequencyLabel.font
                    }
                }
                AbracaImgButton {
                    id: channelUp
                    source: UI.imagesUrl + "chevron-right.svg"
                    toolTipText: appUI.channelUpToolTip
                    enabled: appUI.tuneEnabled
                    onClicked: application.onChannelUpClicked()
                }
            }
            // Item { Layout.fillWidth: true }
        }
    }

    ListView {
        id: serviceList
        clip: true
        enabled: appUI.serviceSelectionEnabled
        model: slModel
        // onEnabledChanged: console.log("serviceList enabled changed: " + enabled)

        signal restartHighlightAnimation()

        property int selectedItemIndex: -1
        property int previousCurrentIndex: -1
        property bool shouldAnimate: false

        topMargin: UI.standardMargin
        bottomMargin: UI.standardMargin

        currentIndex: -1
        highlightFollowsCurrentItem: false
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: itemHeight
        preferredHighlightEnd: serviceList.height - itemHeight
        highlight: Rectangle {
            id: keyboardHighlight
            visible: serviceList.activeFocus
            onVisibleChanged: {
                if (visible) {
                    // Reset opacity highlight appears
                    keyboardHighlight.opacity = 1.0;
                    highlightFadeAnim.restart();
                }
            }
            Connections {
                target: serviceList
                function onRestartHighlightAnimation() {
                    keyboardHighlight.opacity = 1.0;
                    highlightFadeAnim.restart();
                }
            }
            z: 100 // serviceList.currentIndex === serviceList.selectedItemIndex ? 0 : 100
            color: "transparent"
            radius: UI.controlRadius
            y: serviceList.currentItem ? serviceList.currentItem.y : 0
            height: serviceList.currentItem ? serviceList.currentItem.height : 0
            width: serviceList.width - UI.standardMargin
            border.color: UI.colors.accent
            border.width: 1 // serviceList.currentIndex === serviceList.selectedItemIndex ? 0 : 1
            Behavior on y {
                enabled: serviceList.shouldAnimate
                SmoothedAnimation {
                    velocity: 200
                }
            }
            opacity: 1.0
            onYChanged: {
                // Reset opacity when moving highlight
                keyboardHighlight.opacity = 1.0;
                highlightFadeAnim.restart();
            }
            SequentialAnimation on opacity {
                id: highlightFadeAnim
                running: serviceList.activeFocus
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

        onCurrentIndexChanged: {
            // Animate only if moving by exactly 1 position (up or down)
            let diff = Math.abs(currentIndex - previousCurrentIndex);
            shouldAnimate = (diff === 1);
            previousCurrentIndex = currentIndex;
        }

        // connection to selection model
        Connections {
            target: slSelectionModel
            function onCurrentChanged(current, previous) {
                if (serviceList.currentIndex !== current.row) {
                    serviceList.currentIndex = current.row;
                }
                serviceList.selectedItemIndex = current.row;
            }
        }
        // initial index
        Component.onCompleted: {
            currentIndex = slSelectionModel.currentIndex.row;
            selectedItemIndex = slSelectionModel.currentIndex.row;
            Qt.callLater(function () {
                // Manually position to respect highlight range since highlightFollowsCurrentItem is false
                let targetY = currentIndex * itemHeight;
                let viewportTop = contentY;
                let viewportBottom = contentY + height;
                let preferredTop = preferredHighlightBegin;
                let preferredBottom = height - preferredHighlightEnd;

                // Position so item is within preferred highlight range
                if (targetY < viewportTop + preferredTop) {
                    contentY = Math.max(0, targetY - preferredTop);
                } else if (targetY + itemHeight > viewportBottom - preferredBottom) {
                    contentY = Math.min(contentHeight - height, targetY + itemHeight - height + preferredBottom);
                }
            });
        }

        delegate: AbracaItemDelegate {
            required property string serviceName
            required property int index
            required property string sidHex
            required property bool isFavorite
            required property string smallLogoId

            readonly property bool isSelectedItem: index === serviceList.selectedItemIndex

            height: itemHeight
            width: serviceList.width - UI.standardMargin

            highlighted: isSelectedItem // ListView.isCurrentItem
            onClicked: {
                // slSelectionModel.select(serviceList.model.index(index, 0), ItemSelectionModel.ClearAndSelect);
                slSelectionModel.setCurrentIndex(serviceList.model.index(index, 0), ItemSelectionModel.ClearAndSelect);
                serviceList.forceActiveFocus();
                serviceList.restartHighlightAnimation();
                serviceList.currentIndex = index
            }

            text: serviceName

            topPadding: appUI.isCompact ? 0 : (itemHeight - 32) / 2
            leftPadding: topPadding
            rightPadding: 0 // topPadding
            bottomPadding: topPadding

            contentItem: Item {
                id: contentItem
                width: parent.width - leftPadding - rightPadding
                height: parent.height - topPadding - bottomPadding
                // Normal view
                Item {
                    id: normalViewItem
                    visible: !appUI.isCompact
                    anchors.fill: parent

                    AbracaColorizedImage {
                        id: logoImage
                        width: 32
                        height: 32
                        anchors.verticalCenter: normalViewItem.verticalCenter
                        source: smallLogoId !== "" ? "image://metadata/logo/" + smallLogoId : UI.imagesUrl + "icon-service.svg"
                        cache: false
                        colorizationEnabled: smallLogoId === ""
                        colorizationColor: UI.colors.disabled
                    }
                    Column {
                        anchors.verticalCenter: normalViewItem.verticalCenter
                        // anchors.left: favoriteIcon.right
                        anchors.left: logoImage.right
                        anchors.leftMargin: UI.standardMargin
                        spacing: 2
                        AbracaLabel {
                            text: serviceName
                            font.bold: isSelectedItem
                            color: isSelectedItem ? UI.colors.listItemSelected : UI.colors.textPrimary
                        }
                        AbracaLabel {
                            text: "SID: " + sidHex
                            role: UI.LabelRole.Secondary
                        }
                    }
                }
                // Slim view
                AbracaLabel {
                    id: slimLabel
                    visible: appUI.isCompact
                    anchors.verticalCenter: contentItem.verticalCenter
                    anchors.left: contentItem.left
                    anchors.leftMargin: UI.standardMargin
                    text: serviceName
                    font.bold: isSelectedItem
                    color: isSelectedItem ? UI.colors.listItemSelected : UI.colors.textPrimary
                }

                // Shared favorite button (used by both views)
                AbracaImgButton {
                    id: favoriteIcon
                    anchors.verticalCenter: contentItem.verticalCenter
                    anchors.right: contentItem.right

                    colorizationEnabled: checked === false || isSelectedItem
                    colorizationColor: isSelectedItem && isFavorite ? UI.colors.accent : UI.colors.inactive
                    icon.height: UI.iconSizeSmall
                    icon.width: UI.iconSizeSmall
                    checkable: true
                    checked: isFavorite
                    sourceChecked: UI.imagesUrl + "star.svg"
                    sourceUnchecked: UI.imagesUrl + "starEmpty.svg"
                    toolTipChecked: qsTr("Remove service from favorites")
                    toolTipUnchecked: qsTr("Add service to favorites")
                    onToggled: {
                        let modelIndex = serviceList.model.index(index, 0);
                        // console.log("Favorite toggled: " + checked + " for service index: " + index, modelIndex)
                        application.setServiceFavorite(modelIndex, checked);
                    }
                }
            }
        }
        ScrollIndicator.vertical: AbracaScrollIndicator {}
        AbracaHorizontalShadow {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: parent.width
            topDownDirection: true
            shadowDistance: serviceList.contentY
        }
        AbracaHorizontalShadow {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: parent.width
            topDownDirection: false
            shadowDistance: serviceList.contentHeight - serviceList.height - serviceList.contentY
        }
        // Rectangle {
        //     anchors.fill: parent
        //     color: "transparent"
        //     border.color: UI.colors.divider
        //     border.width: serviceList.activeFocus ? 1 : 0
        // }
    }

    Item {
        id: serviceTreeItem
        TreeView {
            id: serviceTree
            anchors.fill: serviceTreeItem
            topMargin: UI.standardMargin
            bottomMargin: UI.standardMargin
            leftMargin: 0

            signal restartTreeHighlightAnimation()

            clip: true
            enabled: appUI.serviceSelectionEnabled
            model: slTreeModel
            selectionModel: slTreeSelectionModel
            // selectionMode: TableView.SingleSelection
            // selectionBehavior: TableView.SelectRows
            selectionBehavior: TableView.SelectionDisabled
            pointerNavigationEnabled: true

            property int previousCurrentRow: -1
            property bool shouldAnimate: false

            function showCurrent(currentIdx) {
                if (currentIdx.valid) {
                    serviceTree.expandToIndex(currentIdx);
                    serviceTree.forceLayout();
                    serviceTree.positionViewAtRow(serviceTree.rowAtIndex(currentIdx), TableView.Contain);
                }
            }

            // connection to selection model
            Connections {
                target: slTreeSelectionModel
                function onCurrentChanged(current, previous) {
                    serviceTree.showCurrent(current);
                }
            }

            onCurrentRowChanged: {
                // Animate only if moving by exactly 1 row (up or down)
                let diff = Math.abs(serviceTree.currentRow - serviceTree.previousCurrentRow);
                serviceTree.shouldAnimate = (diff === 1);
                serviceTree.previousCurrentRow = serviceTree.currentRow;
            }

            // initial index
            Component.onCompleted: {
                serviceTree.previousCurrentRow = serviceTree.rowAtIndex(slTreeSelectionModel.currentIndex);
                Qt.callLater(function () {
                    serviceTree.showCurrent(slTreeSelectionModel.currentIndex);
                });
            }

            Keys.onSpacePressed: {
                if (slTreeSelectionModel.currentIndex.valid) {
                    //slTreeSelectionModel.select(slTreeSelectionModel.currentIndex, ItemSelectionModel.ClearAndSelect);
                    let index = serviceTree.index(serviceTree.currentRow, serviceTree.currentColumn);
                    slTreeSelectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect);
                }
            }

            Keys.onUpPressed: event => {
                if (serviceTree.currentRow > 0) {
                    let newIndex = serviceTree.index(serviceTree.currentRow - 1, 0);
                    slTreeSelectionModel.setCurrentIndex(newIndex, ItemSelectionModel.NoUpdate);
                    event.accepted = true;
                }
            }

            Keys.onDownPressed: event => {
                if (serviceTree.currentRow < serviceTree.rows - 1) {
                    let newIndex = serviceTree.index(serviceTree.currentRow + 1, 0);
                    slTreeSelectionModel.setCurrentIndex(newIndex, ItemSelectionModel.NoUpdate);
                    event.accepted = true;
                }
            }

            onExpanded: (row, depth) => {
                if (depth === 1) {
                    expandRecursively(row)
                }
            }

            alternatingRows: false
            delegate: TreeViewDelegate {
                id: treeViewDelegate

                required property string serviceName
                required property bool isFavorite
                required property string channel

                implicitWidth: serviceTree.width - UI.standardMargin
                implicitHeight: UI.controlHeight

                highlighted: selected
                font.bold: highlighted

                topPadding: 0
                // leftPadding: topPadding
                rightPadding: topPadding
                bottomPadding: topPadding

                spacing: 0
                leftMargin: 0
                indentation: UI.controlHeight - 10

                onClicked: {
                    let index = serviceTree.index(row, column);
                    serviceTree.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect);
                    serviceTree.forceActiveFocus();

                    serviceTree.restartTreeHighlightAnimation();
                    //serviceList.currentIndex = index
                }
                contentItem: Item {
                    id: tContentItem
                    width: parent.width
                    height: parent.height

                    AbracaLabel {
                        anchors.verticalCenter: tContentItem.verticalCenter
                        text: serviceName
                        //font.bold: highlighted
                        font.weight: highlighted ? Font.Bold : (depth === 0 ? Font.Medium : Font.Normal)
                        color: (highlighted && depth > 0) ? UI.colors.listItemSelected : UI.colors.textPrimary
                        // color: highlighted ? UI.colors.accent : UI.colors.textPrimary
                    }
                    AbracaImgButton {
                        id: tFavoriteIcon
                        visible: depth > 0
                        anchors.verticalCenter: tContentItem.verticalCenter
                        anchors.right: tContentItem.right

                        colorizationEnabled: checked === false || treeViewDelegate.highlighted
                        colorizationColor: treeViewDelegate.highlighted && isFavorite ? UI.colors.accent : UI.colors.inactive
                        icon.height: UI.iconSizeSmall
                        icon.width: UI.iconSizeSmall
                        checkable: true
                        checked: isFavorite
                        sourceChecked: UI.imagesUrl + "star.svg"
                        sourceUnchecked: UI.imagesUrl + "starEmpty.svg"
                        toolTipChecked: qsTr("Remove service from favorites")
                        toolTipUnchecked: qsTr("Add service to favorites")
                        onToggled: {
                            let modelIndex = serviceTree.index(row, column);
                            //console.log("Favorite toggled: " + checked + " for service index: " + index, modelIndex, row, column)
                            application.setServiceFavorite(modelIndex, checked);
                        }
                    }
                    AbracaLabel {
                        width: tFavoriteIcon.width
                        horizontalAlignment: Text.AlignHCenter
                        visible: depth === 0
                        anchors.verticalCenter: tContentItem.verticalCenter
                        anchors.right: tContentItem.right
                        text: channel
                        role: UI.LabelRole.Secondary
                        font.italic: true
                    }
                }
                indicator: Item {
                    id: indicatorItem
                    width: UI.controlHeight
                    height: UI.controlHeight
                    x: leftMargin + (treeViewDelegate.depth * treeViewDelegate.indentation)
                    AbracaColorizedImage {
                        id: indicatorImage
                        anchors.centerIn: indicatorItem
                        source: UI.imagesUrl + "chevron-right.svg"
                        width: UI.iconSize
                        height: UI.iconSize
                        colorizationColor: UI.colors.textSecondary
                        rotation: expanded ? 90 : 0
                        Behavior on rotation {
                            SmoothedAnimation {
                                duration: 150
                            }
                        }
                    }
                    TapHandler {
                        onSingleTapped: {
                            let index = treeView.index(row, column);
                            treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
                            treeView.toggleExpanded(row);
                        }
                    }
                }
                background: Rectangle {
                    anchors.fill: treeViewDelegate
                    radius: UI.controlRadius
                    color: treeViewDelegate.highlighted ? UI.colors.highlight : treeViewDelegate.hovered ? UI.colors.listItemHovered : "transparent"
                    anchors.leftMargin: treeViewDelegate.depth * treeViewDelegate.indentation
                }
            }
            ScrollIndicator.vertical: AbracaScrollIndicator {}
            Rectangle {
                id: treeHighlight
                visible: serviceTree.activeFocus && slTreeSelectionModel.currentIndex.valid
                z: 100
                color: "transparent"
                radius: UI.controlRadius
                border.color: UI.colors.accent
                border.width: 1
                height: UI.controlHeight
                width: serviceTree.width - UI.standardMargin - x
                x: serviceTree.currentRow >= 0 ? serviceTree.depth(serviceTree.currentRow) * (UI.controlHeight - 10) : 0
                y: serviceTree.currentRow >= 0 ? serviceTree.currentRow * UI.controlHeight : 0
                Behavior on y {
                    enabled: serviceTree.shouldAnimate
                    SmoothedAnimation {
                        velocity: 200
                    }
                }
                opacity: 1.0
                onYChanged: {
                    // Reset opacity when moving highlight
                    treeHighlight.opacity = 1.0;
                    treeHighlightFadeAnim.restart();
                }
                SequentialAnimation on opacity {
                    id: treeHighlightFadeAnim
                    running: serviceTree.activeFocus
                    loops: 1
                    PauseAnimation {
                        duration: 2000
                    }
                    NumberAnimation {
                        to: 0.2
                        duration: 300
                    }
                }
                Connections {
                    target: serviceTree
                    function onRestartTreeHighlightAnimation() {
                        treeHighlight.opacity = 1.0;
                        treeHighlightFadeAnim.restart();
                    }
                }
            }
        }
        AbracaHorizontalShadow {
            anchors.horizontalCenter: serviceTree.horizontalCenter
            anchors.top: serviceTree.top
            width: serviceTree.width
            topDownDirection: true
            shadowDistance: serviceTree.contentY
        }
        AbracaHorizontalShadow {
            anchors.horizontalCenter: serviceTree.horizontalCenter
            anchors.bottom: serviceTree.bottom
            width: serviceTree.width
            topDownDirection: false
            shadowDistance: serviceTree.contentHeight - serviceTree.height - serviceTree.contentY
        }
    }

    RowLayout {
        id: slRowLayout
        spacing: 0 // UI.standardMargin
        anchors.fill: parent

        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 0 // UI.standardMargin

            LayoutItemProxy {
                Layout.topMargin: UI.standardMargin
                target: ensembleInfo
            }
            LayoutItemProxy {
                Layout.topMargin: UI.standardMargin
                target: channelControl
            }
            AbracaTabBar {
                id: bar
                Layout.fillWidth: true
                visible: slStacked.visible
                AbracaTabButton {
                    text: qsTr("Services")
                }
                AbracaTabButton {
                    text: qsTr("Ensembles")
                }
            }
            StackLayout {
                id: slStacked
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: bar.currentIndex
                LayoutItemProxy {
                    target: serviceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: listWidth
                }
                LayoutItemProxy {
                    target: serviceTreeItem
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: listWidth
                }
            }
            LayoutItemProxy {
                target: serviceList
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: slStacked.visible ? 0 : UI.standardMargin
                Layout.minimumWidth: listWidth
                visible: slStacked.visible === false
            }
        }
        AbracaLine {
            visible: slStacked.visible === false
            Layout.fillHeight: true
            isVertical: true
            Layout.rightMargin: UI.standardMargin
            Layout.bottomMargin: UI.standardMargin
        }

        LayoutItemProxy {
            visible: slStacked.visible === false
            target: serviceTreeItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: listWidth
        }
    }

    function setFittingLayout() {
        if (mainItem.width < 2 * listWidth + slRowLayout.spacing + UI.standardMargin || isPortrait) {
            slStacked.visible = true;
            //slRowLayout.visible = false
        } else {
            slStacked.visible = false;
            //slRowLayout.visible = true
        }
    }
    onWidthChanged: setFittingLayout()
    Component.onCompleted: setFittingLayout()
}
