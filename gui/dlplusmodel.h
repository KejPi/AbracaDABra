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

#ifndef DLPLUSTMODEL_H
#define DLPLUSTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QSortFilterProxyModel>

#include "dldecoder.h"

class DLPlusModelItem;
class DLPlusModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)
public:
    enum Roles
    {
        TagRole = Qt::UserRole,
        ContentRole,
        ToolTipRole,
        IsVisibleRole
    };

    explicit DLPlusModel(QObject *parent = nullptr);
    ~DLPlusModel();
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_modelData.count(); }

    // this method inserts, removes or updates model
    void setDlPlusObject(const DLPlusObject &object);

    void itemsRunning(bool isRunning);
    void itemsToggle();
    void reset();

signals:
    void rowCountChanged();

private:
    // this list needs to be sorted
    QList<DLPlusModelItem *> m_modelData;
};

class DLPlusModelItem
{
public:
    DLPlusModelItem(const DLPlusObject &obj);
    void update(const DLPlusObject &obj);

    void setIsVisible(bool visible);
    bool isVisible() const;
    bool isValid() const { return m_tagLabel.isEmpty() == false; }

    const DLPlusObject &dlPlusObject() const;
    const QString &tagLabel() const;
    const QString &tagText() const;
    const QString &tagTooltip() const;

private:
    DLPlusObject m_dlPlusObject;
    bool m_isVisible = true;
    QString m_tagLabel;
    QString m_tagText;
    QString m_tagToolTip;

    QString typeToLabel(DLPlusContentType type) const;
};

#endif  // DLPLUSTMODEL_H
