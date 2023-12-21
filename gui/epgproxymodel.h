#ifndef EPGPROXYMODEL_H
#define EPGPROXYMODEL_H

#include <QQmlEngine>
#include <QSortFilterProxyModel>

class EPGProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int dayFilter READ dayFilter WRITE setDayFilter NOTIFY dayFilterChanged FINAL)
public:
    explicit EPGProxyModel(QObject *parent = nullptr);
    int dayFilter() const;
    void setDayFilter(int newDayFilter);

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:
    void dayFilterChanged();
private:
    int m_dayFilter;
};

#endif // EPGPROXYMODEL_H
