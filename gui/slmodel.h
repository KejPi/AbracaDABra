#ifndef SLMODEL_H
#define SLMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QList>
#include <QIcon>
#include <slmodelitem.h>

#include <servicelist.h>

class SLModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SLModel(const ServiceList * sl, QObject *parent = 0);
    ~SLModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    ServiceListId id(const QModelIndex &index) const;
    bool isFavoriteService(const QModelIndex &index) const;

public slots:
    void addService(const ServiceListId & servId);
    void removeService(const ServiceListId & servId);
    void clear();

private:
    const ServiceList * m_slPtr;
    QList<SLModelItem *> m_serviceItems;

    QIcon m_favIcon;
    QIcon m_noIcon;
};

#endif // SLMODEL_H
