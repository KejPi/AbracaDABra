/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "motdecoder.h"

#include <QDebug>
#include <QLoggingCategory>

#include "mscdatagroup.h"

Q_LOGGING_CATEGORY(motDecoder, "MOTDecoder", QtInfoMsg)

MOTDecoder::MOTDecoder(QObject *parent) : QObject(parent)
{
    m_directory = nullptr;
    m_objCache = new MOTObjectCache;
}

MOTDecoder::~MOTDecoder()
{
    if (nullptr != m_directory)
    {  // delete existing MOT directory
        delete m_directory;
    }
    delete m_objCache;
}

void MOTDecoder::reset()
{
    if (nullptr != m_directory)
    {  // delete existing MOT directory
        delete m_directory;
        m_directory = nullptr;
    }
    m_objCache->clear();
}

MOTObjectCache::const_iterator MOTDecoder::find(const QString &filename) const
{
    for (auto it = m_directory->begin(); it < m_directory->end(); ++it)
    {
        if (it->getContentName() == filename)
        {
            return it;
        }
    }
    return m_directory->end();
}

void MOTDecoder::newDataGroup(const QByteArray &dataGroup)
{
    qCDebug(motDecoder) << Q_FUNC_INFO << "Data group len=" << dataGroup.size();

    MSCDataGroup mscDataGroup(dataGroup);
    if (!mscDataGroup.isValid())
    {  // data group was not valid - wrong CRC, or length
        return;
    }
    else
    { /* datagroup was valid */
    }

    // unsigned required
    const uint8_t *dataFieldPtr = reinterpret_cast<const uint8_t *>(mscDataGroup.dataFieldConstBegin());

    // [ETSI EN 301 234, 5.1.1 Segmentation header]
    uint8_t repetitionCount = (*dataFieldPtr >> 5) & 0x7;
    // [ETSI EN 301 234, 5.1 Segmentation of MOT entities]
    // MOT entities will be split up in segments with equal size.
    // Only the last segment may have a smaller size (to carry the remaining bytes of the MOT entity).
    uint16_t segmentSize = (*dataFieldPtr++ << 8) & 0x1FFF;
    segmentSize += *dataFieldPtr++;

    qCDebug(motDecoder) << "Data group type =" << mscDataGroup.getType();
    qCDebug(motDecoder) << "Segment number = " << mscDataGroup.getSegmentNum() << ", last = " << mscDataGroup.getLastFlag();
    qCDebug(motDecoder) << "Segment size = " << segmentSize << ", Repetition count = " << repetitionCount;

    switch (mscDataGroup.getType())
    {
        case 3:
        {  // [ETSI EN 301 234, 5.1.4 Segmentation of the MOT header]
            // The segments of the MOT header shall be transported in MSC Data group type 3.
            // this is header mode
            MOTObjectCache::iterator objIt = m_objCache->findMotObj(mscDataGroup.getTransportId());
            if (m_objCache->end() == objIt)
            {  // object does not exist in cache
                qCDebug(motDecoder) << "New MOT header ID" << mscDataGroup.getTransportId() << "number of objects in carousel" << m_objCache->size();

                // all existing object shall be removed, only one MOT object is transmitted in header mode
                m_objCache->clear();

                // add new object to cache
                objIt = m_objCache->addMotObj(MOTObject(mscDataGroup.getTransportId()));
            }
            else
            { /* do nothing - it already exists, just adding next segment */
            }

            // add header segment
            objIt->addSegment((const uint8_t *)dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag(), true);
        }
        break;
        case 4:
        {  // [ETSI EN 301 234, 5.1.2 Segmentation of the MOT body]
            // If conditional access on MOT level is applied, then scrambled MOT body segments
            // shall be transported in MSC data group type 5. In all other cases (no scrambling on MOT level
            // or unscrambled MOT body segments) the segments of the MOT body shall be transported in MSC data group type 4.

            // first check if we are in direcory mode
            if (nullptr != m_directory)
            {  // directory mode
                if (m_directory->addObjectSegment(mscDataGroup.getTransportId(), (const uint8_t *)dataFieldPtr, mscDataGroup.getSegmentNum(),
                                                  segmentSize, mscDataGroup.getLastFlag()))
                {  // directory updated
                    emit newMOTObjectInDirectory(m_directory->cfind(mscDataGroup.getTransportId())->getContentName());
                    if (m_directory->isComplete())
                    {
                        emit directoryComplete();
                    }
                }
            }
            else
            {  // this can be either directory mode but directory was not recieved yet or it can be header mode
                // Header mode is handled within the cache
                MOTObjectCache::iterator objIt = m_objCache->findMotObj(mscDataGroup.getTransportId());
                if (m_objCache->end() == objIt)
                {  // does not exist in cache -> body without header
                    // add new object to cache
                    qCDebug(motDecoder) << "New MOT object ID" << mscDataGroup.getTransportId() << "number of objects in carousel"
                                        << m_objCache->size();

                    objIt = m_objCache->addMotObj(MOTObject(mscDataGroup.getTransportId()));
                }
                objIt->addSegment((const uint8_t *)dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag());

                if (objIt->isComplete())
                {
                    QByteArray body = objIt->getBody();
                    qCDebug(motDecoder) << "MOT complete :)";

                    emit newMOTObject(*objIt);

                    m_objCache->deleteMotObj(objIt->getId());
                }
            }
        }
        break;
        case 6:
        {  // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
            // The segments of an uncompressed MOT directory shall be transported in MSC Data Group type 6.
            if (nullptr != m_directory)
            {  // some directory exists
                // lets check if the segment belong to current directory
                if (m_directory->getTransportId() != mscDataGroup.getTransportId())
                {  // new directory
                    delete m_directory;
                    m_directory = new MOTDirectory(mscDataGroup.getTransportId(), m_objCache);
                }
                else
                { /* segment belongs to existing directory */
                }
            }
            else
            {  // directory does not exist -> creating new directory
                m_directory = new MOTDirectory(mscDataGroup.getTransportId(), m_objCache);
            }

            if (m_directory->addSegment((const uint8_t *)dataFieldPtr, mscDataGroup.getSegmentNum(), segmentSize, mscDataGroup.getLastFlag()))
            {  // directory just completed
                qCDebug(motDecoder) << "MOT Directory is complete";
                emit newMOTDirectory();
            }
        }
        break;
        case 7:
            // [ETSI EN 301 234, 5.1.3 Segmentation of the MOT directory]
            // The segments of a compressed MOT directory shall be transported in MSC Data Group type 7.
            qCWarning(motDecoder) << "Compressed MOT directory not supported";
            break;
        default:
            return;
    }
}
