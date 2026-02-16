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

Item {
    id: mainItem

    property int navigationId: -1
    property alias sourceComponent: loader.sourceComponent
    property var backend
    property bool backendInitialized: false
    signal itemLoaded()

    Component.onCompleted: {
        if (navigationModel.currentId === mainItem.navigationId) {
            loadItem()
        }
    }
    Component.onDestruction: {
        destroyItem()
    }

    function loadItem() { // create component
        if (!mainItem.backendInitialized) {
            mainItem.backend = application.createBackend(mainItem.navigationId);
            mainItem.backendInitialized = true;
        }
        loader.active = true;
    }
    function destroyItem() { // destroy component
        if (mainItem.backendInitialized) {
            mainItem.backend.destroy()
            mainItem.backendInitialized = false;
        }
        loader.active = false
    }

    Connections {
        target: navigationModel
        function onCurrentIdChanged() {
            if (navigationModel.currentId === mainItem.navigationId) {
                loadItem()
            }
            else {
                destroyItem()
            }
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        active: false
        asynchronous: true
        onLoaded: mainItem.itemLoaded()
    }
}
