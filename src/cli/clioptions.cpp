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

#include "clioptions.h"

#include <algorithm>

#include <QCommandLineParser>
#include <QLoggingCategory>

#include "dabtables.h"

namespace
{
void printChannelList()
{
    for (auto it = DabTables::channelList.cbegin(); it != DabTables::channelList.cend(); ++it)
    {
        printf("%-4s %8.3f MHz\n", qPrintable(it.value()), it.key() / 1000.0);
    }
}
}  // namespace

int parseCliArguments(QCoreApplication &app, CliConfig *config)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Headless DAB/DAB+ receiver mode (web UI / interactive terminal dashboard / commandline-only), "
        "built on the same AbracaDABra DAB engine as the GUI.");
    parser.addHelpOption();

    QCommandLineOption cliOpt("cli", "Run in headless CLI mode instead of the GUI. Required for all other options below to apply.");
    parser.addOption(cliOpt);

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

    QCommandLineOption bindOpt("bind",
                                "Web UI bind address (default: 0.0.0.0). If neither --bind nor --port is given, an "
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

    bool forceCommandlineOnly = (verboseLevel >= 2) || parser.isSet(commandlineOnlyOpt);

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

    QString deviceStr = parser.value(deviceOpt).toLower();
    if ("rtltcp" == deviceStr)
    {
        config->deviceType = CliDeviceType::RTLTCP;
    }
    else if ("rtlsdr" == deviceStr)
    {
        config->deviceType = CliDeviceType::RTLSDR;
    }
    else
    {
        fprintf(stderr, "Unknown device type '%s'. Supported: rtlsdr, rtltcp.\n", qPrintable(deviceStr));
        return 1;
    }

    config->deviceArg = parser.value(serialOpt);
    config->rtlTcpHost = parser.value(rtlTcpHostOpt);
    config->rtlTcpPort = quint16(parser.value(rtlTcpPortOpt).toUInt());

    config->channel = parser.value(channelOpt);
    if (parser.isSet(freqOpt))
    {
        config->frequencyKHz = parser.value(freqOpt).toUInt();
    }

    if (parser.isSet(sidOpt))
    {
        config->haveInitialService = true;
        config->initialSid = parser.value(sidOpt).toUInt(nullptr, 0);
        config->initialScids = uint8_t(parser.value(scidsOpt).toUInt());
    }
    if (parser.isSet(serviceOpt))
    {
        config->initialServiceLabel = parser.value(serviceOpt);
    }

    config->bindAddress = QHostAddress(parser.value(bindOpt));
    config->webPort = quint16(parser.value(portOpt).toUInt());
    config->webUiEnabled = parser.isSet(bindOpt) || parser.isSet(portOpt);

    if (parser.isSet(gainOpt))
    {
        config->manualGainIndex = parser.value(gainOpt).toInt();
    }
    config->hardwareAgc = parser.isSet(hardwareAgcOpt);
    config->verboseLevel = verboseLevel;
    config->commandlineOnly = forceCommandlineOnly;

    return -1;
}
