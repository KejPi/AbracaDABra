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
                index: index
                text: itemName
            }
        }
    }
    StackLayout {
        anchors.top: bar.bottom
        anchors.bottom: mainItem.bottom
        anchors.left: mainItem.left
        anchors.right: mainItem.right

        // anchors.horizontalCenter: mainItem.horizontalCenter
        // width: Math.min(mainItem.width, 700)
        currentIndex: bar.currentIndex
        DeviceSettings {
            id: deviceTab
        }
        AudioSettings {
            id: audioTab
        }
        AnnouncementSettings {
            id: announcementTab
        }
        UaSettings {
            id: uaTab
        }
        TiiSettings {
            id: tiiTab
        }
        OtherSettings {
            id: otherTab
        }
    }
}
