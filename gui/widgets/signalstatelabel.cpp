/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "signalstatelabel.h"

#include "radiocontrol.h"

const char *SignalStateLabel::syncLevelLabels[] = {QT_TR_NOOP("No signal"), QT_TR_NOOP("Signal found"), QT_TR_NOOP("Sync")};
const QStringList SignalStateLabel::snrLevelColors = {"white", "#ff4b4b", "#ffb527", "#5bc214"};
const QString SignalStateLabel::templateSvgFill =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18"><circle cx="9" cy="9" r="7" fill="%1"/></svg>)";
const QString SignalStateLabel::templateSvgOutline =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 18 18"><circle cx="9" cy="9" r="6" stroke="%1" stroke-width="2" fill="white"/></svg>)";

SignalStateLabel::SignalStateLabel(QWidget *parent) : QLabel{parent}
{
    setText("");
    setFixedSize(18, 18);

    // render level icons
    m_snrLevelIcons[0].loadFromData(templateSvgFill.arg(snrLevelColors.at(0)).toUtf8(), "svg");
    for (int n = 1; n < 4; ++n)
    {
        m_snrLevelIcons[n].loadFromData(templateSvgOutline.arg(snrLevelColors.at(n)).toUtf8(), "svg");
        m_snrLevelIcons[n + 3].loadFromData(templateSvgFill.arg(snrLevelColors.at(n)).toUtf8(), "svg");
    }
    reset();
}

void SignalStateLabel::reset()
{
    setSignalState(0, 0.0);
}

void SignalStateLabel::setSignalState(uint8_t sync, float snr)
{
    int syncSnrLevel = 0;
    if (sync > static_cast<int>(DabSyncLevel::NoSync))
    {
        syncSnrLevel = 3;
        if (snr < SNRThrehold::SNR_BAD)
        {
            syncSnrLevel = 1;
        }
        else if (snr < SNRThrehold::SNR_GOOD)
        {
            syncSnrLevel = 2;
        }
        if (sync == static_cast<int>(DabSyncLevel::FullSync))
        {
            syncSnrLevel += 3;
        }
    }
    if (syncSnrLevel != m_syncSnrLevel)
    {
        m_syncSnrLevel = syncSnrLevel;
        setPixmap(m_snrLevelIcons[m_syncSnrLevel]);
        setToolTip(syncLevelLabels[sync]);
    }
}
