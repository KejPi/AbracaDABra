#include <QQuickView>
#include <QQmlContext>
#include <QHBoxLayout>
#include <QPushButton>

#include "epgdialog.h"
#include "ui_epgdialog.h"

EPGDialog::EPGDialog(SLModel * serviceListModel, MetadataManager * metadataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EPGDialog)
    , m_serviceListModel(serviceListModel)
    , m_metadataManager(metadataManager)
    , m_isVisible(false)
{
    ui->setupUi(this);

    QQuickView *qmlView = new QQuickView();

    QQmlContext * context = qmlView->rootContext();
    context->setContextProperty("slModel", m_serviceListModel);
    context->setContextProperty("epgDialog", this);

    QQmlEngine *engine = qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), m_metadataManager);

    qmlView->setColor(Qt::transparent);
    qmlView->setSource(QUrl("qrc:/EPG/qml/epg.qml"));

    qRegisterMetaType<EPGModel>("EPGModel" );

    QWidget *container = QWidget::createWindowContainer(qmlView, this);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->addWidget(container);

    QPushButton * button = new QPushButton("Test", this);
    layout->addWidget(button);
}

EPGDialog::~EPGDialog()
{
    delete ui;
}

bool EPGDialog::isVisible() const
{
    return m_isVisible;
}

void EPGDialog::setIsVisible(bool newIsVisible)
{
    if (m_isVisible == newIsVisible)
        return;
    m_isVisible = newIsVisible;
    emit isVisibleChanged();
}

void EPGDialog::showEvent(QShowEvent *event)
{
    qDebug() << "show event";
    setIsVisible(true);
    QDialog::showEvent(event);
}

void EPGDialog::closeEvent(QCloseEvent *event)
{
    qDebug() << "close event";
    setIsVisible(false);
    QDialog::closeEvent(event);
}
