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

import abracaComponents

Item {
    id: mainItem
    property int currentDevice: 0

    GridLayout {
        id: inputDeviceSelectionRow
        anchors.left: mainItem.left
        anchors.right: mainItem.right
        anchors.top: mainItem.top
        anchors.margins: UI.standardMargin
        columnSpacing: UI.standardMargin
        rowSpacing: UI.standardMargin

        readonly property bool mobileView: mainItem.width <= (inputDeviceLabel.implicitWidth + deviceSelectionCombo.implicitWidth
                                                              + connectButton.implicitWidth + statusLabel.implicitWidth + 8*UI.standardMargin)
        columns: mobileView ? 5 : 6
        AbracaLabel {
            id: inputDeviceLabel
            text: qsTr("Input device:")
        }
        Item { Layout.fillHeight: true; Layout.fillWidth: inputDeviceSelectionRow.mobileView;}
        AbracaComboBox {
            id: deviceSelectionCombo
            enabled: settingsBackend.isInputDeviceSelectionEnabled
            model: settingsBackend.inputsModel
            textRole: "itemName"
            valueRole: "itemData"
            onCurrentIndexChanged: {
                if (currentIndex >= 0) {
                    var deviceId = model.data(model.index(currentIndex, 0), ItemModel.DataRole)
                    mainItem.currentDevice = deviceId
                }
            }
            onActivated: {
                model.currentIndex = currentIndex
                contentFlickable.contentY = 0
            }
            Component.onCompleted: {
                currentIndex = model.currentIndex
            }

            property int w: -1
            Layout.preferredWidth: w > 0 ? w : implicitWidth
            onImplicitWidthChanged: {
                if (implicitWidth > w) {
                    w = implicitWidth
                }
            }
        }
        Item { Layout.fillHeight: true; Layout.fillWidth: inputDeviceSelectionRow.mobileView;  }
        AbracaButton {
            id: connectButton
            text: settingsBackend.isConnectButton ? qsTr("Connect") : qsTr("Disconnect")
            enabled: settingsBackend.isConnectButtonEnabled
            onClicked: settingsBackend.requestConnectDisconnectDevice()
            buttonRole: UI.ButtonRole.Primary
        }
        AbracaLabel {
            id: statusLabel
            Layout.columnSpan: inputDeviceSelectionRow.mobileView ? 5 : 1
            Layout.fillWidth: true
            text: settingsBackend.statusLabel
            textFormat: Text.RichText
            horizontalAlignment: inputDeviceSelectionRow.mobileView ? Text.AlignHCenter : Text.AlignRight
        }
    }
    AbracaLine {
        id: line
        anchors.left: mainItem.left
        anchors.right: mainItem.right
        anchors.top: inputDeviceSelectionRow.bottom
        anchors.topMargin: UI.standardMargin
        isVertical: false
    }
    Flickable {
        id: contentFlickable
        anchors.top: line.bottom
        anchors.left: mainItem.left
        anchors.right: mainItem.right
        anchors.bottom: mainItem.bottom
        // anchors.margins: UI.standardMargin
        // anchors.horizontalCenter: mainItem.horizontalCenter
        // width: Math.min(contentWidth, mainItem.width)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: inputDeviceSettingsLayout.implicitWidth
        contentHeight: getContentHeight() // inputDeviceSettingsLayout.implicitHeight

        Item {
            id: contentContainer
            width: contentFlickable.width
            height: inputDeviceSettingsLayout.implicitHeight

            StackLayout {
                id: inputDeviceSettingsLayout
                anchors.horizontalCenter: parent.horizontalCenter

                currentIndex: mainItem.currentDevice

                // order of items must match InputDevice::Id
                Item { }  // undefined
                RtlSdrDevice {
                    id: rtlSdrDeviceSettings;
                    Layout.preferredWidth: Math.max(Math.min(contentFlickable.width, 700), implicitWidth)
                }
                RtlTcpDevice {
                    id: rtlTcpDeviceSettings;
                    Layout.preferredWidth: Math.max(Math.min(contentFlickable.width, 700), implicitWidth)
                }
                RawFileDevice {
                    id: rawFileDeviceSettings;
                    Layout.preferredWidth: Math.max(Math.min(contentFlickable.width, 700), implicitWidth)
                }
                Item {
                    Layout.preferredWidth: airspyDeviceSettingsLoader.item ? Math.max(Math.min(contentFlickable.width, 700), airspyDeviceSettingsLoader.item.implicitWidth) : 0
                    Loader {
                        id: airspyDeviceSettingsLoader
                        anchors.fill: parent
                        active: settingsBackend.haveAirspy
                        sourceComponent: AirspyDevice { id: airspyDeviceSettings }
                    }
                }
                Item {
                    Layout.preferredWidth: sdrPlayDeviceSettingsLoader.item ? Math.max(Math.min(contentFlickable.width, 700), sdrPlayDeviceSettingsLoader.item.implicitWidth) : 0
                    Loader {
                        id: sdrPlayDeviceSettingsLoader
                        anchors.fill: parent
                        active: settingsBackend.haveSoapySdr
                        sourceComponent: SdrPlayDevice { id: sdrPlayDeviceSettings  }
                    }
                }
                Item {
                    Layout.preferredWidth: soapySdrDeviceSettingsLoader.item ? Math.max(Math.min(contentFlickable.width, 700), soapySdrDeviceSettingsLoader.item.implicitWidth) : 0
                    Loader {
                        id: soapySdrDeviceSettingsLoader
                        anchors.fill: parent
                        active: settingsBackend.haveSoapySdr
                        sourceComponent: SoapySdrDevice { id: soapySdrDeviceSettings  }
                    }
                }
                RartTcpDevice {
                    id: rartTcpDeviceSettings
                    Layout.preferredWidth: Math.max(Math.min(contentFlickable.width, 700), implicitWidth)
                }
            }
        }
        function getContentHeight() {
            switch (mainItem.currentDevice) {
            case 1: return rtlSdrDeviceSettings.implicitHeight
            case 2: return rtlTcpDeviceSettings.implicitHeight
            case 3: return rawFileDeviceSettings.implicitHeight
            case 4: return airspyDeviceSettingsLoader.item.implicitHeight
            case 5: return sdrPlayDeviceSettingsLoader.item.implicitHeight
            case 6: return soapySdrDeviceSettingsLoader.item.implicitHeight
            case 7: return rartTcpDeviceSettings.implicitHeight
            }
            return  0
        }
        ScrollIndicator.vertical: AbracaScrollIndicator { }
        ScrollIndicator.horizontal: AbracaScrollIndicator { }
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: line.bottom
        width: inputDeviceSettingsLayout.width
        topDownDirection: true
        shadowDistance: contentFlickable.contentY
    }
    AbracaHorizontalShadow {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: inputDeviceSettingsLayout.width
        topDownDirection: false
        shadowDistance: contentFlickable.contentHeight - contentFlickable.height - contentFlickable.contentY
    }
}
