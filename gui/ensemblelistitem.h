/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
    bool update(const RadioControlEnsemble &ens);
    void beginUpdate();
    void endUpdate();

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
