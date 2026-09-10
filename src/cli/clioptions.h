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

#ifndef CLIOPTIONS_H
#define CLIOPTIONS_H

#include <QCoreApplication>

#include "dabcliapp.h"

// Parses the process's command-line arguments for --cli mode into *config (extends CliConfig
// with the resolved verboseLevel/commandlineOnly flags). Mirrors the option set of the original
// standalone AbracaDABra-cli tool (device/serial/rtltcp-host/port/channel/frequency/sid/scids/
// service/bind/port/gain/hardware-agc/list-channels/commandline-only/verbose), plus --cli itself
// and the shared --help.
//
// Returns -1 if the caller should proceed to start DabCliApp with *config populated; otherwise
// returns the process exit code the caller should return immediately (e.g. 0 after --help,
// --list-channels or a bare -v/--version; 1 on an invalid option/value). QCommandLineParser
// itself handles --help and unknown-option errors by exiting the process directly.
int parseCliArguments(QCoreApplication &app, CliConfig *config);

#endif  // CLIOPTIONS_H
