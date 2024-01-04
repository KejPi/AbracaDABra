#ifndef EPGPROXYMODEL_H
#define EPGPROXYMODEL_H

#include <QDate>
#include <QQmlEngine>
#include <QSortFilterProxyModel>

class EPGProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QDate dateFilter READ dateFilter WRITE setDateFilter NOTIFY dateFilterChanged FINAL)
public:
    explicit EPGProxyModel(QObject *parent = nullptr);

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    QDate dateFilter() const;
    void setDateFilter(const QDate &newDateFilter);

signals:
    void dateFilterChanged();

private:
    QDate m_dateFilter;
};

#endif // EPGPROXYMODEL_H
