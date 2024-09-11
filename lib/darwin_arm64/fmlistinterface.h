#ifndef FMLISTINTERFACE_H
#define FMLISTINTERFACE_H

#include <QObject>
#include <QString>
#include <QNetworkReply>

#include <QtCore/QtGlobal>

#if defined(FMLIST_LIBRARY)
#  define FMLISTINTERFACE_EXPORT Q_DECL_EXPORT
#else
#  define FMLISTINTERFACE_EXPORT Q_DECL_IMPORT
#endif

class FMListInterfacePrivate;

class FMLISTINTERFACE_EXPORT FMListInterface  : public QObject
{
    Q_OBJECT
public:
    FMListInterface(const QString & dbFile);
    ~FMListInterface();
    void updateTiiData();
    QString version() const;

signals:
    void updateTiiDataFinished(QNetworkReply::NetworkError err);

private:
    FMListInterfacePrivate* d_ptr;
};

#endif // FMLISTINTERFACE_H
