#ifndef ENSEMBLELISTITEM_H
#define ENSEMBLELISTITEM_H

#include <stdint.h>
#include <QString>
#include <QList>
#include "radiocontrol.h"

class ServiceListItem;

class EnsembleListItem
{
public:
    EnsembleListItem(const RadioControlServiceListEntry & item);

    bool addService(ServiceListItem *servPtr);          // returns true when new service was added
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

#endif // ENSEMBLELISTITEM_H
