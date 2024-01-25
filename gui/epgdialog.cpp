/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2024 Petr Kopecký <xkejpi (at) gmail (dot) com>
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
#include <QQuickStyle>

#include "epgproxymodel.h"
#include "epgdialog.h"
#include "epgtime.h"
#include "ui_epgdialog.h"

EPGDialog::EPGDialog(SLModel * serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager * metadataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EPGDialog)
    , m_metadataManager(metadataManager)
    , m_currentUEID(0)
{
    ui->setupUi(this);

    m_slProxyModel = new SLProxyModel(this);
    m_slProxyModel->setSourceModel(serviceListModel);
    connect(this, &EPGDialog::filterEmptyEpgChanged, [this](){ m_slProxyModel->setEmptyEpgFilter(m_filterEmptyEpg); });
    connect(this, &EPGDialog::filterEnsembleChanged, [this](){ m_slProxyModel->setUeidFilter(m_filterEnsemble ? m_currentUEID : 0); });
    setFilterEmptyEpg(false);
    setFilterEnsemble(false);

    qmlRegisterType<SLProxyModel>("ProgrammeGuide", 1, 0, "SLProxyModel");
    qmlRegisterType<EPGModel>("ProgrammeGuide", 1, 0, "EPGModel");
    qmlRegisterType<EPGProxyModel>("ProgrammeGuide", 1, 0, "EPGProxyModel");

    m_qmlView = new QQuickView();

    QQmlContext * context = m_qmlView->rootContext();
    context->setContextProperty("slProxyModel", m_slProxyModel);
    context->setContextProperty("metadataManager", m_metadataManager);
    context->setContextProperty("epgTime", EPGTime::getInstance());
    context->setContextProperty("slSelectionModel", slSelectionModel);
    context->setContextProperty("epgDialog", this);

    QQuickStyle::setStyle("Fusion");

    QQmlEngine *engine = m_qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), new LogoProvider(m_metadataManager));

    m_qmlView->setColor(Qt::transparent);
    m_qmlView->setSource(QUrl("qrc:/ProgrammeGuide/epg.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,10,0,0);
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
    setIsVisible(true);
    QDialog::showEvent(event);
}

void EPGDialog::closeEvent(QCloseEvent *event)
{
    setIsVisible(false);
    QDialog::closeEvent(event);
}


bool EPGDialog::filterEmptyEpg() const
{
    return m_filterEmptyEpg;
}

void EPGDialog::setFilterEmptyEpg(bool newFilterEmptyEpg)
{
    if (m_filterEmptyEpg == newFilterEmptyEpg)
        return;
    m_filterEmptyEpg = newFilterEmptyEpg;
    emit filterEmptyEpgChanged();
}

void EPGDialog::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_currentUEID = ens.ueid;
    if (m_filterEnsemble)
    {
        m_slProxyModel->setUeidFilter(m_currentUEID);
    }
}

bool EPGDialog::filterEnsemble() const
{
    return m_filterEnsemble;
}

void EPGDialog::setFilterEnsemble(bool newFilterEnsemble)
{
    if (m_filterEnsemble == newFilterEnsemble)
        return;
    m_filterEnsemble = newFilterEnsemble;
    emit filterEnsembleChanged();
}
