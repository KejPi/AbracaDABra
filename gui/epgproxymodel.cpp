#include "epgproxymodel.h"
#include "epgmodel.h"

EPGProxyModel::EPGProxyModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{
    qDebug() << "Proxy model created";
}

bool EPGProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QDate date = sourceModel()->data(index, EPGModelRoles::StartTimeRole).value<QDateTime>().date();
    return date == m_dateFilter;
}

QDate EPGProxyModel::dateFilter() const
{
    return m_dateFilter;
}

void EPGProxyModel::setDateFilter(const QDate &newDateFilter)
{
    if (m_dateFilter == newDateFilter)
        return;
    m_dateFilter = newDateFilter;
    emit dateFilterChanged();

    qDebug() << "Date filter set" << m_dateFilter;
}
