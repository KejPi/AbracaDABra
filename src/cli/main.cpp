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

#include <algorithm>
#include <atomic>
#include <csignal>
#include <memory>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QTimer>

#include "config.h"
#include "dabcliapp.h"
#include "dabtables.h"

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

// androidfilehelper.cpp uses the "application" logging category, which is normally defined in
// application.cpp (the GUI app entry point). Since the CLI target doesn't link application.cpp
// (to avoid pulling in the whole QML/GUI dependency chain), define it here instead.
Q_LOGGING_CATEGORY(application, "Application", QtInfoMsg)

static void handleSignal(int)
{
    g_quitRequested.store(true);
}

static void printChannelList()
{
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        printf("%-4s %8.3f MHz\n", qPrintable(it.value()), it.key() / 1000.0);
    }
}

int main(int argc, char *argv[])
{
    // SlideShowApp decodes MOT slides via QPixmap, which needs a QPA platform plugin even though
    // this is a headless server; "offscreen" provides one without requiring a real display.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QCoreApplication::setApplicationName("AbracaDABra-cli");
    QCoreApplication::setApplicationVersion(PROJECT_VER);
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Headless DAB/DAB+ receiver with a web UI, built on the AbracaDABra DAB engine.");
    parser.addHelpOption();
    // not parser.addVersionOption(): it auto-exits on -v/--version before we can tell a single -v
    // apart from repeated -vv/-vvv, which we count below to control verbosity instead
    QCommandLineOption versionOpt(QStringList() << "v" << "version", "Displays version information and exits.");
    parser.addOption(versionOpt);

    QCommandLineOption deviceOpt(QStringList() << "d" << "device", "Input device: rtlsdr (default) or rtltcp.", "device", "rtlsdr");
    QCommandLineOption serialOpt("serial", "RTL-SDR device serial number or index (default: first device found).", "serial");
    QCommandLineOption rtlTcpHostOpt("rtltcp-host", "rtl_tcp server host (default: 127.0.0.1).", "host", "127.0.0.1");
    QCommandLineOption rtlTcpPortOpt("rtltcp-port", "rtl_tcp server port (default: 1234).", "port", "1234");

    QCommandLineOption channelOpt(QStringList() << "c" << "channel", "DAB channel to tune, e.g. 12C.", "channel");
    QCommandLineOption freqOpt(QStringList() << "f" << "frequency", "Frequency in kHz (alternative to --channel).", "kHz");

    QCommandLineOption sidOpt("sid", "Initial service to select (SId), decimal or 0x-prefixed hex.", "sid");
    QCommandLineOption scidsOpt("scids", "Service component id within service (default: 0).", "scids", "0");
    QCommandLineOption serviceOpt(QStringList() << "s" << "service", "Initial service to select by (partial, case-insensitive) label.",
                                   "label");

    QCommandLineOption bindOpt("bind", "Web UI bind address (default: 0.0.0.0). If neither --bind nor --port is given, an "
                                        "interactive terminal dashboard is shown instead of the web UI.",
                                "address", "0.0.0.0");
    QCommandLineOption portOpt(QStringList() << "p" << "port", "Web UI port (default: 7979). See --bind.", "port", "7979");

    QCommandLineOption gainOpt("gain", "Manual RTL-SDR gain index (default: software AGC).", "index");
    QCommandLineOption hardwareAgcOpt("hardware-agc", "Use the RTL-SDR's own hardware AGC instead of software AGC (RTL-SDR only).");

    QCommandLineOption listChannelsOpt("list-channels", "Print the DAB channel table and exit.");
    QCommandLineOption commandlineOnlyOpt("commandline-only", "Disable the interactive terminal dashboard (TUI) and log to commandline instead, "
                                                        "independent of verbosity.");
    QCommandLineOption verboseOpt("verbose",
                                   "Increase log verbosity and log to commandline instead of the TUI; repeatable, e.g. by repeating "
                                   "-v (-vv, -vvv) or this option. Twice enables debug logging, three times also enables Qt's "
                                   "own debug logging.");

    parser.addOption(deviceOpt);
    parser.addOption(serialOpt);
    parser.addOption(rtlTcpHostOpt);
    parser.addOption(rtlTcpPortOpt);
    parser.addOption(channelOpt);
    parser.addOption(freqOpt);
    parser.addOption(sidOpt);
    parser.addOption(scidsOpt);
    parser.addOption(serviceOpt);
    parser.addOption(bindOpt);
    parser.addOption(portOpt);
    parser.addOption(gainOpt);
    parser.addOption(hardwareAgcOpt);
    parser.addOption(listChannelsOpt);
    parser.addOption(commandlineOnlyOpt);
    parser.addOption(verboseOpt);

    parser.process(app);

    if (parser.isSet(listChannelsOpt))
    {
        printChannelList();
        return 0;
    }

    int verboseLevel = 0;
    for (const QString &name : parser.optionNames())
    {
        if (name == QLatin1String("v") || name == QLatin1String("version") || name == QLatin1String("verbose"))
        {
            ++verboseLevel;
        }
    }
    verboseLevel = std::min(verboseLevel, 3);

    // a single -v/--version shows version; -vv/-vvv (or repeated --verbose) enable verbose
    // commandline logging without the TUI instead
    if (verboseLevel == 1)
    {
        printf("%s %s\n", qPrintable(QCoreApplication::applicationName()), qPrintable(QCoreApplication::applicationVersion()));
        return 0;
    }

    bool forcecommandlineOnly = (verboseLevel >= 2) || parser.isSet(commandlineOnlyOpt);

    if (verboseLevel <= 1)
    {
        QLoggingCategory::setFilterRules("*.debug=false");
    }
    else if (verboseLevel == 2)
    {
        QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");
    }
    else
    {
        QLoggingCategory::setFilterRules("*.debug=true");
    }

    CliConfig config;

    QString deviceStr = parser.value(deviceOpt).toLower();
    if ("rtltcp" == deviceStr)
    {
        config.deviceType = CliDeviceType::RTLTCP;
    }
    else if ("rtlsdr" == deviceStr)
    {
        config.deviceType = CliDeviceType::RTLSDR;
    }
    else
    {
        fprintf(stderr, "Unknown device type '%s'. Supported: rtlsdr, rtltcp.\n", qPrintable(deviceStr));
        return 1;
    }

    config.deviceArg = parser.value(serialOpt);
    config.rtlTcpHost = parser.value(rtlTcpHostOpt);
    config.rtlTcpPort = quint16(parser.value(rtlTcpPortOpt).toUInt());

    config.channel = parser.value(channelOpt);
    if (parser.isSet(freqOpt))
    {
        config.frequencyKHz = parser.value(freqOpt).toUInt();
    }

    if (parser.isSet(sidOpt))
    {
        config.haveInitialService = true;
        config.initialSid = parser.value(sidOpt).toUInt(nullptr, 0);
        config.initialScids = uint8_t(parser.value(scidsOpt).toUInt());
    }
    if (parser.isSet(serviceOpt))
    {
        config.initialServiceLabel = parser.value(serviceOpt);
    }

    config.bindAddress = QHostAddress(parser.value(bindOpt));
    config.webPort = quint16(parser.value(portOpt).toUInt());
    config.webUiEnabled = parser.isSet(bindOpt) || parser.isSet(portOpt);

    if (parser.isSet(gainOpt))
    {
        config.manualGainIndex = parser.value(gainOpt).toInt();
    }
    config.hardwareAgc = parser.isSet(hardwareAgcOpt);
    config.verboseLevel = verboseLevel;

    DabCliApp cliApp(config);
    if (!cliApp.start())
    {
        return 1;
    }

#if HAVE_FTXUI
    // shown instead of the web UI when the user didn't explicitly ask for one; falls back to
    // plain logging (below) if stdout isn't an interactive terminal (e.g. output redirected)
    std::unique_ptr<CliTui> tui;
    if (!config.webUiEnabled && !forcecommandlineOnly)
    {
        tui = std::make_unique<CliTui>(&cliApp, verboseLevel);
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
