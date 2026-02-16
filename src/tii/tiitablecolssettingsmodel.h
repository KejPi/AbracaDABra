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

#ifndef TIITABLECOLSSETTINGSMODEL_H
#define TIITABLECOLSSETTINGSMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QQmlEngine>

struct TiiColItem
{
    QString name;
    int colIdx = 0;
    bool editable = true;
    bool enabled = true;
};

class Settings;
class TiiTableColsSettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        EditableRole,
        EnabledRole,
    };

    explicit TiiTableColsSettingsModel(QObject *parent = nullptr);

    void init(Settings *settings);  // call after settings are loaded to sync with current state
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void move(int from, int to);

    // C++ API for querying current state
    QList<TiiColItem> items() const;

signals:
    void colSettingsChanged();                     // emitted when an item is changed (enabled/disabled) or order is changed
    void orderChanged();                           // emitted when item order changes
    void enabledChanged(int index, bool enabled);  // emitted when an item is toggled

private:
    Settings *m_settings = nullptr;
    std::vector<TiiColItem> m_items;
};

#endif  // TIITABLECOLSSETTINGSMODEL_H
