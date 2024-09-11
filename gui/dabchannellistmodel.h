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

#ifndef DABCHANNELLISTMODEL_H
#define DABCHANNELLISTMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QSortFilterProxyModel>
#include "dabtables.h"

class DABChannelListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        FrequencyRole = Qt::UserRole,
    };

    explicit DABChannelListModel(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const { return DabTables::channelList.count(); }
};

class DABChannelListFilteredModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    DABChannelListFilteredModel(QObject *parent = nullptr);
    void setChannelFilter(int freq);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    int m_frequencyFilter;
};

#endif // DABCHANNELLISTMODEL_H
