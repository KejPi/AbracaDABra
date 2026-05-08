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

#include "win.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QMetaObject>

static HHOOK   s_keyboardHook = nullptr;
static QObject *s_app         = nullptr;

static LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && s_app)
    {
        const KBDLLHOOKSTRUCT *kbd = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        switch (kbd->vkCode)
        {
        case VK_MEDIA_PLAY_PAUSE:
            QMetaObject::invokeMethod(s_app, "toggleMute", Qt::QueuedConnection);
            return 1;   // consumed — prevent duplicate handling by other apps
        case VK_MEDIA_NEXT_TRACK:
            QMetaObject::invokeMethod(s_app, "onNextFavoriteService", Qt::QueuedConnection);
            return 1;
        case VK_MEDIA_PREV_TRACK:
            QMetaObject::invokeMethod(s_app, "onPreviousFavoriteService", Qt::QueuedConnection);
            return 1;
        default:
            break;
        }
    }
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

void winSetupMediaRemoteCommands(QObject *app)
{
    s_app          = app;
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardProc, nullptr, 0);
}

void winTeardownMediaRemoteCommands()
{
    if (s_keyboardHook)
    {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
    s_app = nullptr;
}
