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
import QtQuick.Window
// import QtQuick.Controls.Material
import abracaComponents

Window {
    id: detachedWindow

    property string pageId: ""
    property string windowTitle: ""
    property var parentItem: null
    property alias container: pageContainer
    property bool isPageEnabled: true

    width: 600
    height: 500

    title: windowTitle

    flags: Qt.Window | Qt.WindowCloseButtonHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint

    onIsPageEnabledChanged: {
        if (isPageEnabled === false) { close() }
    }

    onClosing: {
        if (parentItem) {
            parentItem.dockPage(pageId);
        }
    }

    Component.onCompleted: {
        // Center the window on the screen
        if (x < 0) {
            x = Screen.width / 2 - width / 2;
        }
        if (y < 0) {
            y = Screen.height / 2 - height / 2;
        }
    }
    AbracaPane {
        id: pageContainer
        anchors.fill: parent
    }
}
