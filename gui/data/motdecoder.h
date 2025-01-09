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

#ifndef MOTDECODER_H
#define MOTDECODER_H

#include <QByteArray>
#include <QObject>

#include "motobject.h"

class MOTDecoder : public QObject
{
    Q_OBJECT

public:
    explicit MOTDecoder(QObject *parent = nullptr);
    ~MOTDecoder();
    void newDataGroup(const QByteArray &dataGroup);
    void reset();
    bool hasDirectory() const { return m_directory != nullptr; }
    int directoryCount() const { return hasDirectory() ? m_directory->count() : -1; }
    int directoryCountCompleted() const { return hasDirectory() ? m_directory->countCompleted() : -1; }
    int directoryIsComplete() const { return hasDirectory() ? m_directory->isComplete() : false; }
    uint_fast32_t getDirectoryId() const { return hasDirectory() ? m_directory->getTransportId() : 0xFFFFFFFF; }
    MOTObjectCache::const_iterator find(uint16_t transportId) const { return m_directory->cfind(transportId); }
    MOTObjectCache::const_iterator find(const QString &filename) const;
    MOTObjectCache::const_iterator directoryBegin() const { return m_directory->begin(); }
    MOTObjectCache::const_iterator directoryEnd() const { return m_directory->end(); }

signals:
    void newMOTObject(const MOTObject &obj);
    void newMOTDirectory();

    // ETSI EN 301 234 V2.1.1 (2006-05) [6.2.2.1.1 ContentName]
    // The parameter ContentName is used to uniquely identify an object. At any time only one object with a certain ContentName shall be broadcast.
    void newMOTObjectInDirectory(const QString &contentName);

    void directoryComplete();

private:
    MOTDirectory *m_directory;
    MOTObjectCache *m_objCache;
};

#endif  // MOTDECODER_H
