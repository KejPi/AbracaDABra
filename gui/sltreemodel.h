#ifndef SLTREEMODEL_H
#define SLTREEMODEL_H

#include <QAbstractItemModel>
#include <QObject>

#include "slmodelitem.h"
#include "servicelist.h"

class SLTreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit SLTreeModel(const ServiceList * sl, QObject *parent = 0);
    ~SLTreeModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    ServiceListId id(const QModelIndex &index) const;
    bool isService(const QModelIndex &index) const;
    bool isEnsemble(const QModelIndex &index) const;
    bool isFavoriteService(const QModelIndex &index) const;

public slots:
    void addEnsembleService(const ServiceListId & ensId, const ServiceListId & servId);
    void updateEnsembleService(const ServiceListId & ensId, const ServiceListId & servId);
    void removeEnsembleService(const ServiceListId &ensId, const ServiceListId &servId);
    void clear();

private:
    SLModelItem * m_rootItem;
    const ServiceList * m_slPtr;
};

#endif // SLTREEMODEL_H
