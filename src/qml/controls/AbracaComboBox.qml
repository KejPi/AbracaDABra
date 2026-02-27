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

ComboBox {
    id: control
    implicitContentWidthPolicy: ComboBox.WidestTextWhenCompleted

    property int delegateCount: 7
    editable: false
    property int elideMode: Text.ElideRight

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        enabled: !control.popup.opened
        onWheel: (event) => {
            let newIndex = control.currentIndex;
            if (event.angleDelta.y > 0) {
                decrementCurrentIndex();
            } else if (event.angleDelta.y < 0) {
                incrementCurrentIndex();
            }
        }
    }

    indicator: AbracaColorizedImage {
        id: indicatorImage
        source: UI.imagesUrl + "chevron-down.svg"
        colorizationColor: UI.colors.textSecondary
        width: UI.iconSize
        height: UI.iconSize
        fillMode: Image.PreserveAspectFit
        sourceSize.width: width
        sourceSize.height: height
        x: control.width - width //- control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2

        rotation: control.down ? 180 : 0
        Behavior on rotation {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }
    }
    contentItem: TextInput { // TextInput is needed for implicitContentWidthPolicy (Qt doc)
        id: contentInput
        leftPadding: UI.standardMargin
        verticalAlignment: Text.AlignVCenter
        enabled: control.editable
        readOnly: control.popup.opened
        selectByMouse: control.editable
        text: control.displayText
        color: "transparent"
        TextMetrics {
            id: metrics
            font: contentInput.font
            text: control.displayText
            elideWidth: contentInput.width - contentInput.leftPadding - contentInput.rightPadding - control.spacing
            elide: control.elideMode
        }
        AbracaLabel {
            id: visibleLabel
            color: UI.colors.textPrimary
            anchors.left: parent.left
            anchors.leftMargin: contentInput.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            text: metrics.elidedText
        }
    }
    background: Rectangle {
        implicitWidth:  UI.controlWidth
        implicitHeight: UI.controlHeight
        border.color: control.enabled ? (control.pressed ? UI.colors.clicked : UI.colors.inactive) : UI.colors.disabled
        border.width: control.visualFocus ? 2 : 1
        radius: UI.controlRadius
        color: UI.colors.inputBackground
    }

    popup: Popup {
        id: popupMenu
        y: control.height + 1
        width: control.width
        height: (implicitHeight > 5*control.height) ? Math.min(implicitHeight, control.Window.height - y - control.mapToItem(null, 0, 0).y) : implicitHeight
        padding: 1
        contentItem: ListView {
            clip: true
            implicitHeight: ((control.delegateCount > 0 && count > control.delegateCount) ? control.delegateCount * control.height : contentHeight) // 2*padding
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            border.color: UI.colors.inactive
            color: UI.colors.inputBackground            
            radius: UI.controlRadius
        }
    }

    delegate: ItemDelegate {
        id: delegate

        required property var model
        required property int index

        width: control.width - 2
        height: control.height
        contentItem: AbracaLabel {
            text: delegate.model[control.textRole]
            elide: control.elideMode
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: control.highlightedIndex === index
        background: Rectangle {            
            anchors.fill: delegate
            radius: UI.controlRadius
            color: delegate.highlighted ? UI.colors.highlight : "transparent"
        }
    }
}
