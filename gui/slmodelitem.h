#ifndef SLMODELITEM_H
#define SLMODELITEM_H

#include <QList>
#include <QVariant>
#include "servicelist.h"

class SLModelItem
{
public:
    explicit SLModelItem(ServiceList *  slPtr, SLModelItem *parentItem = 0);
    explicit SLModelItem(ServiceList *  slPtr, const ServiceListItem *sPtr, SLModelItem *parentItem = 0);
    explicit SLModelItem(ServiceList *  slPtr, const EnsembleListItem * ePtr, SLModelItem *parentItem = 0);
    ~SLModelItem();

    void appendChild(SLModelItem *child);

    SLModelItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column, int role) const;
    int row() const;
    SLModelItem *parentItem();
    uint64_t getId() const;
    bool isService() const;    
    bool isEnsemble() const;
    bool isFavoriteService() const;
    uint32_t frequency() const;
    SLModelItem* findChildId(uint64_t id) const;
    void sort(Qt::SortOrder order);    

private:
    QList<SLModelItem*> m_childItems;
    SLModelItem *m_parentItem;

    ServiceList * m_slPtr;
    uint64_t m_serviceId = 0;
    uint64_t m_ensembleId = 0;


    QString label() const;
};

#endif // SLMODELITEM_H
