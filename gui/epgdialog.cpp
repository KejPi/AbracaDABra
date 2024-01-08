/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <QQmlContext>
#include <QHBoxLayout>
#include <QPushButton>

#include "epgproxymodel.h"
#include "epgdialog.h"
#include "epgtime.h"
#include "ui_epgdialog.h"

EPGDialog::EPGDialog(SLModel * serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager * metadataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EPGDialog)
    , m_serviceListModel(serviceListModel)
    , m_metadataManager(metadataManager)
{
    ui->setupUi(this);

    qmlRegisterType<SLModel>("ProgrammeGuide", 1, 0, "SLModel");
    qmlRegisterType<EPGModel>("ProgrammeGuide", 1, 0, "EPGModel");
    qmlRegisterType<EPGProxyModel>("ProgrammeGuide", 1, 0, "EPGProxyModel");

    m_qmlView = new QQuickView();

    QQmlContext * context = m_qmlView->rootContext();
    context->setContextProperty("slModel", m_serviceListModel);
    context->setContextProperty("metadataManager", m_metadataManager);
    context->setContextProperty("epgTime", EPGTime::getInstance());
    context->setContextProperty("slSelectionModel", slSelectionModel);
    context->setContextProperty("epgDialog", this);

    QQmlEngine *engine = m_qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), new LogoProvider(m_metadataManager));

    m_qmlView->setColor(Qt::transparent);
    m_qmlView->setSource(QUrl("qrc:/qml/epg.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->addWidget(container);

    //QPushButton * button = new QPushButton("Test", this);
    //layout->addWidget(button);
    setMinimumWidth(600);
    setMinimumHeight(400);
}

EPGDialog::~EPGDialog()
{
    delete m_qmlView;
    delete ui;
}

QPersistentModelIndex EPGDialog::selectedEpgItem() const
{
    return m_selectedEpgItem;
}

void EPGDialog::setSelectedEpgItem(const QPersistentModelIndex &newSelectedEpgItem)
{
    if (m_selectedEpgItem == newSelectedEpgItem)
        return;
    m_selectedEpgItem = newSelectedEpgItem;
    emit selectedEpgItemChanged();
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
    qDebug() << Q_FUNC_INFO;
    setIsVisible(true);
    QDialog::showEvent(event);
}

void EPGDialog::closeEvent(QCloseEvent *event)
{
    qDebug() << Q_FUNC_INFO;
    setIsVisible(false);
    QDialog::closeEvent(event);
}
