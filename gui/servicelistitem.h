#ifndef SERVICELISTITEM_H
#define SERVICELISTITEM_H

#include <stdint.h>
#include <QString>
#include <QList>
#include "radiocontrol.h"

class EnsembleListItem;

class ServiceListItem
{
public:
    ServiceListItem(const RadioControlServiceListEntry & item, bool fav = false, int currentEns = 0);

    bool addEnsemble(EnsembleListItem * ensPtr);     // returns true when new ensemble was added
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

#endif // SERVICELISTITEM_H
