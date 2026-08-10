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

#include "clitui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define AB_ISATTY _isatty
#define AB_FILENO _fileno
#else
#include <unistd.h>
#define AB_ISATTY isatty
#define AB_FILENO fileno
#endif

#include <QCoreApplication>
#include <QJsonArray>
#include <QMutexLocker>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include "dabcliapp.h"

namespace
{
constexpr int kMaxLogLines = 300;
constexpr int kLogPanelLines = 6;
constexpr int kChannelsWidth = 18;   // terminal columns reserved for the channel list
constexpr int kMinServiceWidth = 24;  // minimum terminal columns left for the service list
constexpr int kMinSlideCols = 16;
constexpr int kMinSlideRows = 8;
constexpr int kVolumeStep = 5;
constexpr int kSlideColorLevels = 256;  // per-channel steps for the slideshow's posterized fg/bg colors

int quantizeChannel(int v)
{
    constexpr int kStep = 255 / (kSlideColorLevels - 1);
    return std::clamp(int(std::lround(v / double(kStep))) * kStep, 0, 255);
}
}  // namespace

using namespace ftxui;

CliTui::CliTui(DabCliApp *app, int verboseLevel, QObject *parent) : QObject(parent), m_app(app), m_verboseLevel(verboseLevel)
{
    connect(&m_timer, &QTimer::timeout, this, &CliTui::pump);
}

CliTui::~CliTui() = default;

bool CliTui::start()
{
    if (!AB_ISATTY(AB_FILENO(stdout)))
    {
        return false;
    }

    const QJsonArray channels = DabCliApp::channelsJson();
    for (const QJsonValue &v : channels)
    {
        const QJsonObject o = v.toObject();
        const QString label = o.value("channel").toString();
        const uint32_t freq = uint32_t(o.value("frequencyKHz").toDouble());
        m_channelEntries.push_back(QString("%1  %2 MHz").arg(label, -4).arg(freq / 1000.0, 0, 'f', 3).toStdString());
        m_channelFreqKHz.push_back(freq);
    }

    refreshStatus();

    m_screen.reset(new ScreenInteractive(ScreenInteractive::Fullscreen()));

    MenuOption channelOption = MenuOption::Vertical();
    channelOption.on_enter = [this] { tuneSelectedChannel(); };
    Component channelMenu = Menu(&m_channelEntries, &m_selectedChannel, channelOption);

    MenuOption serviceOption = MenuOption::Vertical();
    serviceOption.on_enter = [this] { playSelectedService(); };
    Component serviceMenu = Menu(&m_serviceEntries, &m_selectedService, serviceOption);

    Component container = Container::Horizontal({channelMenu, serviceMenu});

    Component rootRenderer = Renderer(container,
                                       [this, channelMenu, serviceMenu]() -> Element
                                       {
                                           const QJsonObject ensemble = m_status.value("ensemble").toObject();
                                           const QJsonObject current = m_status.value("current").toObject();
                                           const int sync = m_status.value("syncLevel").toInt();
                                           const double snr = m_status.value("snr").toDouble();
                                           const QString channel = m_status.value("channel").toString();
                                           const double freqKHz = m_status.value("frequencyKHz").toDouble();

                                           const std::string syncStr = sync >= 2 ? "SYNC" : (sync == 0 ? "NO SIGNAL" : "SEARCHING");
                                           const Color syncColor = sync >= 2 ? Color::Green : (sync == 0 ? Color::Red : Color::Yellow);

                                           const QString nowPlaying = current.value("playing").toBool()
                                                                           ? current.value("label").toString()
                                                                           : QStringLiteral("(not playing)");
                                           const QString title = m_app->currentStreamTitle();
                                           const int volumePercent = m_status.value("volumePercent").toInt(100);

                                           Elements dlPlusLines;
                                           {
                                               const QJsonArray tags = current.value("dlPlusTags").toArray();
                                               int shown = 0;
                                               for (const QJsonValue &v : tags)
                                               {
                                                   if (shown >= 3)
                                                   {
                                                       break;
                                                   }
                                                   const QJsonObject tagObj = v.toObject();
                                                   const QString tagText = tagObj.value("text").toString();
                                                   if (tagText.isEmpty())
                                                   {
                                                       continue;
                                                   }
                                                   dlPlusLines.push_back(hbox(
                                                       {text(QString("%1: ").arg(tagObj.value("label").toString()).toStdString()) | dim,
                                                        text(tagText.toStdString())}));
                                                   ++shown;
                                               }
                                           }

                                           Element header =
                                               vbox({
                                                   hbox({filler(), text(QCoreApplication::applicationName().toStdString()) | bold | color(Color::Cyan),
                                                         text(" "), text(QString("%1").arg(QCoreApplication::applicationVersion()).toStdString()) | dim,
                                                         filler()}),
                                                   hbox({text("Ensemble: "), text(ensemble.value("label").toString().toStdString()) | bold,
                                                         filler(),
                                                         text(QString("Channel: %1%2")
                                                                  .arg(channel.isEmpty() ? QStringLiteral("-") : channel)
                                                                  .arg(freqKHz > 0 ? QString(" (%1 kHz)").arg(freqKHz) : QString())
                                                                  .toStdString())}),
                                                   hbox({text("Signal: "), text(syncStr) | color(syncColor),
                                                         text(QString("   SNR: %1 dB").arg(snr, 0, 'f', 1).toStdString()), filler(),
                                                         text(QString("Volume: %1%2")
                                                                  .arg(volumePercent == 0 ? QStringLiteral("muted") : QString::number(volumePercent))
                                                                  .arg(volumePercent == 0 ? QString() : QStringLiteral("%"))
                                                                  .toStdString())}),
                                                   hbox({text("Now playing: "), text(nowPlaying.toStdString()) | bold}),
                                                   hbox({text(title.isEmpty() ? std::string(" ") : title.toStdString()) | dim}),
                                                   vbox(dlPlusLines),
                                               }) |
                                               border;

                                           Element chanBox =
                                               window(text(" Channels "), channelMenu->Render() | frame) |
                                               size(WIDTH, EQUAL, kChannelsWidth);

                                           // size the service list to the longest entry currently shown, so labels and
                                           // bitrates are never clipped regardless of the fixed kMinServiceWidth guess
                                           int maxServiceLen = 0;
                                           for (const std::string &s : m_serviceEntries)
                                           {
                                               maxServiceLen = std::max(maxServiceLen, int(s.size()));
                                           }
                                           const int serviceWidth = std::max(kMinServiceWidth, maxServiceLen + 2 /* borders */);
                                           Element svcBox = window(text(" Services "), serviceMenu->Render() | frame) |
                                                             size(WIDTH, EQUAL, serviceWidth);

                                           // size the slide panel to whatever terminal space is left, so the image is
                                           // as sharp as the terminal allows instead of a small fixed low-resolution box
                                           const Dimensions termSize = Terminal::Size();
                                           const int reservedWidth = kChannelsWidth + serviceWidth + 2;
                                           const int slideCols = std::max(kMinSlideCols, termSize.dimx - reservedWidth);
                                           const int reservedHeight = 7 + int(dlPlusLines.size()) + (kLogPanelLines + 2) + 2 + 2;
                                           const int slideRows = std::max(kMinSlideRows, termSize.dimy - reservedHeight);

                                           const QImage &slideImg = scaledSlide(slideCols * 2, slideRows * 4);

                                           Element slideContent;
                                           if (slideImg.isNull())
                                           {
                                               slideContent = vbox({filler(), text("(no image)") | center, filler()}) | flex;
                                           }
                                           else
                                           {
                                               // quadrant glyphs indexed by mask bit0=TL, bit1=TR, bit2=BL, bit3=BR
                                               static const char32_t kQuadrantGlyph[16] = {U' ', U'▘', U'▝', U'▀', U'▖', U'▌', U'▞', U'▛',
                                                                                            U'▗', U'▚', U'▐', U'▜', U'▄', U'▙', U'▟', U'█'};

                                               const int fitW = slideImg.width();
                                               const int fitH = slideImg.height();
                                               const int imgW = fitW;
                                               const int imgH = fitH - (fitH % 2);  // even number of rows to pair up

                                               // average each vertical pair of true rows into one 2x2-per-cell row
                                               std::vector<QRgb> avg(size_t(imgW) * (imgH / 2));
                                               for (int y = 0; y < imgH / 2; ++y)
                                               {
                                                   for (int x = 0; x < imgW; ++x)
                                                   {
                                                       const QRgb p0 = slideImg.pixel(x, 2 * y);
                                                       const QRgb p1 = slideImg.pixel(x, 2 * y + 1);
                                                       const int r = (qRed(p0) + qRed(p1)) / 2;
                                                       const int g = (qGreen(p0) + qGreen(p1)) / 2;
                                                       const int b = (qBlue(p0) + qBlue(p1)) / 2;
                                                       avg[size_t(y) * imgW + x] = qRgb(r, g, b);
                                                   }
                                               }
                                               const int avgW = imgW;
                                               const int avgH = imgH / 2;

                                               static const int kBit[2][2] = {{0, 1}, {2, 3}};

                                               Elements rows;
                                               for (int y = 0; y < avgH / 2; ++y)
                                               {
                                                   Elements cols;
                                                   for (int x = 0; x < avgW / 2; ++x)
                                                   {
                                                       QRgb sub[2][2];
                                                       int luma[2][2];
                                                       int lumaSum = 0;
                                                       for (int dy = 0; dy < 2; ++dy)
                                                       {
                                                           for (int dx = 0; dx < 2; ++dx)
                                                           {
                                                               const QRgb p = avg[size_t(2 * y + dy) * avgW + (2 * x + dx)];
                                                               sub[dy][dx] = p;
                                                               luma[dy][dx] = (299 * qRed(p) + 587 * qGreen(p) + 114 * qBlue(p)) / 1000;
                                                               lumaSum += luma[dy][dx];
                                                           }
                                                       }
                                                       const int threshold = lumaSum / 4;
                                                       int mask = 0;
                                                       qint64 fgR = 0, fgG = 0, fgB = 0, fgN = 0;
                                                       qint64 bgR = 0, bgG = 0, bgB = 0, bgN = 0;
                                                       for (int dy = 0; dy < 2; ++dy)
                                                       {
                                                           for (int dx = 0; dx < 2; ++dx)
                                                           {
                                                               const QRgb p = sub[dy][dx];
                                                               if (luma[dy][dx] >= threshold)
                                                               {
                                                                   mask |= (1 << kBit[dy][dx]);
                                                                   fgR += qRed(p);
                                                                   fgG += qGreen(p);
                                                                   fgB += qBlue(p);
                                                                   ++fgN;
                                                               }
                                                               else
                                                               {
                                                                   bgR += qRed(p);
                                                                   bgG += qGreen(p);
                                                                   bgB += qBlue(p);
                                                                   ++bgN;
                                                               }
                                                           }
                                                       }
                                                       const Color fg = fgN > 0 ? Color::RGB(quantizeChannel(int(fgR / fgN)), quantizeChannel(int(fgG / fgN)),
                                                                                              quantizeChannel(int(fgB / fgN)))
                                                                                : Color::Black;
                                                       const Color bg = bgN > 0 ? Color::RGB(quantizeChannel(int(bgR / bgN)), quantizeChannel(int(bgG / bgN)),
                                                                                              quantizeChannel(int(bgB / bgN)))
                                                                                : Color::Black;
                                                       const std::string glyph = QString(QChar(kQuadrantGlyph[mask])).toUtf8().toStdString();
                                                       cols.push_back(text(glyph) | color(fg) | bgcolor(bg));
                                                   }
                                                   rows.push_back(hbox(cols));
                                               }
                                               // the fitted image may be smaller than the panel on one axis (letterboxing
                                               // to preserve aspect ratio), so center it instead of leaving it top-left
                                               slideContent = center(vbox(rows));
                                           }
                                           Element slideBox = window(text(" Slideshow "), slideContent) |
                                                               size(WIDTH, EQUAL, slideCols + 2) | size(HEIGHT, EQUAL, slideRows + 2);

                                           Element body = hbox({chanBox, svcBox, slideBox}) | flex;

                                           Elements logElems;
                                           {
                                               QMutexLocker lock(&m_logMutex);
                                               const int first = std::max(0, int(m_logLines.size()) - kLogPanelLines);
                                               for (int i = first; i < m_logLines.size(); ++i)
                                               {
                                                   logElems.push_back(text(m_logLines.at(i).toStdString()));
                                               }
                                           }
                                           Element logBox = window(text(" Log "), vbox(logElems)) | size(HEIGHT, EQUAL, kLogPanelLines + 2);

                                           Elements footerLines;
                                           if (!m_statusMessage.isEmpty())
                                           {
                                               footerLines.push_back(text(m_statusMessage.toStdString()) | color(Color::Red));
                                           }
                                           QString hint = QStringLiteral(
                                               "Left/Right: switch panel   Up/Down: navigate   Enter: tune/play   +/-: volume   m: mute   q: quit");
                                           if (m_verboseLevel > 0)
                                           {
                                               hint += QString("   (verbosity v%1)").arg(m_verboseLevel);
                                           }
                                           footerLines.push_back(text(hint.toStdString()) | dim);

                                           return vbox({header, body, logBox, vbox(footerLines)});
                                       });

    Component withEvents = CatchEvent(rootRenderer,
                                       [this](Event event) -> bool
                                       {
                                           if (event == Event::Character('q') || event.input() == std::string(1, 3))
                                           {
                                               m_screen->Exit();
                                               return true;
                                           }
                                           if (event == Event::Character('+') || event == Event::Character('='))
                                           {
                                               m_app->setVolume(m_app->volumePercent() + kVolumeStep);
                                               return true;
                                           }
                                           if (event == Event::Character('-') || event == Event::Character('_'))
                                           {
                                               m_app->setVolume(m_app->volumePercent() - kVolumeStep);
                                               return true;
                                           }
                                           if (event == Event::Character('m'))
                                           {
                                               m_app->toggleMute();
                                               return true;
                                           }
                                           return false;
                                       });

    m_loop = std::make_unique<Loop>(m_screen.get(), withEvents);

    m_timer.start(50);
    return true;
}

void CliTui::pump()
{
    if (!m_loop)
    {
        return;
    }
    refreshStatus();
    // RunOnce() only redraws when it has a task to process (e.g. real keyboard input); without
    // this, external state changes (tuning, log lines, slide updates) wouldn't show up until the
    // next keypress.
    m_screen->RequestAnimationFrame();
    m_loop->RunOnce();
    if (m_loop->HasQuitted())
    {
        m_timer.stop();
        emit quitRequested();
    }
}

void CliTui::refreshStatus()
{
    m_status = m_app->statusJson();

    const QJsonArray services = m_status.value("services").toArray();
    m_serviceEntries.clear();
    m_serviceSid.clear();
    m_serviceScids.clear();
    for (const QJsonValue &v : services)
    {
        const QJsonObject o = v.toObject();
        const QString label = o.value("label").toString();
        const int bitRate = o.value("bitRate").toInt();
        m_serviceEntries.push_back(QString("%1  (%2 kbps)").arg(label, -24).arg(bitRate).toStdString());
        m_serviceSid.push_back(uint32_t(o.value("sid").toString().toUInt(nullptr, 0)));
        m_serviceScids.push_back(uint8_t(o.value("scids").toInt()));
    }

    if (m_selectedService >= int(m_serviceEntries.size()))
    {
        m_selectedService = m_serviceEntries.empty() ? 0 : int(m_serviceEntries.size()) - 1;
    }
    if (m_selectedChannel >= int(m_channelEntries.size()))
    {
        m_selectedChannel = m_channelEntries.empty() ? 0 : int(m_channelEntries.size()) - 1;
    }

    const int slideVersion = m_status.value("current").toObject().value("slideVersion").toInt();
    if (slideVersion != m_lastSlideVersion)
    {
        m_lastSlideVersion = slideVersion;
        m_slideImageFull = QImage();
        m_slideScaledCols = -1;  // force scaledSlide() to recompute even if the terminal size didn't change

        QByteArray data;
        QString contentType;
        if (slideVersion > 0 && m_app->currentSlide(&data, &contentType))
        {
            QImage img;
            if (img.loadFromData(data))
            {
                m_slideImageFull = img.convertToFormat(QImage::Format_RGB32);
            }
        }
    }
}

const QImage &CliTui::scaledSlide(int cols, int rows)
{
    if (cols != m_slideScaledCols || rows != m_slideScaledRows)
    {
        m_slideScaledCols = cols;
        m_slideScaledRows = rows;
        m_slideImageScaled = m_slideImageFull.isNull()
                                 ? QImage()
                                 : m_slideImageFull.scaled(cols, rows, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return m_slideImageScaled;
}

void CliTui::tuneSelectedChannel()
{
    if (m_selectedChannel < 0 || m_selectedChannel >= int(m_channelFreqKHz.size()))
    {
        return;
    }
    QString err;
    if (!m_app->requestTuneFrequency(m_channelFreqKHz.at(m_selectedChannel), &err))
    {
        m_statusMessage = err;
    }
    else
    {
        m_statusMessage.clear();
    }
}

void CliTui::playSelectedService()
{
    if (m_selectedService < 0 || m_selectedService >= int(m_serviceSid.size()))
    {
        return;
    }
    QString err;
    if (!m_app->requestService(m_serviceSid.at(m_selectedService), m_serviceScids.at(m_selectedService), &err))
    {
        m_statusMessage = err;
    }
    else
    {
        m_statusMessage.clear();
    }
}

void CliTui::appendLogLine(const QString &line)
{
    QMutexLocker lock(&m_logMutex);
    m_logLines.append(line);
    while (m_logLines.size() > kMaxLogLines)
    {
        m_logLines.removeFirst();
    }
}
