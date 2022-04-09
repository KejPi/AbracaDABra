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

    class Id
    {
    public:
        Id(uint32_t sid, uint8_t scids) { id = (uint64_t(scids)<<32) | sid;  }
        Id(uint32_t freq, uint32_t ueid) { id = (uint64_t(freq)<<32) | ueid;  }
        bool isService() const { return 0 == (id & 0xFFFF00000000u); }
        bool isEnsemble() const { return !isService(); }
        uint32_t sid() { return static_cast<uint32_t>(id & 0xFFFFFFFFu ); }
        uint8_t scids() { return static_cast<uint8_t>((id>>32) & 0xFFu ); }
        uint32_t ueid() { return static_cast<uint32_t>(id & 0x00FFFFFFu ); }
    private:
        uint64_t id;
    };


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
    void serviceAddedToEnsemble(uint64_t servId, uint64_t ensId);
    void serviceAdded(uint64_t servId);
    void empty();

private:
    QHash<uint64_t, ServiceListItem *> m_serviceList;
    QHash<uint64_t, EnsembleListItem *> m_ensembleList;
};

#endif // SERVICELIST_H
