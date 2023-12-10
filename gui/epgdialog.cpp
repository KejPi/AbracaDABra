#include <QQuickView>
#include <QQmlContext>
#include <QHBoxLayout>
#include <QPushButton>

#include "epgdialog.h"
#include "ui_epgdialog.h"

EPGDialog::EPGDialog(MetadataManager * metadataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EPGDialog)
    , m_metadataManager(metadataManager)
{
    ui->setupUi(this);

    QQuickView *qmlView = new QQuickView();

    QQmlContext * context = qmlView->rootContext();
    context->setContextProperty("epgModel", m_metadataManager->epgModel());


    qmlView->setColor(Qt::transparent);
    qmlView->setSource(QUrl("qrc:/EPG/epg.qml"));

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
