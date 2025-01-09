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

#ifndef CHANNELSELECTIONDIALOG_H
#define CHANNELSELECTIONDIALOG_H

#include <QDialog>
#include <QList>

class QCheckBox;
class ChannelSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChannelSelectionDialog(const QMap<uint32_t, bool> &channelSelection, QWidget *parent = nullptr);
    ~ChannelSelectionDialog();
    void getChannelList(QMap<uint32_t, bool> &channelSelection) const;

private:
    QMap<uint32_t, QCheckBox *> m_chList;
    QPushButton *m_acceptButton;
    int m_checkCntr = 0;

    void checkAll(bool check);
    void onCheckBoxClicked(bool checked);
};

#endif  // CHANNELSELECTIONDIALOG_H
