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

class ServiceListItem;
class EnsembleListItem;

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

    void addService(const RadioControlServiceListEntry & s);
    int numServices() const { return m_serviceList.size(); }
    int numEnsembles() const { return m_ensembleList.size(); }
    void clear();

    ServiceListConstIterator serviceListBegin() const { return m_serviceList.cbegin();}
    ServiceListConstIterator serviceListEnd() const { return m_serviceList.cend();}
    ServiceListConstIterator findService(uint64_t id) const { return m_serviceList.find(id); }
    EnsembleListConstIterator ensembleListBegin() const { return m_ensembleList.cbegin();}
    EnsembleListConstIterator ensembleListEnd() const { return m_ensembleList.cend();}
    EnsembleListConstIterator findEnsemble(uint64_t id) const { return m_ensembleList.find(id); }
    void save(QSettings & settings);
    void load(QSettings & settings);   
signals:
    void serviceAdded(const ServiceListItem *);
    void ensembleAdded(const EnsembleListItem *);
    void empty();

private:
    QHash<uint64_t, ServiceListItem *> m_serviceList;
    QHash<uint64_t, EnsembleListItem *> m_ensembleList;
};

class ServiceListItem
{
public:
    ServiceListItem(const RadioControlServiceListEntry & item);

    void addEnsemble(EnsembleListItem * ensPtr);

    DabSId SId() const { return m_sid; }
    uint8_t SCIdS() const { return m_scids; }
    QString label() const { return m_label; }
    QString shortLabel() const { return m_shortLabel; }
    int numEnsembles() const { return m_ensembleList.size(); }
    const EnsembleListItem * getEnsemble(int num = -1) const;

    bool operator==(const ServiceListItem & other);
    uint64_t getId() const { return calcId(m_sid.value, m_scids); }
    static uint64_t getId(const RadioControlServiceListEntry & item) { return calcId(item.SId.value, item.SCIdS); }
private:    
    // Service
    DabSId m_sid;           // SId (contains ECC)
    uint8_t m_scids;        // service component ID within the service
    QString m_label;        // Service label
    QString m_shortLabel;   // Short label
    uint16_t m_bitRate;     // Service bitrate

    QList<EnsembleListItem *> m_ensembleList;

    ServiceListItem() = delete;           // disabled
    QList<EnsembleListItem *>::iterator findEnsemble(uint64_t id);

    static uint64_t calcId(uint32_t sid, uint8_t scids) { return ((uint64_t(scids)<<32) | sid); }
};


class EnsembleListItem
{
public:
    EnsembleListItem(const RadioControlServiceListEntry & item);

    void addService(ServiceListItem *servPtr);
    void storeSnr(float snr) { m_lastSnr = snr; }

    uint32_t frequency() const { return m_frequency; }   // frequency of ensemble
    uint32_t ueid() const { return m_ueid; }             // UEID of ensemble
    QString label() const { return m_label; }            // ensemble label
    QString shortLabel() const { return m_shortLabel; }  // ensemble short label
    float snr() const { return m_lastSnr; }              // last SNR
    int numServices() const { return m_serviceList.size(); }
    const ServiceListItem * getService(int num = 0) const;

    uint64_t getId() const { return calcId(m_ueid, m_frequency); }
    static uint64_t getId(const RadioControlServiceListEntry & item) { return calcId(item.ensemble.ueid, item.ensemble.frequency); }

    bool operator==(const EnsembleListItem & other);
private:
    uint32_t m_frequency;   // frequency of ensemble
    uint32_t m_ueid;        // UEID of ensemble
    QString m_label;        // ensemble label
    QString m_shortLabel;   // Short label
    float m_lastSnr = 0;

    QList<ServiceListItem *> m_serviceList;

    EnsembleListItem() = delete;           // disabled
    QList<ServiceListItem *>::iterator findService(uint64_t id);

    static uint64_t calcId(uint32_t ueid, uint32_t freq) { return ((uint64_t(freq)<<32) | ueid); }
};


#endif // SERVICELIST_H