#include "dldecoder.h"
#include "dabtables.h"
#include <QString>
#include <QDebug>

DLDecoder::DLDecoder(QObject *parent) : QObject(parent)
{

}

void DLDecoder::reset()
{
    label.clear();
    dlCommand.clear();
    segmentCntr = 0;
}

bool DLDecoder::crc16check(const QByteArray & data)
{
    uint16_t crc = 0xFFFF;
    uint16_t mask = 0x1020;

    for (int n = 0; n < data.size()-2; ++n)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            uint16_t in = (data[n] >> bit) & 0x1;
            uint16_t tmp = in ^ ((crc>>15) & 0x1);
            crc <<= 1;

            crc ^= (tmp * mask);
            crc += tmp;
        }
    }

    uint16_t invTxCRC = ~(uint16_t(data[data.size()-2] << 8) | uint8_t(data[data.size()-1]));

    return (crc == invTxCRC);
}

void DLDecoder::newDataGroup(const QByteArray & dataGroup)
{
    // CRC check is done in dabProc
//    // check CRC
//    if (crc16check(dataGroup)) // + header + crc
    {   // CRC passed
        if (dataGroup[0] & 0x10)
        {  // C flag == 1
            switch (dataGroup[0] & 0x0F)
            {
            case 0b0001:
                // [0 0 0 1: clear display command - remove the dynamic label message from the display;]
                qDebug() << "DL: clear display command";
                break;
            case 0b0010:
                // [0 0 1 0: DL Plus command, see ETSI TS 102 980 [7].]
                qDebug() << "DL+ command";
#if DLDECODER_VERBOSE>0
                if (dataGroup[0] & 0x40)
                {  // first is set
                    qDebug("DL+ Link bit %d, cmd len = %d, CId = %d, CB = %d",
                           int((dataGroup[1] & 0x80) != 0), (dataGroup[1] & 0x0F) + 1,
                           (dataGroup[2] >> 4) & 0x0F, dataGroup[2] & 0x0F);

                }
                else
                {  // first is not set
                    qDebug("DL+ Link bit %d, semgment number %d, cmd len = %d, CId = %d, CB = %d",
                           int((dataGroup[1] & 0x80) != 0), (dataGroup[1] >> 4) & 0x7, (dataGroup[1] & 0x0F) + 1,
                           (dataGroup[2] >> 4) & 0x0F, dataGroup[2] & 0x0F);
                }

                if (0 == (dataGroup[2] & 0xF0))
                {  // DL+ tag command
                    uint8_t itemToggle = (dataGroup[2] >> 3) & 0x1;
                    uint8_t itemRunning = (dataGroup[2] >> 2) & 0x1;
                    uint8_t numTags = ((dataGroup[2]) & 0x3) + 1;

                    qDebug("DL+ Item toggle %d, Item Running %d, Num tags %d", itemToggle, itemRunning, numTags);

                    for (uint8_t t = 0; t<numTags; ++t)
                    {
                        uint8_t contentsType = dataGroup[3+3*t] & 0x7F;
                        uint8_t startMarker = dataGroup[4+3*t] & 0x7F;
                        uint8_t lengthMarker = dataGroup[5+3*t] & 0x7F;

                        qDebug("DL+ tag %d, start %d, length %d", contentsType, startMarker, lengthMarker);
                    }

                }

#endif

                break;
            default:
                qDebug() << "DL: Unexpected DL command";
            }
        }
        else
        {
            uint8_t numChars = (dataGroup[0] & 0x0F) + 1;

            if (dataGroup[0] & 0x40)
            {   // first
                charset = (dataGroup[1] >> 4) & 0x0F;
            }
            else
            {   // not first
                if ((label.size() == 0)
                        || (segmentCntr != ((dataGroup[1] & 0x70) >> 4))
                        || (label.size() + numChars > XPAD_DL_LEN_MAX))
                {  // missing first => reset and waiting until we receive first
                   // or missing some previous
                   // or label is just too long
                   reset();
                   return;
                }
            }
            segmentCntr++;  // incrementing expected segment number

            // copy characters

            label.append(dataGroup.mid(2, numChars));
            uint8_t t = dataGroup[0] & 0x80;
            if (dataGroup[0] & 0x20)
            {   // last
#if DLDECODER_VERBOSE>1
                qDebug() << QString("DL[new=%1] (charset %2) [len=%3]: %4").arg(t != toggle).arg(charset).arg(label.size()).arg(QString(label));
#endif
                //emit dlComplete(charset, label);
                emit dlComplete(DabTables::convertToQString(label.data(), charset, label.size()));

                toggle = t;
                reset();
            }
        }
    }
//    else
//    {
//        // do nothing
//    }
}
