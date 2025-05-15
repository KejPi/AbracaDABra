/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
import Qt5Compat.GraphicalEffects
// import QtQuick.Effects              // not available in Qt < 6.5

Item {
    property bool topDownDirection: false
    height: 50
    clip: true

    Rectangle {
        id: shadowItem
        color: "black"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        radius: 8
        height: 20
        y: topDownDirection ? -height : parent.height
        visible: false
    }
    // Qt >= 6.5
    /*
    MultiEffect {
        source: shadowItem
        anchors.fill: shadowItem
        autoPaddingEnabled: true
        shadowBlur: 1.0
        shadowColor: 'black'
        shadowEnabled: true
        shadowOpacity: 0.5
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
        visible: true
    }
    */
    DropShadow {
        id: shadowId
        anchors.fill: shadowItem
        source: shadowItem
        //samples: 40
        radius: 10
        color: EPGColors.shadowColor
        enabled: true
        opacity: 0.5
        horizontalOffset: 0
        verticalOffset: 0
        transparentBorder: true
        visible: true
    }
}
