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
import abracaComponents

Label {
    id: control

    property bool linkEnabled: true
    property string toolTipText: ""
    property alias mouseArea: mouseArea

    property int role: UI.LabelRole.Primary

    signal rightClick()

    color: enabled ? role === UI.LabelRole.Secondary ? UI.colors.textSecondary : UI.colors.textPrimary
                  : UI.colors.textDisabled
    linkColor: enabled ? UI.colors.link : UI.colors.textDisabled
    onLinkActivated: (link)=>{
        if (control.linkEnabled) {
            application.openLink(link)
        }
    }

    AbracaToolTip {
        text: toolTipText
        hoverMouseArea: mouseArea
    }

    MouseArea {
        id: mouseArea
        enabled: linkEnabled || toolTipText.length > 0
        anchors.fill: control
        hoverEnabled: toolTipText.length > 0        
        acceptedButtons: Qt.RightButton
        cursorShape: control.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: (mouse)=> { if (mouse.button === Qt.RightButton) rightClick() }
    }
}

