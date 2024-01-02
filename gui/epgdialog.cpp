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

#include <QQuickView>
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

    QQuickView *qmlView = new QQuickView();

    QQmlContext * context = qmlView->rootContext();
    context->setContextProperty("slModel", m_serviceListModel);
    context->setContextProperty("metadataManager", m_metadataManager);
    context->setContextProperty("epgTime", EPGTime::getInstance());
    context->setContextProperty("slSelectionModel", slSelectionModel);

    QQmlEngine *engine = qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), new LogoProvider(m_metadataManager));

    qmlView->setColor(Qt::transparent);
    qmlView->setSource(QUrl("qrc:/qml/epg.qml"));

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
