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

#ifndef AUDIORECSCHEDULEDIALOG_H
#define AUDIORECSCHEDULEDIALOG_H

#include <QDialog>
#include <QItemSelectionModel>

#include "audiorecschedulemodel.h"
#include "slmodel.h"

namespace Ui
{
class AudioRecScheduleDialog;
}

class AudioRecScheduleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioRecScheduleDialog(AudioRecScheduleModel *model, SLModel *slModel, QWidget *parent = nullptr);
    ~AudioRecScheduleDialog();

    void addItem();
    void addItem(const AudioRecScheduleItem &item);
    void editItem();
    void removeItem();
    void deleteAll();

    void setLocale(const QLocale &newLocale);
    void setServiceListModel(SLModel *newSlModel);

signals:
    void selectionChanged(const QItemSelection &selected);

private:
    Ui::AudioRecScheduleDialog *ui;

    QLocale m_locale;
    AudioRecScheduleModel *m_model;
    SLModel *m_slModel;
    void updateActions(const QItemSelection &selection);
    void resizeTableColumns();
};

#endif  // AUDIORECSCHEDULEDIALOG_H
