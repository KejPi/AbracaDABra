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

pragma Singleton
import QtQuick
import QtQuick.Controls
import abracaComponents

QtObject {
    enum ControlRole {
        Primary = 0,
        Secondary = 1
    }
    enum LabelRole {
        Primary = 0,
        Secondary = 1
    }
    enum ButtonRole {
        Primary = 0,
        Neutral = 1,
        Positive = 2,
        Negative = 3
    }
    enum ButtonType {
        Normal = 0,
        IconOnly = 1
    }

    readonly property bool isMacOS: Qt.platform.os === "osx" || Qt.platform.os === "macos"
    readonly property bool isWindows: Qt.platform.os === "windows"
    readonly property bool isLinux: Qt.platform.os === "linux"
    readonly property bool isAndroid: Qt.platform.os === "android"

    readonly property url imagesUrl: "qrc:/qt/qml/abracaComponents/qml/images/"
    readonly property real scaleFactor: Screen.devicePixelRatio

    readonly property int narrowViewWidth: 440
    readonly property bool isDesktop: !isAndroid
    readonly property bool isMobile: isAndroid && (Math.min(Screen.width, Screen.height) < narrowViewWidth)

    readonly property int standardMargin: 8
    readonly property int controlHeight: isAndroid ? 40 : 32
    readonly property int controlHeightSmaller: isAndroid ? 30 : 24
    readonly property int controlHeightSmall: isAndroid ? 20 : 16
    readonly property int controlWidth: 100
    readonly property int controlRadius: isAndroid ? 6 : 4
    readonly property int iconSize: isAndroid ? 24 : 20
    readonly property int iconSizeSmall: isAndroid ? 20 : 16

    // Navigation buttons
    readonly property int navigationButtonSize: isAndroid ? 54 : 60
    readonly property int navigationSmallButtonSize: isAndroid ? 48 : 44

    readonly property int smallFontPointSize: Qt.application.font.pointSize - 2
    readonly property int biggerFontPointSize: Qt.application.font.pointSize + 2
    readonly property int largeFontPointSize: Qt.application.font.pointSize + 4
    readonly property int toolTipOffsetX: 15
    readonly property int toolTipOffsetY: 15

    readonly property string fixedFontFamily: appUI.fixedFontFamily

    // Nested colors object
    readonly property QtObject colors: QtObject {
        readonly property color background: appUI.colors[ApplicationUI.Background]
        readonly property color backgroundLight: appUI.colors[ApplicationUI.BackgroundLight]
        readonly property color backgroundDark: appUI.colors[ApplicationUI.BackgroundDark]
        readonly property color statusbarBackground: appUI.colors[ApplicationUI.StatusbarBackground]
        readonly property color divider: appUI.colors[ApplicationUI.Divider]

        readonly property color accent: appUI.colors[ApplicationUI.Accent]
        readonly property color disabled: appUI.colors[ApplicationUI.Disabled]
        readonly property color hovered: appUI.colors[ApplicationUI.Hovered]
        readonly property color clicked: appUI.colors[ApplicationUI.Clicked]
        readonly property color highlight: appUI.colors[ApplicationUI.Highlight]
        readonly property color inactive: appUI.colors[ApplicationUI.Inactive]
        readonly property color selectionColor: appUI.colors[ApplicationUI.SelectionColor]

        readonly property color listItemHovered: appUI.colors[ApplicationUI.ListItemHovered]
        readonly property color listItemSelected: appUI.colors[ApplicationUI.ListItemSelected]

        readonly property color buttonPrimary: appUI.colors[ApplicationUI.ButtonPrimary]
        readonly property color buttonNeutral: appUI.colors[ApplicationUI.ButtonNeutral]
        readonly property color buttonPositive: appUI.colors[ApplicationUI.ButtonPositive]
        readonly property color buttonNegative: appUI.colors[ApplicationUI.ButtonNegative]

        readonly property color buttonTextPrimary: appUI.colors[ApplicationUI.ButtonTextPrimary]
        readonly property color buttonTextNeutral: appUI.colors[ApplicationUI.ButtonTextNeutral]
        readonly property color buttonTextPositive: appUI.colors[ApplicationUI.ButtonTextPositive]
        readonly property color buttonTextNegative: appUI.colors[ApplicationUI.ButtonTextNegative]

        readonly property color buttonPrimaryHover: appUI.colors[ApplicationUI.ButtonPrimaryHover]
        readonly property color buttonNeutralHover: appUI.colors[ApplicationUI.ButtonNeutralHover]
        readonly property color buttonPositiveHover: appUI.colors[ApplicationUI.ButtonPositiveHover]
        readonly property color buttonNegativeHover: appUI.colors[ApplicationUI.ButtonNegativeHover]

        readonly property color buttonPrimaryClicked: appUI.colors[ApplicationUI.ButtonPrimaryClicked]
        readonly property color buttonNeutralClicked: appUI.colors[ApplicationUI.ButtonNeutralClicked]
        readonly property color buttonPositiveClicked: appUI.colors[ApplicationUI.ButtonPositiveClicked]
        readonly property color buttonNegativeClicked: appUI.colors[ApplicationUI.ButtonNegativeClicked]

        readonly property color textPrimary: appUI.colors[ApplicationUI.TextPrimary]
        readonly property color textSecondary: appUI.colors[ApplicationUI.TextSecondary]
        readonly property color textDisabled: appUI.colors[ApplicationUI.TextDisabled]
        readonly property color textSelected: appUI.colors[ApplicationUI.TextSelected]
        readonly property color link: appUI.colors[ApplicationUI.Link]

        readonly property color icon: appUI.colors[ApplicationUI.Icon]
        readonly property color iconInactive: appUI.colors[ApplicationUI.IconInactive]
        readonly property color iconDisabled: appUI.colors[ApplicationUI.IconDisabled]

        readonly property color inputBackground: appUI.colors[ApplicationUI.InputBackground]

        readonly property color controlBackground: appUI.colors[ApplicationUI.ControlBackground]
        readonly property color controlBorder: appUI.colors[ApplicationUI.ControlBorder]

        readonly property color noSignal: appUI.colors[ApplicationUI.NoSignal]
        readonly property color lowSignal: appUI.colors[ApplicationUI.LowSignal]
        readonly property color midSignal: appUI.colors[ApplicationUI.MidSignal]
        readonly property color goodSignal: appUI.colors[ApplicationUI.GoodSignal]

        readonly property color epgCurrentProgColor: appUI.colors[ApplicationUI.EpgCurrentProgColor]
        readonly property color epgCurrentProgProgressColor: appUI.colors[ApplicationUI.EpgCurrentProgProgressColor]

        readonly property color emptyLogoColor: appUI.colors[ApplicationUI.EmptyLogoColor]
    }
}
