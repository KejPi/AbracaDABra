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

#ifndef AUDIORECSCHEDULEMODEL_H
#define AUDIORECSCHEDULEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QObject>

#include "audiorecscheduleitem.h"
#include "slmodel.h"

class AudioRecScheduleModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AudioRecScheduleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex()) override;
    const QList<AudioRecScheduleItem> &getSchedule() const;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    void insertItem(const AudioRecScheduleItem &item);
    void replaceItemAtIndex(const QModelIndex &index, const AudioRecScheduleItem &item);
    const AudioRecScheduleItem &itemAtIndex(const QModelIndex &index) const;
    void setSlModel(SLModel *newSlModel);
    void load(const QString &filename);
    void loadFromSettings(QSettings *settings);
    void save(const QString &filename);
    void cleanup(const QDateTime &currentTime);
    void clear();
    bool isEmpty() const { return m_modelData.isEmpty(); }

private:
    enum
    {
        NumColumns = 6
    };
    enum
    {
        ColState,
        ColLabel,
        ColStartTime,
        ColEndTime,
        ColDuration,
        ColService
    };

    QList<AudioRecScheduleItem> m_modelData;
    SLModel *m_slModel;

    void sortFindConflicts();
};

#endif  // AUDIORECSCHEDULEMODEL_H
