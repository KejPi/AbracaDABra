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

#include "logmodel.h"

#include <QColor>

int LogModel::rowCount(const QModelIndex &parent) const
{
    return m_msgList.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (m_msgList.size() > index.row())
    {
        switch (role)
        {
            case Qt::ForegroundRole:
                if (m_isDarkMode)
                {
                    switch (m_msgList.at(index.row()).type)
                    {
                        case QtInfoMsg:
                            return QVariant();
                        case QtDebugMsg:
                            return QVariant(QColor(Qt::cyan));
                        case QtWarningMsg:
                            return QVariant(QColor(Qt::yellow));
                        case QtCriticalMsg:
                            return QVariant(QColor(Qt::red));
                        case QtFatalMsg:
                            return QVariant(QColor(Qt::red));
                        default:
                            return QVariant();
                    }
                }
                else
                {
                    switch (m_msgList.at(index.row()).type)
                    {
                        case QtInfoMsg:
                            return QVariant();
                        case QtDebugMsg:
                            return QVariant(QColor(Qt::blue));
                        case QtWarningMsg:
                            return QVariant(QColor(Qt::magenta));
                        case QtCriticalMsg:
                            return QVariant(QColor(Qt::red));
                        case QtFatalMsg:
                            return QVariant(QColor(Qt::red));
                        default:
                            return QVariant();
                    }
                }
                break;
            case Qt::DisplayRole:
                return m_msgList.at(index.row()).msg;
            default:
                return QVariant();
        }
    }

    return QVariant();
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[Qt::ForegroundRole] = "textColor";
    roles[Qt::DisplayRole] = "logMsg";
    return roles;
}

void LogModel::setupDarkMode(bool darkModeEna)
{
    m_isDarkMode = darkModeEna;
    if (rowCount() > 0)
    {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::ForegroundRole});
    }
}

void LogModel::appendRow(const QString &rowTxt, int role)
{
    int row = rowCount();
    auto item = LogItem{rowTxt, static_cast<QtMsgType>(role)};
    beginInsertRows(QModelIndex(), row, row);
    m_msgList.append(item);
    endInsertRows();
}

void LogModel::clear()
{
    beginResetModel();
    m_msgList.clear();
    endResetModel();
}
