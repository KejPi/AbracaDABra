#ifndef TIITABLEMODEL_H
#define TIITABLEMODEL_H

#include <QObject>
#include <QAbstractTableModel>

#include "dabsdr.h"

class TiiTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum { ColMainId, ColSubId, ColLevel, NumCols};

    explicit TiiTableModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void clear();

    void populateModel(const QList<dabsdrTii_t> & data);
private:

    QList<dabsdrTii_t> m_modelData;
};

#endif // TIITABLEMODEL_H
