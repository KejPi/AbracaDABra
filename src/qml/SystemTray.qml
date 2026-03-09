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
import Qt.labs.platform

import abracaComponents

SystemTrayIcon {
    id: trayIcon

    property var appWindow
    property var application
    property var appUI
    property var settingsBackend

    visible: settingsBackend.showTrayIcon
    icon.mask: UI.isWindows === false
    icon.source: UI.isWindows ? "qrc:/resources/appIcon.png" : UI.imagesUrl + "trayIcon.svg"

    menu: Menu {
        MenuItem {
            text: appUI.isMuted ? qsTr("Unmute") : qsTr("Mute")
            onTriggered: {
                application.onMuteButtonToggled(!appUI.isMuted);
            }
        }
        MenuItem {
            text: qsTr("Quit")
            onTriggered: application.close()
        }
    }

    onActivated: {
        appWindow.show();
        appWindow.raise();
        appWindow.requestActivate();
    }
}
