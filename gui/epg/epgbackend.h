/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#ifndef EPGBACKEND_H
#define EPGBACKEND_H

#include <QItemSelectionModel>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>

#include "audiorecscheduleitem.h"
#include "epgtime.h"
#include "metadatamanager.h"
#include "settings.h"
#include "slmodel.h"
#include "slproxymodel.h"
#include "uicontrolprovider.h"

class EPGBackend : public UIControlProvider
{
    Q_OBJECT
    Q_PROPERTY(SLProxyModel *slProxyModel READ slProxyModel CONSTANT FINAL)
    Q_PROPERTY(MetadataManager *metadataManager READ metadataManager CONSTANT FINAL)
    Q_PROPERTY(EPGTime *epgTime READ epgTime CONSTANT FINAL)
    Q_PROPERTY(QItemSelectionModel *slSelectionModel READ slSelectionModel CONSTANT FINAL)
    Q_PROPERTY(QPersistentModelIndex selectedEpgItem READ selectedEpgItem WRITE setSelectedEpgItem NOTIFY selectedEpgItemChanged FINAL)
    Q_PROPERTY(bool filterEmptyEpg READ filterEmptyEpg WRITE setFilterEmptyEpg NOTIFY filterEmptyEpgChanged FINAL)
    Q_PROPERTY(bool filterEnsemble READ filterEnsemble WRITE setFilterEnsemble NOTIFY filterEnsembleChanged FINAL)
    UI_PROPERTY_SETTINGS(QVariant, splitterState, m_settings->epg.splitterState)

public:
    explicit EPGBackend(SLModel *serviceListModel, QItemSelectionModel *slSelectionModel, MetadataManager *metadataManager, Settings *settings,
                        QObject *parent = nullptr);
    ~EPGBackend();

    QPersistentModelIndex selectedEpgItem() const;
    void setSelectedEpgItem(const QPersistentModelIndex &newSelectedEpgItem);

    bool filterEmptyEpg() const;
    void setFilterEmptyEpg(bool newFilterEmptyEpg);

    void onEnsembleInformation(const RadioControlEnsemble &ens);
    bool filterEnsemble() const;
    void setFilterEnsemble(bool newFilterEnsemble);
    Q_INVOKABLE void scheduleRecording();

    SLProxyModel *slProxyModel() const { return m_slProxyModel; }
    EPGTime *epgTime() const { return EPGTime::getInstance(); }
    MetadataManager *metadataManager() const { return m_metadataManager; }
    QItemSelectionModel *slSelectionModel() const { return m_slSelectionModel; }

signals:
    void selectedEpgItemChanged();
    void filterEmptyEpgChanged();
    void filterEnsembleChanged();
    void scheduleAudioRecording(const AudioRecScheduleItem &item);

protected:
    // void showEvent(QShowEvent *event) override;
    // void closeEvent(QCloseEvent *event) override;

private:
    Settings *m_settings;

    MetadataManager *m_metadataManager = nullptr;
    bool m_isVisible = false;
    SLProxyModel *m_slProxyModel = nullptr;
    int m_currentUEID;
    bool m_isDarkMode;
    QList<QColor> m_colors;
    QItemSelectionModel *m_slSelectionModel = nullptr;
};

#endif  // EPGBACKEND_H
