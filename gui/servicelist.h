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

#ifndef SERVICELIST_H
#define SERVICELIST_H

// #include <QAbstractItemModel>
// #include <QModelIndex>

#include <stdint.h>

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSettings>
#include <QString>

#include "ensemblelistitem.h"
#include "radiocontrol.h"
#include "servicelistid.h"
#include "servicelistitem.h"

typedef QHash<ServiceListId, ServiceListItem *>::Iterator ServiceListIterator;
typedef QHash<ServiceListId, EnsembleListItem *>::Iterator EnsembleListIterator;
typedef QHash<ServiceListId, ServiceListItem *>::ConstIterator ServiceListConstIterator;
typedef QHash<ServiceListId, EnsembleListItem *>::ConstIterator EnsembleListConstIterator;

class ServiceList : public QObject
{
    Q_OBJECT

public:
    ServiceList(QObject *parent = 0);
    ~ServiceList();

    void addService(const RadioControlEnsemble &e, const RadioControlServiceComponent &s, bool fav = false, int currentEns = 0);
    int numServices() const { return m_serviceList.size(); }
    int numEnsembles(const ServiceListId &servId = 0) const;
    int currentEnsembleIdx(const ServiceListId &servId) const;
    void clear(bool clearFavorites = true);

    void setServiceFavorite(const ServiceListId &servId, bool ena);
    bool isServiceFavorite(const ServiceListId &servId) const;
    ServiceListConstIterator serviceListBegin() const { return m_serviceList.cbegin(); }
    ServiceListConstIterator serviceListEnd() const { return m_serviceList.cend(); }
    ServiceListConstIterator findService(const ServiceListId &id) const { return m_serviceList.find(id); }
    EnsembleListConstIterator ensembleListBegin() const { return m_ensembleList.cbegin(); }
    EnsembleListConstIterator ensembleListEnd() const { return m_ensembleList.cend(); }
    EnsembleListConstIterator findEnsemble(const ServiceListId &id) const { return m_ensembleList.find(id); }
    void save(const QString &filename);
    void load(const QString &filename);
    void loadFromSettings(QSettings *settings);

    void beginEnsembleUpdate(const RadioControlEnsemble &e);
    void endEnsembleUpdate(const RadioControlEnsemble &e);
    void removeEnsemble(const RadioControlEnsemble &e);
signals:
    void serviceAddedToEnsemble(const ServiceListId &ensId, const ServiceListId &servId);
    void serviceAdded(const ServiceListId &servId);

    void serviceUpdatedInEnsemble(const ServiceListId &ensId, const ServiceListId &servId);
    void serviceUpdated(const ServiceListId &servId);

    void serviceRemovedFromEnsemble(const ServiceListId &ensId, const ServiceListId &servId);
    void serviceRemoved(const ServiceListId &servId);

    void ensembleRemoved(const ServiceListId &ensId);
    void empty();

private:
    QHash<ServiceListId, ServiceListItem *> m_serviceList;
    QHash<ServiceListId, EnsembleListItem *> m_ensembleList;
    QSet<ServiceListId> m_favoritesList;
};

#endif  // SERVICELIST_H
