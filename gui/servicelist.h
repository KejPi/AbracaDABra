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
#include "servicelistitem.h"
#include "ensemblelistitem.h"

typedef QHash<uint64_t, ServiceListItem *>::Iterator ServiceListIterator;
typedef QHash<uint64_t, EnsembleListItem *>::Iterator EnsembleListIterator;
typedef QHash<uint64_t, ServiceListItem *>::ConstIterator ServiceListConstIterator;
typedef QHash<uint64_t, EnsembleListItem *>::ConstIterator EnsembleListConstIterator;

class ServiceList : public QObject
{
    Q_OBJECT

public:
    ServiceList(QObject * parent = 0);
    ~ServiceList();

    void addService(const RadioControlEnsemble &e, const RadioControlServiceComponent &s, bool fav = false, int currentEns = 0);
    int numServices() const { return m_serviceList.size(); }
    int numEnsembles(uint64_t servId = 0) const;
    int currentEnsembleIdx(uint64_t servId) const;
    void clear();

    void setServiceFavorite(uint64_t servId, bool ena);
    bool isServiceFavorite(uint64_t servId) const;
    ServiceListConstIterator serviceListBegin() const { return m_serviceList.cbegin();}
    ServiceListConstIterator serviceListEnd() const { return m_serviceList.cend();}
    ServiceListConstIterator findService(uint64_t id) const { return m_serviceList.find(id); }
    EnsembleListConstIterator ensembleListBegin() const { return m_ensembleList.cbegin();}
    EnsembleListConstIterator ensembleListEnd() const { return m_ensembleList.cend();}
    EnsembleListConstIterator findEnsemble(uint64_t id) const { return m_ensembleList.find(id); }
    void save(QSettings & settings);
    void load(QSettings & settings);   
signals:
    void serviceAddedToEnsemble(const EnsembleListItem *, const ServiceListItem *);
    void serviceAdded(const ServiceListItem *);
    void empty();

private:
    QHash<uint64_t, ServiceListItem *> m_serviceList;
    QHash<uint64_t, EnsembleListItem *> m_ensembleList;
};

#endif // SERVICELIST_H
