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

import abracaComponents

SplitView {
    id: control

    // If a pane becomes smaller than this, it snaps to 0
    property int snapThreshold: 0
    // Minimum width when pane is "visible"
    property int minVisibleSize: 120
    property int handleWidth: 0

    handle: Rectangle {
        id: handleItem
        implicitWidth: control.handleWidth > 0 ? control.handleWidth : UI.standardMargin * 1.5
        implicitHeight: control.handleWidth > 0 ? control.handleWidth : UI.standardMargin * 1.5
        color: UI.colors.background

        readonly property color indicatorColor: SplitHandle.pressed ? UI.colors.clicked
                                                                    : (SplitHandle.hovered ? UI.colors.hovered : UI.colors.inactive)

        SplitHandle.onPressedChanged: {
            if (!SplitHandle.pressed) {
                control.applySnapToZero()
            }
        }

        Rectangle {
            width: control.orientation === Qt.Vertical ? (handleItem.SplitHandle.hovered || handleItem.SplitHandle.pressed ? handleItem.width * 0.5 : UI.controlHeight): UI.standardMargin/2
            height: control.orientation === Qt.Vertical ? UI.standardMargin/2 : (handleItem.SplitHandle.hovered || handleItem.SplitHandle.pressed ? handleItem.height * 0.5 : UI.controlHeight)
            anchors.centerIn: handleItem
            radius: UI.controlRadius
            color: handleItem.indicatorColor
            Behavior on width {
                NumberAnimation { duration: 250 }
            }
            Behavior on height {
                NumberAnimation { duration: 250 }
            }
        }
        containmentMask: Item {
            x: control.orientation === Qt.Vertical ? 0 : (handleItem.width - width) / 2
            y: control.orientation === Qt.Vertical ? (handleItem.height - height) / 2 : 0
            width: control.orientation === Qt.Vertical ? handleItem.width : UI.controlHeight
            height: control.orientation === Qt.Vertical ? UI.controlHeight : handleItem.height
        }
    }

    // Snap logic called when handle released
    function applySnapToZero() {
        if (orientation === Qt.Horizontal)
            applyHorizontalSnap()
        else
            applyVerticalSnap()
    }

    function applyHorizontalSnap() {
        if (contentChildren.length !== 2 || control.snapThreshold === 0) {
            return
        }

        let leftItem = contentChildren[0]
        let rightItem = contentChildren[1]

        let total = control.width
        if (total <= 0)
            return

        let leftWidth = leftItem.width
        let rightWidth = rightItem.width

        // LEFT pane
        if (leftWidth < control.snapThreshold) {
            // hide left
            leftItem.SplitView.preferredWidth = 0
            rightItem.SplitView.preferredWidth = total
        } else if (leftWidth < control.minVisibleSize) {
            // enforce minimum “open” width
            leftItem.SplitView.preferredWidth = control.minVisibleSize
            rightItem.SplitView.preferredWidth = total - control.minVisibleSize
        } else {
            // keep current ratio
            leftItem.SplitView.preferredWidth = leftWidth
            rightItem.SplitView.preferredWidth = rightWidth
        }

        // LEFT pane
        if (rightWidth < control.snapThreshold) {
            // hide right
            leftItem.SplitView.preferredWidth = total
            rightItem.SplitView.preferredWidth = 0
        } else if (rightWidth < control.minVisibleSize) {
            // enforce minimum “open” width
            leftItem.SplitView.preferredWidth = total - control.minVisibleSize
            rightItem.SplitView.preferredWidth = control.minVisibleSize
        } else {
            // keep current ratio
            leftItem.SplitView.preferredWidth = leftWidth
            rightItem.SplitView.preferredWidth = rightWidth
        }
    }

    function applyVerticalSnap() {
        // Same idea, but use height and preferredHeight

        if (contentChildren.length !== 2 || control.snapThreshold === 0) {
            return
        }

        let topItem = contentChildren[0]
        let bottomItem = contentChildren[1]

        let total = control.height
        if (total <= 0)
            return

        let topHeight = topItem.height
        let bottomHeight = bottomItem.height

        // TOP pane
        if (topHeight < control.snapThreshold) {
            // hide left
            topItem.SplitView.preferredHeight = 0
            bottomItem.SplitView.preferredHeight = total
        } else if (topHeight < control.minVisibleSize) {
            // enforce minimum “open” width
            topItem.SplitView.preferredHeight = control.minVisibleSize
            bottomItem.SplitView.preferredHeight = total - control.minVisibleSize
        } else {
            // keep current ratio
            topItem.SplitView.preferredHeight = topHeight
            bottomItem.SplitView.preferredHeight = bottomHeight
        }

        // BOTTOM pane
        if (bottomHeight < control.snapThreshold) {
            // hide right
            topItem.SplitView.preferredHeight = total
            bottomItem.SplitView.preferredHeight = 0
        } else if (bottomHeight < control.minVisibleSize) {
            // enforce minimum “open” width
            topItem.SplitView.preferredHeight = total - control.minVisibleSize
            bottomItem.SplitView.preferredHeight = control.minVisibleSize
        } else {
            // keep current ratio
            topItem.SplitView.preferredHeight = topHeight
            bottomItem.SplitView.preferredHeight = bottomHeight
        }
    }

}
