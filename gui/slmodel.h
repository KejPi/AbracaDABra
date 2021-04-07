#ifndef SLMODEL_H
#define SLMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <servicelist.h>

class SLModelItem;

class SLModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SLModel(ServiceList * sl, QObject *parent = 0);
    ~SLModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    uint64_t getId(const QModelIndex &index) const;
    bool isService(const QModelIndex &index) const;
    bool isEnsemble(const QModelIndex &index) const;

public slots:
    void addService(const ServiceListItem * s);
    void addEnsemble(const EnsembleListItem * e);
    void clear();

private:
    SLModelItem *rootItem;
    ServiceList * slPtr;
};

#endif // SLMODEL_H
