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

    void addService(const RadioControlServiceListEntry & s, bool fav = false, int currentEns = 0);
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
    ServiceListItem(const RadioControlServiceListEntry & item, bool fav = false, int currentEns = 0);

    void addEnsemble(EnsembleListItem * ensPtr);
    void setFavorite(bool ena) { m_favorite = ena; }

    DabSId SId() const { return m_sid; }
    uint8_t SCIdS() const { return m_scids; }
    QString label() const { return m_label; }
    QString shortLabel() const { return m_shortLabel; }
    int numEnsembles() const { return m_ensembleList.size(); }   
    bool isFavorite() const { return m_favorite; }
    const EnsembleListItem * getEnsemble(int num = -1) const;         // returns ensemble idx (wraps over end of list)
    const EnsembleListItem * switchEnsemble();                        // switches current ensemble, returns ensemble
    uint32_t currentEnsembleIdx() const { return m_currentEnsemble; } // used to save service list

    bool operator==(const ServiceListItem & other);
    uint64_t getId() const { return getId(m_sid.value, m_scids); }
    static uint64_t getId(const RadioControlServiceListEntry & item) { return getId(item.SId.value, item.SCIdS); }
#warning "Consider replacing RadioControlAudioService by RadioControlServiceListEntry"
    static uint64_t getId(const RadioControlAudioService & item) { return getId(item.SId.value, item.SCIdS); }
    static uint64_t getId(uint32_t sid, uint8_t scids) { return ((uint64_t(scids)<<32) | sid); }
private:    
    // Service
    DabSId m_sid;           // SId (contains ECC)
    uint8_t m_scids;        // service component ID within the service
    QString m_label;        // Service label
    QString m_shortLabel;   // Short label
    bool m_favorite;        // Favorite service
    uint16_t m_bitRate;     // Service bitrate
    int m_currentEnsemble;

    QList<EnsembleListItem *> m_ensembleList;

    ServiceListItem() = delete;           // disabled
    QList<EnsembleListItem *>::iterator findEnsemble(uint64_t id);
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

    uint64_t getId() const { return getId(m_ueid, m_frequency); }
    static uint64_t getId(const RadioControlServiceListEntry & item) { return getId(item.ensemble.ueid, item.ensemble.frequency); }
    static uint64_t getId(uint32_t ueid, uint32_t freq) { return ((uint64_t(freq)<<32) | ueid); }

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

};


#endif // SERVICELIST_H
