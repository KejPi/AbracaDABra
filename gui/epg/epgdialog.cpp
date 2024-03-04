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
    setupDarkMode(false);

    QQmlEngine *engine = m_qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), new LogoProvider(m_metadataManager));

    //m_qmlView->setColor(Qt::transparent);
    m_qmlView->setSource(QUrl("qrc:/ProgrammeGuide/epg.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QVBoxLayout * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
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

void EPGDialog::setupDarkMode(bool darkModeEna)
{
    m_isDarkMode = darkModeEna;
    QList<QColor> colors;

    if (darkModeEna)
    {        
        colors.append(qApp->palette().color(QPalette::Window));
        colors.append(QColor(0xec,0xec,0xec));   // textColor
        colors.append(QColor(0xb3,0xb3,0xb3));   // fadeTextColor
        colors.append(QColor(0x68,0x68,0x68));   // gridColor
        colors.append(QColor(0x00,0xcb,0xff));   // highlightColor
        colors.append(QColor(0x00,0xcb,0xff));   // selectedBorderColor
        colors.append(QColor(0x33,0x33,0x33));   // pastProgColor
        colors.append(QColor(0x51,0x51,0x51));   // nextProgColor
        colors.append(QColor(0x33,0x61,0x7d));   // currentProgColor
        colors.append(QColor(0x17,0x84,0xb9));   // progressColor
        colors.append(QColor(Qt::black));        // emptyLogoColor
        colors.append(QColor(Qt::black));        // shadowColor

        colors.append(QColor(0xa0,0xa0,0xa0));   // switchBgColor
        colors.append(QColor(0xc0,0xc0,0xc0));   // switchBorderColor
        colors.append(QColor(0x51,0x51,0x51));   // switchHandleColor
        colors.append(QColor(0xb0,0xb0,0xb0));   // switchHandleBorderColor
        colors.append(QColor(0xd0,0xd0,0xd0));   // switchHandleDownColor

        colors.append(QColor(0x65,0x65,0x65));   // buttonColor
        colors.append(QColor(0x7c,0x7c,0x7c));   // buttonDownColor
        colors.append(QColor(0x28,0x28,0x28));   // buttonBorderColor
    }
    else
    {
        colors.append(qApp->palette().color(QPalette::Window));
        colors.append(QColor(Qt::black));        // textColor
        colors.append(QColor(0x60,0x60,0x60));   // fadeTextColor
        colors.append(QColor(0xc0,0xc0,0xc0));   // gridColor
        colors.append(QColor(0x3e,0x9b,0xfc));   // highlightColor
        colors.append(QColor(0x3e,0x9b,0xfc));   // selectedBorderColor
        colors.append(QColor(0xe4,0xe4,0xe4));   // pastProgColor
        colors.append(QColor(0xf9,0xf9,0xf9));   // nextProgColor
        colors.append(QColor(0xe2,0xf4,0xff));   // currentProgColor
        colors.append(QColor(0xa4,0xde,0xff));   // progressColor
        colors.append(QColor(Qt::white));        // emptyLogoColor
        colors.append(QColor(Qt::darkGray));     // shadowColor

        colors.append(QColor(0xd9,0xd9,0xd9));   // switchBgColor
        colors.append(QColor(0xcc,0xcc,0xcc));   // switchBorderColor
        colors.append(QColor(Qt::white));        // switchHandleColor
        colors.append(QColor(0x99,0x99,0x99));   // switchHandleBorderColor
        colors.append(QColor(0xcc,0xcc,0xcc));   // switchHandleDownColor

        colors.append(QColor(Qt::white));        // buttonColor
        colors.append(QColor(0xf0,0xf0,0xf0));   // buttonDownColor
        colors.append(QColor(0xb4,0xb4,0xb4));   // buttonBorderColor
    }
    setColors(colors);
}

QList<QColor> EPGDialog::colors() const
{
    return m_colors;
}

void EPGDialog::setColors(const QList<QColor> &newColors)
{
    if (m_colors == newColors)
        return;
    m_colors = newColors;
    emit colorsChanged();
}

void EPGDialog::scheduleRecording()
{
    if (m_selectedEpgItem.isValid())
    {
        const EPGModel * model = dynamic_cast<const EPGModel *>(m_selectedEpgItem.model());
        AudioRecScheduleItem item;
        item.setName(model->data(m_selectedEpgItem, EPGModelRoles::NameRole).toString());
        item.setStartTime(model->data(m_selectedEpgItem, EPGModelRoles::StartTimeRole).value<QDateTime>());
        item.setDurationSec(model->data(m_selectedEpgItem, EPGModelRoles::DurationSecRole).toInt());
        item.setServiceId(model->serviceId());

        emit scheduleAudioRecording(item);
    }
}
