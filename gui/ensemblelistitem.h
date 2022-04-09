#ifndef ENSEMBLELISTITEM_H
#define ENSEMBLELISTITEM_H

#include <stdint.h>
#include <QString>
#include <QList>
#include "servicelistid.h"
#include "radiocontrol.h"


class ServiceListItem;

class EnsembleListItem
{
public:
    EnsembleListItem(const RadioControlEnsemble &ens);

    bool addService(ServiceListItem *servPtr);          // returns true when new service was added
    void storeSnr(float snr) { m_lastSnr = snr; }

    ServiceListId id() const { return m_id; }

    uint32_t frequency() const { return m_frequency; }   // frequency of ensemble
    uint32_t ueid() const { return m_ueid; }             // UEID of ensemble
    QString label() const { return m_label; }            // ensemble label
    QString shortLabel() const { return m_shortLabel; }  // ensemble short label
    float snr() const { return m_lastSnr; }              // last SNR
    int numServices() const { return m_serviceList.size(); }
    const ServiceListItem * getService(int num = 0) const;

    bool operator==(const EnsembleListItem & other) const;
private:
    ServiceListId m_id;
    uint32_t m_frequency;   // frequency of ensemble
    uint32_t m_ueid;        // UEID of ensemble
    QString m_label;        // ensemble label
    QString m_shortLabel;   // Short label
    float m_lastSnr = 0;

    QList<ServiceListItem *> m_serviceList;

    EnsembleListItem() = delete;           // disabled
    QList<ServiceListItem *>::iterator findService(const ServiceListId &id);

};

#endif // ENSEMBLELISTITEM_H
