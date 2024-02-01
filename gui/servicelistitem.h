/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef SERVICELISTITEM_H
#define SERVICELISTITEM_H

#include <stdint.h>
#include <QString>
#include <QList>
#include "servicelistid.h"
#include "radiocontrol.h"

class EnsembleListItem;

class ServiceListItem
{
public:
    ServiceListItem(const RadioControlServiceComponent & item, int currentEns = 0);

    bool update(const RadioControlServiceComponent &item);
    bool addEnsemble(EnsembleListItem * ensPtr);     // returns true when new ensemble was added
    bool removeEnsemble(EnsembleListItem * ensPtr);     // returns true when new ensemble was added
    //void setFavorite(bool ena) { m_favorite = ena; }

    ServiceListId id() const { return m_id; }

    DabSId SId() const { return m_sid; }
    uint8_t SCIdS() const { return m_scids; }
    QString label() const { return m_label; }
    QString shortLabel() const { return m_shortLabel; }
    int numEnsembles() const { return m_ensembleList.size(); }
    //bool isFavorite() const { return m_favorite; }
    const EnsembleListItem * getEnsemble(int num = -1) const;          // returns ensemble idx (wraps over end of list)
    const EnsembleListItem * switchEnsemble(const ServiceListId &id);  // switches current ensemble, returns ensemble
    uint32_t currentEnsembleIdx() const { return m_currentEnsemble; }  // used to save service list

    bool operator==(const ServiceListItem & other) const;

    bool isObsolete() const;
    void setIsObsolete(bool isObsolete);
private:
    // Service
    ServiceListId m_id;     // unique service list ID
    DabSId m_sid;           // SId (contains ECC)
    uint8_t m_scids;        // service component ID within the service
    QString m_label;        // Service label
    QString m_shortLabel;   // Short label
    //bool m_favorite;        // Favorite service
    int m_currentEnsemble;
    bool m_isObsolete;      // this is used for ensemble update

    QList<EnsembleListItem *> m_ensembleList;

    ServiceListItem() = delete;           // disabled
    QList<EnsembleListItem *>::iterator findEnsemble(const ServiceListId &id);
};

#endif // SERVICELISTITEM_H
