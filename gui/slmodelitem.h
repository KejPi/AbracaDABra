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
    SLModelItem* findChildId(const ServiceListId &id) const;
    void sort(Qt::SortOrder order);    

private:
    QList<SLModelItem*> m_childItems;
    SLModelItem *m_parentItem;

    const ServiceList * m_slPtr;
    ServiceListId m_id;

};

#endif // SLMODELITEM_H
