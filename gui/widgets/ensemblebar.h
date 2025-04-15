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

#ifndef ENSEMBLEBAR_H
#define ENSEMBLEBAR_H

#include <QColor>
#include <QList>
#include <QString>
#include <QToolTip>
#include <QWidget>

class EnsembleBar : public QWidget
{
    Q_OBJECT

public:
    // Theme enum for color palette selection
    enum ColorTheme
    {
        LightTheme,
        DarkTheme
    };

    explicit EnsembleBar(QWidget *parent = nullptr);

    struct Subchannel
    {
        int id;       // Subchannel number
        int startCU;  // Starting position (inclusive)
        int numCU;    // Length in number of CU
        QColor color;
        bool isEmpty;  // Indicates if this is a gap segment
        bool isSelected = false;
        bool isAudio;
    };

    // Set all segments at once (auto-colors any segments with transparent color and adds gaps)
    void setSubchannels(const QList<Subchannel> &subchannels);

    // Clear all segments
    void clearSubchannels();

signals:
    void subchannelClicked(int subchId);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;  // For handling hover events
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    enum
    {
        TotalNumCU = 864
    };

    QList<Subchannel> m_subchannels;  // Includes both data segments and gap segments

    static const QList<QColor> m_palette;

    int findSubChAtPosition(const QPoint &pos) const;
    void assignAutoColors();
    void setSelected(int id);
    void fillGaps();  // Add gap segments where needed
};

#endif  // ENSEMBLEBAR_H
