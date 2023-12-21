#include "epgproxymodel.h"
#include "epgmodel.h"

EPGProxyModel::EPGProxyModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{}


int EPGProxyModel::dayFilter() const
{
    return m_dayFilter;
}

void EPGProxyModel::setDayFilter(int newDayFilter)
{
    if (m_dayFilter == newDayFilter)
        return;
    m_dayFilter = newDayFilter;
    emit dayFilterChanged();
}

bool EPGProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QDateTime date = sourceModel()->data(index, EPGModelRoles::StartTimeRole).value<QDateTime>();
    // QDateTime fakeToday;
    // fakeToday.setDate(QDate(2023, 12, 02));
    QDate fakeToday(2023, 12, 02);

    return date.date() ==  fakeToday.addDays(m_dayFilter);
}
