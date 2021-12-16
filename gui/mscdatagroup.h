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
    bool valid = false;
    bool extensionFlag;
    bool segmentFlag;
    bool userAccessFlag;
    uint8_t type;
    uint8_t continuityIdx;
    uint8_t repetitionIdx;
    uint16_t extensionField;
    bool lastFlag;
    uint16_t segmentNum;
    bool transportIdFlag;
    uint8_t lengthIndicator;
    uint16_t transportId;
    QByteArray endUserAddrField;
    QByteArray dataField;

    bool crc16check(const QByteArray &data);
};

#endif // MSCDATAGROUP_H
