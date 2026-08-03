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

#ifndef CLITUI_H
#define CLITUI_H

#include <memory>
#include <vector>

#include <QImage>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace ftxui
{
class Loop;
class ScreenInteractive;
}  // namespace ftxui

class DabCliApp;

// Interactive terminal dashboard shown by AbracaDABra-cli instead of the web UI when --bind/--port
// are not given. Built on the same DabCliApp API the web UI uses (statusJson()/channelsJson() for
// state, requestTuneChannel()/requestService()/requestShutdown() for actions).
//
// FTXUI's ScreenInteractive runs its own input-reading background thread but expects its Loop to
// be pumped (RunOnce()) from a single calling thread; here that's a QTimer on the Qt main thread,
// so no extra thread and no conflict with Qt's own event loop is needed.
class CliTui : public QObject
{
    Q_OBJECT
public:
    explicit CliTui(DabCliApp *app, int verboseLevel, QObject *parent = nullptr);
    ~CliTui() override;

    // Returns false (without side effects) if stdout is not an interactive terminal; the caller
    // should fall back to plain log output in that case.
    bool start();

    // Thread-safe; appends a line to the in-TUI log panel. Used by the qInstallMessageHandler
    // installed in main.cpp while the TUI is active.
    void appendLogLine(const QString &line);

signals:
    // emitted once the user asks to quit from within the TUI (q / Ctrl+C)
    void quitRequested();

private:
    void pump();

    DabCliApp *m_app;
    int m_verboseLevel;

    std::unique_ptr<ftxui::ScreenInteractive> m_screen;
    std::unique_ptr<ftxui::Loop> m_loop;
    QTimer m_timer;

    QJsonObject m_status;

    std::vector<std::string> m_serviceEntries;
    QVector<uint32_t> m_serviceSid;
    QVector<uint8_t> m_serviceScids;
    int m_selectedService = 0;

    std::vector<std::string> m_channelEntries;
    QVector<uint32_t> m_channelFreqKHz;
    int m_selectedChannel = 0;

    QMutex m_logMutex;
    QStringList m_logLines;

    QString m_statusMessage;

    // full-resolution decode of the current MOT slide, redecoded only when slideVersion changes
    int m_lastSlideVersion = -1;
    QImage m_slideImageFull;

    // rescaled to fit the terminal, recomputed only when the source image or target size changes;
    // rendered as a grid of 2x2-subpixel quadrant-block characters (2 image pixel rows/columns
    // per terminal cell), so cols/rows passed to scaledSlide() are in *sub-pixel* units (terminal
    // cell count * 2), not terminal cells
    int m_slideScaledCols = -1;
    int m_slideScaledRows = -1;
    QImage m_slideImageScaled;

    void refreshStatus();
    void tuneSelectedChannel();
    void playSelectedService();
    const QImage &scaledSlide(int cols, int rows);
};

#endif  // CLITUI_H
