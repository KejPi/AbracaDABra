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

#include "tiitablemodel.h"

TiiTableModel::TiiTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{}

int TiiTableModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.count();
}

int TiiTableModel::columnCount(const QModelIndex &parent) const
{
    return NumCols;
}

QVariant TiiTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.row() >= m_modelData.size() || index.row() < 0)
    {
        return QVariant();
    }

    const auto &item = m_modelData.at(index.row());
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case ColMainId:
            return m_modelData.at(index.row()).main;
        case ColSubId:
            return m_modelData.at(index.row()).sub;
        case ColLevel:
            return QString::number(static_cast<double>(m_modelData.at(index.row()).level), 'f', 4);
        }
    }
    return QVariant();
}

QVariant TiiTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColMainId:
            return tr("Main ID");
        case ColSubId:
            return tr("Sub ID");
        case ColLevel:
            return tr("Level");
        default:
            break;
        }
    }
    return QVariant();
}

void TiiTableModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void TiiTableModel::populateModel(const QList<dabsdrTii_t> &data)
{
    beginResetModel();
    m_modelData.clear();
    m_modelData = data;
    endResetModel();
}
