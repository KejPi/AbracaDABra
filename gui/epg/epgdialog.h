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

#ifndef EPGDIALOG_H
#define EPGDIALOG_H

#include <QDialog>
#include <QQuickImageProvider>
#include <QQuickView>
#include <QItemSelectionModel>
#include "audiorecscheduleitem.h"
#include "metadatamanager.h"
#include "settings.h"
#include "slmodel.h"
#include "slproxymodel.h"

namespace Ui {
class EPGDialog;
}

class EPGDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QPersistentModelIndex selectedEpgItem READ selectedEpgItem WRITE setSelectedEpgItem NOTIFY selectedEpgItemChanged FINAL)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged FINAL)
    Q_PROPERTY(bool filterEmptyEpg READ filterEmptyEpg WRITE setFilterEmptyEpg NOTIFY filterEmptyEpgChanged FINAL)
    Q_PROPERTY(bool filterEnsemble READ filterEnsemble WRITE setFilterEnsemble NOTIFY filterEnsembleChanged FINAL)
    Q_PROPERTY(QList<QColor> colors READ colors NOTIFY colorsChanged FINAL)

public:
    explicit EPGDialog(SLModel *serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager *metadataManager, Settings *settings, QWidget *parent = nullptr);
    ~EPGDialog();

    QPersistentModelIndex selectedEpgItem() const;
    void setSelectedEpgItem(const QPersistentModelIndex &newSelectedEpgItem);

    bool isVisible() const;
    void setIsVisible(bool newIsVisible);

    bool filterEmptyEpg() const;
    void setFilterEmptyEpg(bool newFilterEmptyEpg);

    void onEnsembleInformation(const RadioControlEnsemble & ens);
    bool filterEnsemble() const;
    void setFilterEnsemble(bool newFilterEnsemble);

    void setupDarkMode(bool darkModeEna);
    QList<QColor> colors() const;
    void setColors(const QList<QColor> &newColors);

    Q_INVOKABLE void scheduleRecording();

signals:
    void selectedEpgItemChanged();

    void isVisibleChanged();

    void filterEmptyEpgChanged();

    void filterEnsembleChanged();

    void colorsChanged();

    void scheduleAudioRecording(const AudioRecScheduleItem & item);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::EPGDialog *ui;
    QQuickView *m_qmlView;
    Settings * m_settings;

    MetadataManager * m_metadataManager;
    bool m_isVisible = false;
    SLProxyModel * m_slProxyModel;
    int m_currentUEID;
    bool m_isDarkMode;
    QList<QColor> m_colors;
};


// this class serves as simple image provider for EPG QML using MatedataManager as backend
class LogoProvider : public QQuickImageProvider
{
public:
    explicit LogoProvider(MetadataManager * metadataManager) : QQuickImageProvider(QQuickImageProvider::Pixmap)
        , m_metadataManager(metadataManager) {};

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        QPixmap logo = m_metadataManager->data(ServiceListId(static_cast<uint64_t>(id.toUInt())),
                                               MetadataManager::MetadataRole::SmallLogo).value<QPixmap>();
        if (logo.isNull())
        {
            logo = QPixmap(requestedSize.width() > 0 ? requestedSize.width() : 32,
                           requestedSize.height() > 0 ? requestedSize.height() : 32);
            logo.fill(QColor(0,0,0,0));
        }

        if (size)
        {
            *size = QSize(logo.width(), logo.height());
        }

        return logo;
    }

private:
    MetadataManager * m_metadataManager;
};


#endif // EPGDIALOG_H
