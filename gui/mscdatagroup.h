#ifndef MSCDATAGROUP_H
#define MSCDATAGROUP_H

#include <QByteArray>

class MSCDataGroup
{
public:
    explicit MSCDataGroup(const QByteArray &dataGroup);
    QByteArray::const_iterator dataFieldConstBegin() const;

    uint8_t getType() const;
    uint16_t getSegmentNum() const;
    uint16_t getTransportId() const;
    bool getLastFlag() const;
    bool isValid() const;

private:
    bool m_isValid = false;
    bool m_extensionFlag;
    bool m_segmentFlag;
    bool m_userAccessFlag;
    uint8_t m_type;
    uint8_t m_continuityIdx;
    uint8_t m_repetitionIdx;
    uint16_t m_extensionField;
    bool m_lastFlag;
    uint16_t m_segmentNum;
    bool m_transportIdFlag;
    uint8_t m_lengthIndicator;
    uint16_t m_transportId;
    QByteArray m_endUserAddrField;
    QByteArray m_dataField;

    bool crc16check(const QByteArray &data);
};

#endif // MSCDATAGROUP_H
