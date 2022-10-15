#ifndef SERVICELISTID_H
#define SERVICELISTID_H

#include <stdint.h>
#include "radiocontrol.h"

#define SERVICELISTID_SERVICE_MASK ((uint64_t(1)<<(32+8)) - 1)
#define SERVICELISTID_SID_MASK     ((uint64_t(1)<<(32)) - 1)
#define SERVICELISTID_UEID_MASK    ((uint64_t(1)<<(24)) - 1)

class ServiceListId
{
public:
    ServiceListId(uint64_t id = 0) { m_id = id; }
    ServiceListId(const RadioControlServiceComponent & s) { m_id = calcServiceId(s.SId.value(), s.SCIdS); };
    ServiceListId(const RadioControlEnsemble & e) { m_id = calcEnsembleId(e.frequency, e.ueid); }
    ServiceListId(uint32_t sid, uint8_t scids) { m_id = calcServiceId(sid, scids); }
    ServiceListId(uint32_t freq, uint32_t ueid) { m_id = calcEnsembleId(freq, ueid);  }
    bool isService() const { return 0 == (m_id & ~SERVICELISTID_SERVICE_MASK); }
    bool isEnsemble() const { return !isService(); }
    bool isValid() const {return 0 != m_id; }
    uint64_t value() const { return m_id; }

    uint32_t sid() const { return static_cast<uint32_t>(m_id & SERVICELISTID_SID_MASK ); }
    uint8_t scids() const { return static_cast<uint8_t>((m_id>>32) & 0xFFu ); }
    uint32_t ueid() const { return static_cast<uint32_t>(m_id & SERVICELISTID_UEID_MASK ); }

    bool operator==(const ServiceListId & other) const { return other.m_id == m_id; }

    // this is required for using it in hash
    operator uint64_t() const { return m_id;}
private:
    uint64_t m_id;

    uint64_t calcServiceId(uint32_t sid, uint8_t scids) { return ((uint64_t(scids)<<32) | sid); }
    uint64_t calcEnsembleId(uint32_t freq, uint32_t ueid) { return (uint64_t(freq)<<32) | ueid; }
};

#endif // SERVICELISTID_H
