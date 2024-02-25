/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef AUDIORECITEMDIALOG_H
#define AUDIORECITEMDIALOG_H

#include <QDialog>
#include "slmodel.h"
#include "audiorecscheduleitem.h"

namespace Ui {
class AudioRecItemDialog;
}

class AudioRecItemDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioRecItemDialog(QLocale locale, SLModel * slModel, QWidget *parent = nullptr);
    ~AudioRecItemDialog();

    const AudioRecScheduleItem & itemData() const;
    void setItemData(const AudioRecScheduleItem &newItemData);

private:
    Ui::AudioRecItemDialog *ui;
    QLocale m_locale;
    AudioRecScheduleItem m_itemData;
    SLModel * m_slModel;

    void alignUiState();
    void onStartDateChanged();
    void onStartTimeChanged(const QTime & time);
    void onDurationChanged(const QDateTime &duration);
    void onServiceSelection(const QItemSelection &selection);
    void updateEndTime();
};

#endif // AUDIORECITEMDIALOG_H
