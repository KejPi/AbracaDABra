#ifndef SERVICELIST_H
#define SERVICELIST_H

//#include <QAbstractItemModel>
//#include <QModelIndex>

#include <QObject>
#include <QString>
#include <QList>
#include <QMutex>
#include <stdint.h>
#include <QHash>
#include <QSettings>

#include "radiocontrol.h"
#include "servicelistid.h"
#include "servicelistitem.h"
#include "ensemblelistitem.h"

typedef QHash<ServiceListId, ServiceListItem *>::Iterator ServiceListIterator;
typedef QHash<ServiceListId, EnsembleListItem *>::Iterator EnsembleListIterator;
typedef QHash<ServiceListId, ServiceListItem *>::ConstIterator ServiceListConstIterator;
typedef QHash<ServiceListId, EnsembleListItem *>::ConstIterator EnsembleListConstIterator;

class ServiceList : public QObject
{
    Q_OBJECT

public:
    ServiceList(QObject * parent = 0);
    ~ServiceList();

    void addService(const RadioControlEnsemble &e, const RadioControlServiceComponent &s, bool fav = false, int currentEns = 0);
    int numServices() const { return m_serviceList.size(); }
    int numEnsembles(const ServiceListId &servId = 0) const;
    int currentEnsembleIdx(const ServiceListId &servId) const;
    void clear();

    void setServiceFavorite(const ServiceListId &servId, bool ena);
    bool isServiceFavorite(const ServiceListId &servId) const;
    ServiceListConstIterator serviceListBegin() const { return m_serviceList.cbegin();}
    ServiceListConstIterator serviceListEnd() const { return m_serviceList.cend();}
    ServiceListConstIterator findService(const ServiceListId & id) const { return m_serviceList.find(id); }
    EnsembleListConstIterator ensembleListBegin() const { return m_ensembleList.cbegin();}
    EnsembleListConstIterator ensembleListEnd() const { return m_ensembleList.cend();}
    EnsembleListConstIterator findEnsemble(const ServiceListId & id) const { return m_ensembleList.find(id); }
    void save(QSettings & settings);
    void load(QSettings & settings);

    void beginEnsembleUpdate(const RadioControlEnsemble &e);
    void endEnsembleUpdate(const RadioControlEnsemble &e);
signals:
    void serviceAddedToEnsemble(const ServiceListId & ensId, const ServiceListId & servId);
    void serviceAdded(const ServiceListId & servId);

    void serviceRemovedFromEnsemble(const ServiceListId & ensId, const ServiceListId & servId);
    void serviceRemoved(const ServiceListId & servId);
    void empty();

private:
    QHash<ServiceListId, ServiceListItem *> m_serviceList;
    QHash<ServiceListId, EnsembleListItem *> m_ensembleList;
};

#endif // SERVICELIST_H
