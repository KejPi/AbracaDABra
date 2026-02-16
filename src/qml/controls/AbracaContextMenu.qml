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
//import QtQuick.Controls.Basic
// import QtQuick.Controls

import abracaComponents 1.0

// Context menu populated from C++ backend
AbracaMenu {
    id: contextMenu
    property alias menuModel: instantiator.model
    property int menuWidth: implicitWidth

    width: menuWidth

    Instantiator {
        id: instantiator

        AbracaMenuItem {
            text: model.text
            enabled: model.enabled
            checkable: model.checkable
            checked: model.checked

            onTriggered: {
                // Call back to C++ to handle the action
                contextMenu.menuModel.triggerAction(index)
            }
        }

        onObjectAdded: function(index, object) {
            contextMenu.insertItem(index, object)
            calcWidth()
        }

        onObjectRemoved: function(index, object) {
            contextMenu.removeItem(object)
            calcWidth()
        }
    }
    function calcWidth() {
        var result = 0;
        var padding = 20;
        for (var i = 0; i < count; ++i) {
            var item = itemAt(i);
            result = Math.max(item.contentItem.implicitWidth, result);
            padding = Math.max(item.padding, padding);
        }
        contextMenu.menuWidth = result + padding * 2;
    }
}
