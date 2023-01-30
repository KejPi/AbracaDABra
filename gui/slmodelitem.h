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

#ifndef SLMODELITEM_H
#define SLMODELITEM_H

#include <QList>
#include <QVariant>
#include "servicelist.h"

class SLModelItem
{
public:
    explicit SLModelItem(const ServiceList *  slPtr, SLModelItem *parentItem = 0);
    explicit SLModelItem(const ServiceList *slPtr, const ServiceListId &id, SLModelItem *parentItem = 0);
    ~SLModelItem();

    void appendChild(SLModelItem *child);    

    SLModelItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column, int role) const;
    int row() const;
    SLModelItem *parentItem();
    ServiceListId id() const;
    bool isService() const;    
    bool isEnsemble() const;
    bool isFavoriteService() const;
    QString label() const;
    QString shortLabel() const;
    uint32_t frequency() const;
    DabSId SId() const;
    SLModelItem* findChildId(const ServiceListId &id, bool recursive = false) const;
    bool removeChildId(const ServiceListId &id);
    void sort(Qt::SortOrder order);

private:
    QList<SLModelItem*> m_childItems;
    SLModelItem *m_parentItem;

    const ServiceList * m_slPtr;
    ServiceListId m_id;

};

#endif // SLMODELITEM_H
