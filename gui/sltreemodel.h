#ifndef SLTREEMODEL_H
#define SLTREEMODEL_H

#include <QAbstractItemModel>
#include <QObject>
#include "slmodelitem.h"

#include <servicelist.h>

class SLTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SLTreeModel(ServiceList * sl, QObject *parent = 0);
    ~SLTreeModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
//    QVariant headerData(int section, Qt::Orientation orientation,
//                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    uint64_t getId(const QModelIndex &index) const;
    bool isService(const QModelIndex &index) const;
    bool isEnsemble(const QModelIndex &index) const;
    bool isFavoriteService(const QModelIndex &index) const;

public slots:
    void addEnsembleService(const EnsembleListItem *e, const ServiceListItem *s);
    void clear();

private:
    SLModelItem *rootItem;
    ServiceList * slPtr;
};

#endif // SLTREEMODEL_H
