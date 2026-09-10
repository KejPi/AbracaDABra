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

#include "clirunner.h"

#include <atomic>
#include <csignal>
#include <memory>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include <QElapsedTimer>
#include <QSocketNotifier>
#include <QTimer>

#include "config.h"

#if HAVE_FTXUI
#include "clitui.h"

// set once the TUI is up and running; used by cliTuiMessageHandler() below to route log output
// into the TUI's log panel instead of stderr (which the TUI's alternate screen is covering)
static CliTui *g_tui = nullptr;

static void cliTuiMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString formatted = qFormatLogMessage(type, context, msg);
    if (g_tui)
    {
        g_tui->appendLogLine(formatted);
    }
    else
    {
        fprintf(stderr, "%s\n", qPrintable(formatted));
    }
}
#endif

static std::atomic<bool> g_quitRequested{false};

static void handleSignal(int)
{
    g_quitRequested.store(true);
}

int runCliApplication(QCoreApplication &app, const CliConfig &config)
{
    DabCliApp cliApp(config);
    if (!cliApp.start())
    {
        return 1;
    }

#if HAVE_FTXUI
    // shown instead of the web UI when the user didn't explicitly ask for one; falls back to
    // plain logging (below) if stdout isn't an interactive terminal (e.g. output redirected)
    std::unique_ptr<CliTui> tui;
    if (!config.webUiEnabled && !config.commandlineOnly)
    {
        tui = std::make_unique<CliTui>(&cliApp, config.verboseLevel);
        if (tui->start())
        {
            g_tui = tui.get();
            qInstallMessageHandler(cliTuiMessageHandler);
            QObject::connect(tui.get(), &CliTui::quitRequested, &app, [&]() { g_quitRequested.store(true); });
        }
        else
        {
            tui.reset();
        }
    }
#endif

#ifndef _WIN32
    // when not showing the interactive TUI (commandline-only/-vv/-vvv, or the TUI failed to start,
    // e.g. non-tty stdout), still allow basic keyboard control (same keys as the TUI) from a tty
    bool commandlineKeysActive = !config.webUiEnabled;
#if HAVE_FTXUI
    commandlineKeysActive = commandlineKeysActive && !tui;
#endif
    struct termios origTermios;
    bool termiosSaved = false;
    std::unique_ptr<QSocketNotifier> stdinNotifier;
    if (commandlineKeysActive && isatty(STDIN_FILENO))
    {
        struct termios raw;
        if (0 == tcgetattr(STDIN_FILENO, &origTermios))
        {
            raw = origTermios;
            raw.c_lflag &= ~(ICANON | ECHO);  // read keys immediately, without waiting for Enter
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (0 == tcsetattr(STDIN_FILENO, TCSANOW, &raw))
            {
                termiosSaved = true;
            }
        }
        if (termiosSaved)
        {
            stdinNotifier = std::make_unique<QSocketNotifier>(STDIN_FILENO, QSocketNotifier::Read);
            QObject::connect(stdinNotifier.get(), &QSocketNotifier::activated, &app,
                              [&cliApp]()
                              {
                                  char buf[64];
                                  ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
                                  for (ssize_t i = 0; i < n; ++i)
                                  {
                                      const char c = buf[i];
                                      if ('+' == c || '=' == c)
                                      {
                                          cliApp.setVolume(cliApp.volumePercent() + 5);  // matches CliTui::kVolumeStep
                                          printf("volume: %d%%\n", cliApp.volumePercent());
                                          fflush(stdout);
                                      }
                                      else if ('-' == c || '_' == c)
                                      {
                                          cliApp.setVolume(cliApp.volumePercent() - 5);
                                          printf("volume: %d%%\n", cliApp.volumePercent());
                                          fflush(stdout);
                                      }
                                      else if ('m' == c || 'M' == c)
                                      {
                                          cliApp.toggleMute();
                                          printf("volume: %d%%\n", cliApp.volumePercent());
                                          fflush(stdout);
                                      }
                                      else if ('q' == c || 3 == c)
                                      {
                                          g_quitRequested.store(true);
                                      }
                                  }
                              });
        }
    }
#endif

    // periodically check for Ctrl+C / SIGTERM so we can shut down the DAB engine cleanly
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    QObject::connect(&cliApp, &DabCliApp::readyToQuit, &app, &QCoreApplication::quit);

    bool shutdownRequested = false;
    QElapsedTimer shutdownTimer;
    QTimer quitTimer;
    QObject::connect(&quitTimer, &QTimer::timeout, &app,
                      [&]()
                      {
                          if (!g_quitRequested.load())
                          {
                              return;
                          }
                          if (!shutdownRequested)
                          {
                              shutdownRequested = true;
                              shutdownTimer.start();
                              // gracefully tune the DAB engine to idle before quitting; readyToQuit()
                              // then triggers app.quit(). Fall back to a hard quit below if that
                              // handshake doesn't complete in time (e.g. device already gone).
                              cliApp.requestShutdown();
                          }
                          else if (shutdownTimer.hasExpired(3000))
                          {
                              app.quit();
                          }
                      });
    quitTimer.start(200);

    int ret = app.exec();
#ifndef _WIN32
    if (termiosSaved)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
    }
#endif
    return ret;
}
