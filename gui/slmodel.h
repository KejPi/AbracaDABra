/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-20243 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef SLMODEL_H
#define SLMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QList>
#include <QIcon>
#include "slmodelitem.h"

#include "servicelist.h"
#include "metadatamanager.h"

// enum SLModelRoles {
//     ShortIdRole = Qt::UserRole,
// };

enum SLModelRole{
    IdRole = Qt::UserRole,
    SmallLogoRole,
    SmallLogoIdRole,   // this role is used to trick QML for loading the logo when available
    EpgModelRole,
};


class SLModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SLModel(const ServiceList * sl, const MetadataManager * mm, QObject *parent = 0);
    ~SLModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QHash<int, QByteArray> roleNames() const override;

    ServiceListId id(const QModelIndex &index) const;
    bool isFavoriteService(const QModelIndex &index) const;

public slots:
    void addService(const ServiceListId & servId);
    void updateService(const ServiceListId & servId);
    void removeService(const ServiceListId & servId);
    void epgModelChanged(const ServiceListId & servId);
    void metadataUpdated(const ServiceListId &servId, MetadataManager::MetadataRole role);
    void clear();

private:
    const ServiceList * m_slPtr;
    const MetadataManager * m_metadataMgrPtr;
    QList<SLModelItem *> m_serviceItems;    

    QIcon m_favIcon;
    QIcon m_noIcon;
};

#endif // SLMODEL_H
