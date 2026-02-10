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

#include "soapysdrgainmodel.h"

SoapySdrGainModel::SoapySdrGainModel(QObject *parent) : QAbstractListModel{parent}
{
    connect(this, &QAbstractListModel::rowsInserted, this, &SoapySdrGainModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &SoapySdrGainModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &SoapySdrGainModel::rowCountChanged);
}

QVariant SoapySdrGainModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_modelData.size())
    {
        return QVariant();
    }

    const GainItem &item = m_modelData.at(index.row());

    switch (role)
    {
        case NameRole:
            return item.name;
        case GainValueRole:
            return item.gainValue;
        case MinimumRole:
            return item.minimum;
        case MaximumRole:
            return item.maximum;
        case StepRole:
            return item.step;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> SoapySdrGainModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[GainValueRole] = "gainValue";
    roles[MinimumRole] = "minimum";
    roles[MaximumRole] = "maximum";
    roles[StepRole] = "step";
    return roles;
}

void SoapySdrGainModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void SoapySdrGainModel::addItem(const QString &name, int value, int minimum, int maximum, int step)
{
    beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
    m_modelData.append({name, value, minimum, maximum, step});
    endInsertRows();
}

bool SoapySdrGainModel::setGainValue(int row, int value)
{
    if (row < 0 || row >= m_modelData.size())
    {
        return false;
    }
    m_modelData[row].gainValue = value;
    emit dataChanged(index(row, 0), index(row, 0), {GainValueRole});
    return true;
}
