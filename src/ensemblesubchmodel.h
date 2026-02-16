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

#ifndef ENSEMBLESUBCHMODEL_H
#define ENSEMBLESUBCHMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QSortFilterProxyModel>
#include <QtQmlIntegration>

#include "radiocontrol.h"

class EnsembleSubchModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged FINAL)
    Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged FINAL)
public:
    enum Roles
    {
        IdRole = Qt::UserRole,
        StartCuRole,
        NumCuRole,
        OccupiedCuRole,
        ColorRole,
        IsSelectedRole,
        IsAudioRole,
        IsEmptyRole,
        ProtectionRole,
        BitrateRole,
        ContentRole,
        ServicesRole,
        ServicesShortRole,
        RelativeWidthRole,
    };

    struct Subchannel
    {
        int id;       // Subchannel number
        int startCU;  // Starting position (inclusive)
        int numCU;    // Length in number of CU
        QColor color;
        bool isEmpty;  // Indicates if this is a gap segment
        bool isSelected = false;
        bool isAudio;

        DabProtection protection;
        int bitrate;
        enum
        {
            AAC,
            MP2,
            Data
        } content;
        QStringList services;
    };

    explicit EnsembleSubchModel(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_subchannels.count(); }

    // Set all segments at once (auto-colors any segments with transparent color and adds gaps)
    void init(const QList<RadioControlServiceComponent> &scList);
    void clear();
    int currentRow() const { return m_currentRow; }
    void setCurrentRow(int currentRow);
    void setCurrentSubCh(int subchId);

signals:
    void rowCountChanged();
    void currentRowChanged();

private:
    enum
    {
        TotalNumCU = 864
    };

    QList<Subchannel> m_subchannels;  // Includes both data segments and gap segments
    const QList<QColor> m_palette;

    void assignAutoColors();
    void fillGaps();  // Add gap segments where needed
    int m_currentRow = -1;
};

class EnsembleNonEmptySubchModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    EnsembleNonEmptySubchModel(QObject *parent = nullptr);
    Q_INVOKABLE void selectSubCh(int row);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

#endif  // ENSEMBLESUBCHMODEL_H
