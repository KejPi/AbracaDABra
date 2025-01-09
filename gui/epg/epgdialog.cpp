/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "epgdialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QQmlContext>

#include "epgproxymodel.h"
#include "epgtime.h"
#include "ui_epgdialog.h"

EPGDialog::EPGDialog(SLModel *serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager *metadataManager, Settings *settings,
                     QWidget *parent)
    : QDialog(parent), ui(new Ui::EPGDialog), m_metadataManager(metadataManager), m_settings(settings), m_currentUEID(0)
{
    ui->setupUi(this);

#ifndef Q_OS_MAC
    // Set window flags to add minimize buttons
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
#endif

    if (!m_settings->epg.geometry.isEmpty())
    {
        restoreGeometry(m_settings->epg.geometry);
        QSize sz = size();
        QTimer::singleShot(10, this, [this, sz]() { resize(sz); });
    }

    m_slProxyModel = new SLProxyModel(this);
    m_slProxyModel->setSourceModel(serviceListModel);
    connect(this, &EPGDialog::filterEmptyEpgChanged, [this]() { m_slProxyModel->setEmptyEpgFilter(m_settings->epg.filterEmptyEpg); });
    connect(this, &EPGDialog::filterEnsembleChanged, [this]() { m_slProxyModel->setUeidFilter(m_settings->epg.filterEnsemble ? m_currentUEID : 0); });
    // set initial state
    m_slProxyModel->setEmptyEpgFilter(m_settings->epg.filterEmptyEpg);
    m_slProxyModel->setUeidFilter(m_settings->epg.filterEnsemble ? m_currentUEID : 0);

    m_qmlView = new QQuickView();

    QQmlContext *context = m_qmlView->rootContext();
    context->setContextProperty("slProxyModel", m_slProxyModel);
    context->setContextProperty("metadataManager", m_metadataManager);
    context->setContextProperty("epgTime", EPGTime::getInstance());
    context->setContextProperty("slSelectionModel", slSelectionModel);
    context->setContextProperty("epgDialog", this);

    setupDarkMode(false);

    QQmlEngine *engine = m_qmlView->engine();
    engine->addImageProvider(QLatin1String("metadata"), new LogoProvider(m_metadataManager));

    // m_qmlView->setColor(Qt::transparent);
    m_qmlView->setSource(QUrl("qrc:/app/qmlcomponents/epg.qml"));

    QWidget *container = QWidget::createWindowContainer(m_qmlView, this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container);

    // QPushButton * button = new QPushButton("Test", this);
    // layout->addWidget(button);
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
    return m_settings->epg.selectedItem;
}

void EPGDialog::setSelectedEpgItem(const QPersistentModelIndex &newSelectedEpgItem)
{
    if (m_settings->epg.selectedItem == newSelectedEpgItem)
    {
        return;
    }
    m_settings->epg.selectedItem = newSelectedEpgItem;
    emit selectedEpgItemChanged();
}

bool EPGDialog::isVisible() const
{
    return m_isVisible;
}

void EPGDialog::setIsVisible(bool newIsVisible)
{
    if (m_isVisible == newIsVisible)
    {
        return;
    }
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
    m_settings->epg.geometry = saveGeometry();
    setIsVisible(false);
    QDialog::closeEvent(event);
}

bool EPGDialog::filterEmptyEpg() const
{
    return m_settings->epg.filterEmptyEpg;
}

void EPGDialog::setFilterEmptyEpg(bool newFilterEmptyEpg)
{
    if (m_settings->epg.filterEmptyEpg == newFilterEmptyEpg)
    {
        return;
    }
    m_settings->epg.filterEmptyEpg = newFilterEmptyEpg;
    emit filterEmptyEpgChanged();
}

void EPGDialog::onEnsembleInformation(const RadioControlEnsemble &ens)
{
    m_currentUEID = ens.ueid;
    if (m_settings->epg.filterEnsemble)
    {
        m_slProxyModel->setUeidFilter(m_currentUEID);
    }
}

bool EPGDialog::filterEnsemble() const
{
    return m_settings->epg.filterEnsemble;
}

void EPGDialog::setFilterEnsemble(bool newFilterEnsemble)
{
    if (m_settings->epg.filterEnsemble == newFilterEnsemble)
    {
        return;
    }
    m_settings->epg.filterEnsemble = newFilterEnsemble;
    emit filterEnsembleChanged();
}

void EPGDialog::setupDarkMode(bool darkModeEna)
{
    m_isDarkMode = darkModeEna;
    QList<QColor> colors;

    if (darkModeEna)
    {
        colors.append(qApp->palette().color(QPalette::Window));
        colors.append(QColor(0xec, 0xec, 0xec));  // textColor
        colors.append(QColor(0xb3, 0xb3, 0xb3));  // fadeTextColor
        colors.append(QColor(0x68, 0x68, 0x68));  // gridColor
        colors.append(QColor(0x00, 0xcb, 0xff));  // highlightColor
        colors.append(QColor(0x00, 0xcb, 0xff));  // selectedBorderColor
        colors.append(QColor(0x33, 0x33, 0x33));  // pastProgColor
        colors.append(QColor(0x51, 0x51, 0x51));  // nextProgColor
        colors.append(QColor(0x33, 0x61, 0x7d));  // currentProgColor
        colors.append(QColor(0x17, 0x84, 0xb9));  // progressColor
        colors.append(QColor(Qt::black));         // emptyLogoColor
        colors.append(QColor(Qt::black));         // shadowColor

        colors.append(QColor(0xa0, 0xa0, 0xa0));  // switchBgColor
        colors.append(QColor(0xc0, 0xc0, 0xc0));  // switchBorderColor
        colors.append(QColor(0x51, 0x51, 0x51));  // switchHandleColor
        colors.append(QColor(0xb0, 0xb0, 0xb0));  // switchHandleBorderColor
        colors.append(QColor(0xd0, 0xd0, 0xd0));  // switchHandleDownColor

        colors.append(QColor(0x65, 0x65, 0x65));  // buttonColor
        colors.append(QColor(0x7c, 0x7c, 0x7c));  // buttonDownColor
        colors.append(QColor(0x28, 0x28, 0x28));  // buttonBorderColor

        m_qmlView->setColor(Qt::black);
    }
    else
    {
        colors.append(qApp->palette().color(QPalette::Window));
        colors.append(QColor(Qt::black));         // textColor
        colors.append(QColor(0x60, 0x60, 0x60));  // fadeTextColor
        colors.append(QColor(0xc0, 0xc0, 0xc0));  // gridColor
        colors.append(QColor(0x3e, 0x9b, 0xfc));  // highlightColor
        colors.append(QColor(0x3e, 0x9b, 0xfc));  // selectedBorderColor
        colors.append(QColor(0xe4, 0xe4, 0xe4));  // pastProgColor
        colors.append(QColor(0xf9, 0xf9, 0xf9));  // nextProgColor
        colors.append(QColor(0xe2, 0xf4, 0xff));  // currentProgColor
        colors.append(QColor(0xa4, 0xde, 0xff));  // progressColor
        colors.append(QColor(Qt::white));         // emptyLogoColor
        colors.append(QColor(Qt::darkGray));      // shadowColor

        colors.append(QColor(0xd9, 0xd9, 0xd9));  // switchBgColor
        colors.append(QColor(0xcc, 0xcc, 0xcc));  // switchBorderColor
        colors.append(QColor(Qt::white));         // switchHandleColor
        colors.append(QColor(0x99, 0x99, 0x99));  // switchHandleBorderColor
        colors.append(QColor(0xcc, 0xcc, 0xcc));  // switchHandleDownColor

        colors.append(QColor(Qt::white));         // buttonColor
        colors.append(QColor(0xf0, 0xf0, 0xf0));  // buttonDownColor
        colors.append(QColor(0xb4, 0xb4, 0xb4));  // buttonBorderColor

        m_qmlView->setColor(Qt::white);
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
    {
        return;
    }
    m_colors = newColors;
    emit colorsChanged();
}

void EPGDialog::scheduleRecording()
{
    if (m_settings->epg.selectedItem.isValid())
    {
        const EPGModel *model = dynamic_cast<const EPGModel *>(m_settings->epg.selectedItem.model());
        AudioRecScheduleItem item;
        item.setName(model->data(m_settings->epg.selectedItem, EPGModelRoles::NameRole).toString());
        item.setStartTime(model->data(m_settings->epg.selectedItem, EPGModelRoles::StartTimeRole).value<QDateTime>());
        item.setDurationSec(model->data(m_settings->epg.selectedItem, EPGModelRoles::DurationSecRole).toInt());
        item.setServiceId(model->serviceId());

        emit scheduleAudioRecording(item);
    }
}
