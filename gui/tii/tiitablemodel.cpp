#include "tiitablemodel.h"

TiiTableModel::TiiTableModel(QObject *parent)
    : QAbstractTableModel{parent}
{}

int TiiTableModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.count();
}

int TiiTableModel::columnCount(const QModelIndex &parent) const
{
    return NumCols;
}

QVariant TiiTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.row() >= m_modelData.size() || index.row() < 0)
    {
        return QVariant();
    }

    const auto &item = m_modelData.at(index.row());
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case ColMainId:
            return m_modelData.at(index.row()).main;
        case ColSubId:
            return m_modelData.at(index.row()).sub;
        case ColLevel:
            return QString::number(static_cast<double>(m_modelData.at(index.row()).level), 'f', 4);
        }
    }
    return QVariant();
}

QVariant TiiTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColMainId:
            return tr("Main ID");
        case ColSubId:
            return tr("Sub ID");
        case ColLevel:
            return tr("Level");
        default:
            break;
        }
    }
    return QVariant();
}

void TiiTableModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    endResetModel();
}

void TiiTableModel::populateModel(const QList<dabsdrTii_t> &data)
{
    beginResetModel();
    m_modelData.clear();
    m_modelData = data;
    endResetModel();
}
