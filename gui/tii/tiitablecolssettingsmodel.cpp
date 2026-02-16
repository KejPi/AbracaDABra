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

#include "tiitablecolssettingsmodel.h"

#include "settings.h"
#include "txtablemodel.h"

TiiTableColsSettingsModel::TiiTableColsSettingsModel(QObject *parent) : QAbstractListModel(parent)
{
    m_items = {
        {tr("Code"), TxTableModel::ColCode, false, true},    // not editable
        {tr("Level"), TxTableModel::ColLevel, false, true},  // not editable
        {tr("Distance"), TxTableModel::ColDist, true, true},      {tr("Azimuth"), TxTableModel::ColAzimuth, true, true},
        {tr("Location"), TxTableModel::ColLocation, true, false}, {tr("Power"), TxTableModel::ColPower, true, false},
    };
}

void TiiTableColsSettingsModel::init(Settings *settings)
{
    m_settings = settings;

    // Sync enabled state with settings
    for (auto &item : m_items)
    {
        switch (item.colIdx)
        {
            case TxTableModel::ColLocation:
                item.enabled = m_settings->tii.txTable.location.enabled;
                break;
            case TxTableModel::ColPower:
                item.enabled = m_settings->tii.txTable.power.enabled;
                break;
            case TxTableModel::ColDist:
                item.enabled = m_settings->tii.txTable.dist.enabled;
                break;
            case TxTableModel::ColAzimuth:
                item.enabled = m_settings->tii.txTable.azimuth.enabled;
                break;
        }
    }
    // sort items according to current order in settings
    std::sort(m_items.begin(), m_items.end(),
              [this](const TiiColItem &a, const TiiColItem &b)
              {
                  int orderA = 0;
                  int orderB = 0;
                  switch (a.colIdx)
                  {
                      case TxTableModel::ColCode:
                          orderA = m_settings->tii.txTable.code.order;
                          break;
                      case TxTableModel::ColLevel:
                          orderA = m_settings->tii.txTable.level.order;
                          break;
                      case TxTableModel::ColLocation:
                          orderA = m_settings->tii.txTable.location.order;
                          break;
                      case TxTableModel::ColPower:
                          orderA = m_settings->tii.txTable.power.order;
                          break;
                      case TxTableModel::ColDist:
                          orderA = m_settings->tii.txTable.dist.order;
                          break;
                      case TxTableModel::ColAzimuth:
                          orderA = m_settings->tii.txTable.azimuth.order;
                          break;
                  }
                  switch (b.colIdx)
                  {
                      case TxTableModel::ColCode:
                          orderB = m_settings->tii.txTable.code.order;
                          break;
                      case TxTableModel::ColLevel:
                          orderB = m_settings->tii.txTable.level.order;
                          break;
                      case TxTableModel::ColLocation:
                          orderB = m_settings->tii.txTable.location.order;
                          break;
                      case TxTableModel::ColPower:
                          orderB = m_settings->tii.txTable.power.order;
                          break;
                      case TxTableModel::ColDist:
                          orderB = m_settings->tii.txTable.dist.order;
                          break;
                      case TxTableModel::ColAzimuth:
                          orderB = m_settings->tii.txTable.azimuth.order;
                          break;
                  }
                  return orderA < orderB;
              });
    emit colSettingsChanged();
}

int TiiTableColsSettingsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant TiiTableColsSettingsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
    {
        return {};
    }

    const auto &item = m_items[index.row()];
    switch (role)
    {
        case NameRole:
            return item.name;
        case EditableRole:
            return item.editable;
        case EnabledRole:
            return item.enabled;
    }
    return {};
}

bool TiiTableColsSettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
    {
        return false;
    }

    if (role == EnabledRole)
    {
        auto &item = m_items[index.row()];
        if (item.editable == false)
        {
            return false;
        }
        const bool newVal = value.toBool();
        if (item.enabled != newVal)
        {
            item.enabled = newVal;
            emit dataChanged(index, index, {EnabledRole});
            // emit enabledChanged(index.row(), newVal);

            // update settings
            switch (item.colIdx)
            {
                case TxTableModel::ColLocation:
                    m_settings->tii.txTable.location.enabled = newVal;
                    break;
                case TxTableModel::ColPower:
                    m_settings->tii.txTable.power.enabled = newVal;
                    break;
                case TxTableModel::ColDist:
                    m_settings->tii.txTable.dist.enabled = newVal;
                    break;
                case TxTableModel::ColAzimuth:
                    m_settings->tii.txTable.azimuth.enabled = newVal;
                    break;
            }
            emit colSettingsChanged();

            return true;
        }
    }
    return false;
}

Qt::ItemFlags TiiTableColsSettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QHash<int, QByteArray> TiiTableColsSettingsModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {EditableRole, "editable"},
        {EnabledRole, "enabled"},
    };
}

void TiiTableColsSettingsModel::move(int from, int to)
{
    if (from == to)
    {
        return;
    }
    if (from < 0 || from >= static_cast<int>(m_items.size()))
    {
        return;
    }
    if (to < 0 || to >= static_cast<int>(m_items.size()))
    {
        return;
    }

    // QAbstractListModel requires special handling of beginMoveRows indices
    const int destRow = (to > from) ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow))
    {
        return;
    }

    auto item = m_items[from];
    m_items.erase(m_items.begin() + from);
    m_items.insert(m_items.begin() + to, item);

    // update settings
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        switch (m_items[i].colIdx)
        {
            case TxTableModel::ColCode:
                m_settings->tii.txTable.code.order = static_cast<int>(i);
                break;
            case TxTableModel::ColLevel:
                m_settings->tii.txTable.level.order = static_cast<int>(i);
                break;
            case TxTableModel::ColLocation:
                m_settings->tii.txTable.location.order = static_cast<int>(i);
                break;
            case TxTableModel::ColPower:
                m_settings->tii.txTable.power.order = static_cast<int>(i);
                break;
            case TxTableModel::ColDist:
                m_settings->tii.txTable.dist.order = static_cast<int>(i);
                break;
            case TxTableModel::ColAzimuth:
                m_settings->tii.txTable.azimuth.order = static_cast<int>(i);
                break;
        }
    }

    endMoveRows();
    // emit orderChanged();
    emit colSettingsChanged();
}

QList<TiiColItem> TiiTableColsSettingsModel::items() const
{
    QVector<TiiColItem> result;
    result.reserve(static_cast<int>(m_items.size()));
    for (const auto &item : m_items)
    {
        result.append(item);
    }
    return result;
}
