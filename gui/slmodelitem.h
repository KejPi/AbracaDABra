#ifndef SLMODELITEM_H
#define SLMODELITEM_H

#include <QList>
#include <QVariant>
#include "servicelist.h"

class SLModelItem
{
public:
    explicit SLModelItem(SLModelItem *parentItem = 0);
    explicit SLModelItem(const ServiceListItem *sPtr, SLModelItem *parentItem = 0);
    explicit SLModelItem(const EnsembleListItem * ePtr, SLModelItem *parentItem = 0);
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
    void sort(Qt::SortOrder order);    

private:
    QList<SLModelItem*> m_childItems;
    SLModelItem *m_parentItem;

    const ServiceListItem * m_servicePtr = nullptr;
    const EnsembleListItem * m_ensemblePtr = nullptr;

    QString label();
};

#endif // SLMODELITEM_H
