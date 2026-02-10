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
// import QtQuick.Controls.Basic
import QtQuick.Layouts

import abracaComponents

Loader {
    id: dialogLoader

    property var messageBoxBackend

    active: messageBoxBackend ? messageBoxBackend.visible : false
    asynchronous: true

    sourceComponent: AbracaDialog {
        id: dialog

        modal: true
        visible: true

        contentItem: ColumnLayout {
            id: contentLayout
            spacing: 3*UI.standardMargin

            readonly property bool portraitView: messageBoxBackend && (messageBoxBackend.buttonOrientation === MessageBoxBackend.Vertical || messageBoxBackend.buttons.length <= 2)
            readonly property int maxWidth: portraitView ? messageBoxBackend.buttonOrientation === MessageBoxBackend.Vertical ? Math.max(buttonColumn.implicitWidth, 250)
                                                                                                                              : Math.max(buttonRow.implicitWidth, 250)
                                                        : 400

            // Keys.onReleased: event=>{
            //     if (event.key === Qt.Key_Escape) {
            //         // dialog.close();
            //         // dialog.rejected();
            //         // event.accepted = true;
            //         if (messageBoxBackend) {
            //             messageBoxBackend.handleEscapePressed();
            //             event.accepted = true;
            //         }
            //     }
            // }

            AbracaLabel {
                text: messageBoxBackend ? messageBoxBackend.message : ""
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.maximumWidth: contentLayout.maxWidth
                horizontalAlignment: contentLayout.portraitView ? Text.AlignHCenter : Text.AlignLeft
                font.bold: true
                font.pointSize: UI.biggerFontPointSize
            }
            AbracaLabel {
                text: messageBoxBackend ? messageBoxBackend.informativeText : ""
                visible: text.length > 0
                wrapMode: Text.WordWrap
                horizontalAlignment: contentLayout.portraitView ? Text.AlignHCenter : Text.AlignLeft
                Layout.fillWidth: true
                Layout.maximumWidth: contentLayout.maxWidth
                role: UI.LabelRole.Secondary
            }

            // Horizontal button layout
            RowLayout {
                id: buttonRow
                spacing: 10
                Layout.alignment: contentLayout.portraitView ? Qt.AlignHCenter : Qt.AlignRight
                Layout.fillWidth: contentLayout.portraitView
                visible: messageBoxBackend && messageBoxBackend.buttonOrientation === MessageBoxBackend.Horizontal                                

                Repeater {
                    model: messageBoxBackend ? messageBoxBackend.buttons : []

                    AbracaButton {
                        Layout.fillWidth: contentLayout.portraitView
                        text: modelData.text
                        // highlighted: messageBoxBackend && index === messageBoxBackend.defaultButtonIndex
                        //buttonRole: index === messageBoxBackend.defaultButtonIndex ? UI.ButtonRole.Primary : UI.ButtonRole.Neutral
                        buttonRole: {
                            switch (modelData.role) {
                                case MessageBoxBackend.Positive:
                                    return UI.ButtonRole.Positive;
                                case MessageBoxBackend.Negative:
                                    return UI.ButtonRole.Negative;
                                default:
                                    return index === messageBoxBackend.defaultButtonIndex ? UI.ButtonRole.Primary : UI.ButtonRole.Neutral;
                            }
                        }

                        onClicked: {
                            messageBoxBackend.handleButtonClicked(modelData.value);
                        }

                        // Handle Enter key for default button
                        Keys.onReturnPressed: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                messageBoxBackend.handleButtonClicked(modelData.value);
                            }
                        }
                        Keys.onEnterPressed: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                messageBoxBackend.handleButtonClicked(modelData.value);
                            }
                        }

                        Component.onCompleted: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                forceActiveFocus();
                            }
                        }
                    }
                }
            }

            // Vertical button layout
            ColumnLayout {
                id: buttonColumn
                spacing: 10
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                visible: messageBoxBackend && messageBoxBackend.buttonOrientation === MessageBoxBackend.Vertical

                Repeater {
                    model: messageBoxBackend ? messageBoxBackend.buttons : []

                    AbracaButton {
                        text: modelData.text
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                        Layout.minimumWidth: 150
                        // highlighted: messageBoxBackend && index === messageBoxBackend.defaultButtonIndex
                        buttonRole: index === messageBoxBackend.defaultButtonIndex ? UI.ButtonRole.Primary : UI.ButtonRole.Neutral
                        onClicked: {
                            messageBoxBackend.handleButtonClicked(modelData.value);
                        }

                        // Handle Enter key for default button
                        Keys.onReturnPressed: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                messageBoxBackend.handleButtonClicked(modelData.value);
                            }
                        }
                        Keys.onEnterPressed: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                messageBoxBackend.handleButtonClicked(modelData.value);
                            }
                        }

                        Component.onCompleted: {
                            if (messageBoxBackend && index === messageBoxBackend.defaultButtonIndex) {
                                forceActiveFocus();
                            }
                        }
                    }
                }
            }
        }
    }
}
