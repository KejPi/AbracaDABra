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

// Windows-only console-subsystem launcher for --cli usage.
//
// AbracaDABra.exe is built with the WIN32 (GUI) subsystem so double-clicking
// it doesn't pop a console window. But cmd.exe/PowerShell don't wait for
// GUI-subsystem child processes, and Windows doesn't attach a console to a
// GUI-subsystem process at all unless one is explicitly wired up. That
// combination breaks --cli usage: cmd returns to its own prompt immediately
// (racing the child for keyboard input) and the child starts with no
// working console I/O.
//
// This launcher is a real CONSOLE-subsystem process, installed alongside
// AbracaDABra.exe as AbracaDABra.com. Windows prefers .com over .exe when a
// bare command name is typed, so "AbracaDABra --cli ..." picks this up
// automatically from cmd.exe/PowerShell, while Explorer double-clicks still
// resolve to AbracaDABra.exe unaffected.
//
// Being console-subsystem, cmd.exe waits for it properly (fixing the
// dual-reader race), and it explicitly hands its own valid console handles
// to the relaunched GUI-subsystem child via STARTUPINFO/bInheritHandles,
// giving that child working stdin/stdout/stderr without needing
// AttachConsole/AllocConsole games inside the app itself.

#include <windows.h>
#include <string>

int main() {
    wchar_t exePath[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return 1;
    }

    std::wstring targetPath(exePath);
    size_t pos = targetPath.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return 1;
    }
    targetPath = targetPath.substr(0, pos + 1) + L"AbracaDABra.exe";

            // Forward our own command line verbatim (minus our own program name)
            // to the real GUI-subsystem executable.
    const wchar_t* fullCmdLine = GetCommandLineW();
    const wchar_t* p = fullCmdLine;
    if (*p == L'"') {
        ++p;
        while (*p && *p != L'"') ++p;
        if (*p == L'"') ++p;
    } else {
        while (*p && *p != L' ' && *p != L'\0') ++p;
    }
    while (*p == L' ') ++p;

    std::wstring cmdLine = L"\"" + targetPath + L"\"";
    if (*p) {
        cmdLine += L" ";
        cmdLine += p;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(
        targetPath.c_str(),
        cmdLine.data(),   // writable buffer required by CreateProcessW
        nullptr, nullptr,
        TRUE,             // bInheritHandles
        0,                // no CREATE_NEW_CONSOLE / DETACHED_PROCESS
        nullptr, nullptr,
        &si, &pi);

    if (!ok) {
        return static_cast<int>(GetLastError());
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}
