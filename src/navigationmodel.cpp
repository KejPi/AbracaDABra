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

#include "navigationmodel.h"

NavigationModel::NavigationModel(QObject *parent) : QAbstractListModel{parent}
{
    // clang-format off
    m_modelData = {
        // id, shortLabel, longLabel, iconName, flags
        new NavigationModelItem(NavigationId::ServiceList, tr("List"), tr("List"), "ServiceList.qml", "icon-list.svg",
                                (NavigationOption::EnabledOption |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::PortraitSmallOption)),
        new NavigationModelItem(NavigationId::Service, tr("Service"), "", "ServiceView.qml", "icon-service.svg",
                                (NavigationOption::EnabledOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOption)),
        new NavigationModelItem(NavigationId::EnsembleInfo, tr("Info"), tr("Ensemble information"), "EnsembleInfoView.qml", "icon-info.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOption)),
        new NavigationModelItem(NavigationId::Epg, tr("EPG"), tr("Electronic Programme Guide"), "epg/EPG.qml", "icon-epg.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOption)),
        new NavigationModelItem(
            NavigationId::CatSls, tr("CatSLS"), tr("Categorized slideshow"), "CatSLS.qml", "icon-cat-sls.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOthersOption |
                                    NavigationOption::PortraitSmallOthersOption)),
        new NavigationModelItem(NavigationId::Tii, tr("TII"), tr("TII"), "tii/TII.qml", "icon-tii.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOption)),
        new NavigationModelItem(NavigationId::Scanner, tr("Scanner"), tr("DAB Scanner"), "tii/Scanner.qml", "icon-scanner.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOthersOption |
                                    NavigationOption::PortraitSmallOthersOption)),
        new NavigationModelItem(NavigationId::DabSignal, tr("Signal"), tr("DAB Signal Overview"), "DabSignal.qml", "icon-signal-overview.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape1Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOthersOption)),
        new NavigationModelItem(
            NavigationId::Settings, tr("Settings"), tr("Settings"), "settings/Settings.qml", "icon-settings.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption |
                                    NavigationOption::Landscape2Option |
                                    NavigationOption::PortraitOthersOption |
                                    NavigationOption::PortraitSmallOthersOption |
                                    NavigationOption::LandscapeSmallOthersOption)),
        new NavigationModelItem(NavigationId::ShowMore, tr("Others"), tr("Others"), "", "icon-others.svg",
                                (NavigationOption::EnabledOption |
                                    NavigationOption::Landscape2Option |
                                    NavigationOption::PortraitOption |
                                    NavigationOption::LandscapeSmallOption |
                                    NavigationOption::PortraitSmallOption)),
#ifndef Q_OS_ANDROID
        new NavigationModelItem(NavigationId::AudioRecordingSchedule, tr("Audio recording schedule"), tr("Audio recording schedule..."), "audiorec/AudioRecording.qml", "icon-rec-schedule.svg",
                                    (NavigationOption::EnabledOption |
                                    NavigationOption::OthersOption)),
#endif
        new NavigationModelItem(NavigationId::AudioRecording, tr("Start audio recording"), tr("Start audio recording"), "", "icon-rec-start.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),

        new NavigationModelItem(NavigationId::Undefined, "", "", "", "",
                                (NavigationOption::SeparatorOption | NavigationOption::LandscapeOthersOption)),

        new NavigationModelItem(NavigationId::BandScan, tr("Band scan"), tr("Band scan..."), "", "icon-bandscan.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),
        new NavigationModelItem(NavigationId::ExportServiceList, tr("Export service list"), tr("Export service list"), "", "icon-list-export.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),
        new NavigationModelItem(NavigationId::ClearServiceList, tr("Clear service list"), tr("Clear service list"), "", "icon-list-clear.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),

        new NavigationModelItem(NavigationId::Undefined, "", "", "", "",
                                (NavigationOption::SeparatorOption | NavigationOption::LandscapeOthersOption)),

        new NavigationModelItem(NavigationId::AppLog, tr("Log"), tr("Application log"), "AppLog.qml", "icon-log.svg",
                                (NavigationOption::EnabledOption | NavigationOption::UndockableOption | NavigationOption::OthersOption)),
        new NavigationModelItem(NavigationId::About, tr("About"), tr("About AbracaDABra"), "About.qml", "icon-about.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),

       new NavigationModelItem(NavigationId::Undefined, "", "", "", "",
                                (NavigationOption::SeparatorOption | NavigationOption::LandscapeOthersOption)),

        new NavigationModelItem(NavigationId::Quit, tr("Quit"), tr("Quit AbracaDABra"), "", "icon-off.svg",
                                (NavigationOption::EnabledOption | NavigationOption::OthersOption)),

    };
    // clang-format on
}

NavigationModel::~NavigationModel()
{
    qDeleteAll(m_modelData);
}

QVariant NavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    if (index.row() >= rowCount())
    {
        return QVariant();
    }
    // valid index
    const NavigationModelItem *item = m_modelData.at(index.row());

    switch (static_cast<NavigationModelRoles>(role))
    {
        case FunctionalityIdRole:
            return item->id();
        case ShortLabelRole:
            return item->shortLabel();
        case LongLabelRole:
            return item->longLabel();
        case IconNameRole:
            return item->iconName();
        case IsEnabledRole:
            return item->isEnabled();
        case OptionsRole:
            return item->options().toInt();
        case QmlComponentPathRole:
            return item->qmlComponentPath();
        case ItemRole:
            return QVariant().fromValue(item);
        case IsUndockedRole:
            return item->isUndocked();
        case UndockedWidth:
            return item->undockedWidth();
        case UndockedHeight:
            return item->undockedHeight();
        case UndockedX:
            return item->undockedX();
        case UndockedY:
            return item->undockedY();
        case IsSeparatorRole:
            return item->isSeparator();
        case CanUndockRole:
            return item->isUndockable();
    }
    return QVariant();
}

bool NavigationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    // qDebug() << Q_FUNC_INFO << index << value << role;

    if (index.isValid())
    {
        // valid index
        NavigationModelItem *item = m_modelData.at(index.row());
        bool result = false;

        switch (static_cast<NavigationModelRoles>(role))
        {
            case IsUndockedRole:
            {
                bool undock = value.toBool();
                if (undock == item->isUndocked())
                {
                    break;  // no change
                }
                item->setIsUndocked(undock);
                emit isUndockedChanged(item->id(), item->isUndocked());
                if (m_currentId != item->id())
                {  // active is item that is either undocked or current
                    if (undock)
                    {  // not current but undocked ==> active changed to true
                        emit isActiveChanged(item->id(), true);
                    }
                    else
                    {  // not current and not undocked ==> active changed to false
                        emit isActiveChanged(item->id(), false);
                    }
                }
                result = true;
            }
            break;
            case UndockedWidth:
                item->setUndockedWidth(value.toInt());
                result = true;
                break;
            case UndockedHeight:
                item->setUndockedHeight(value.toInt());
                result = true;
                break;
            case UndockedX:
                item->setUndockedX(value.toInt());
                result = true;
                break;
            case UndockedY:
                item->setUndockedY(value.toInt());
                result = true;
                break;
            case FunctionalityIdRole:
            case ShortLabelRole:
                item->setShortLabel(value.toString());
                result = true;
                break;
            case LongLabelRole:
                item->setLongLabel(value.toString());
                result = true;
                break;
            case IconNameRole:
            case QmlComponentPathRole:
            case IsEnabledRole:
                item->setIsEnabled(value.toBool());
                result = true;
                break;
            case OptionsRole:
            case ItemRole:
            case IsSeparatorRole:
            case CanUndockRole:
                return false;
        }

        if (result)
        {
            emit dataChanged(index, index, {role});
            return true;
        }
    }
    return false;
}

QHash<int, QByteArray> NavigationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NavigationModelRoles::FunctionalityIdRole] = "functionalityId";
    roles[NavigationModelRoles::ShortLabelRole] = "shortLabel";
    roles[NavigationModelRoles::LongLabelRole] = "longLabel";
    roles[NavigationModelRoles::IconNameRole] = "iconName";
    roles[NavigationModelRoles::QmlComponentPathRole] = "qmlComponentPath";
    roles[NavigationModelRoles::IsEnabledRole] = "isEnabled";
    roles[NavigationModelRoles::IsUndockedRole] = "isUndocked";
    roles[NavigationModelRoles::IsSeparatorRole] = "isSeparator";
    roles[NavigationModelRoles::UndockedWidth] = "undockedWidth";
    roles[NavigationModelRoles::UndockedHeight] = "undockedHeight";
    roles[NavigationModelRoles::UndockedX] = "undockedX";
    roles[NavigationModelRoles::UndockedY] = "undockedY";
    roles[NavigationModelRoles::OptionsRole] = "options";
    roles[NavigationModelRoles::CanUndockRole] = "canUndock";
    return roles;
}

NavigationModel::NavigationId NavigationModel::currentId() const
{
    return m_currentId;
}

void NavigationModel::setCurrentId(NavigationId newCurrentId)
{
    if (m_currentId == newCurrentId)
    {
        return;
    }

    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id() == m_currentId)
        {  // found
            if (!m_modelData.at(row)->isUndocked())
            {  // was active, now not
                emit isActiveChanged(m_currentId, false);
            }
        }
        if (m_modelData.at(row)->id() == newCurrentId)
        {  // found
            if (!m_modelData.at(row)->isUndocked())
            {  // now active
                emit isActiveChanged(newCurrentId, true);
            }
        }
    }
    m_currentId = newCurrentId;
    emit currentIdChanged();
}

void NavigationModel::setEnabled(NavigationModel::NavigationId id, bool enabled)
{
    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id() == id)
        {  // found
            m_modelData.at(row)->setIsEnabled(enabled);
            emit dataChanged(index(row, 0), index(row, 0), {NavigationModel::IsEnabledRole, NavigationModel::IsUndockedRole});

            if (enabled == false && currentId() == id)
            {  // reset current id to service view (default)
                setCurrentId(NavigationModel::Service);
            }
        }
    }
}

void NavigationModel::setLabel(NavigationId id, const QString &longLabel, const QString &shortLabel)
{
    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id() == id)
        {  // found
            m_modelData.at(row)->setLongLabel(longLabel);
            m_modelData.at(row)->setShortLabel(shortLabel);
            emit dataChanged(index(row, 0), index(row, 0), {NavigationModel::LongLabelRole, NavigationModel::ShortLabelRole});
        }
    }
}

bool NavigationModel::isActive(NavigationId id) const
{
    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id() == id)
        {
            return m_modelData.at(row)->isUndocked() || id == m_currentId;
        }
    }
    return false;
}

void NavigationModel::alignIdForCurrentView(bool isPortrait)
{
    QFlags<NavigationModel::NavigationOption> mask = isPortrait ? NavigationOption::PortraitAnyOption : NavigationOption::LandscapeAnyOption;
    for (auto it = m_modelData.cbegin(); it != m_modelData.cend(); ++it)
    {
        if ((*it)->id() == m_currentId && ((*it)->options() & mask))
        {  // id is valid in current mode
            return;
        }
    }
    // fallback -> service
    setCurrentId(NavigationId::Service);
}

QVariantMap NavigationModel::getFunctionalityItemData(NavigationModel::NavigationId id) const
{
    for (int row = 0; row < m_modelData.size(); ++row)
    {
        if (m_modelData.at(row)->id() == id)
        {  // found
            QVariantMap map;
            const NavigationModelItem *item = m_modelData.at(row);
            map["functionalityId"] = item->id();
            map["shortLabel"] = item->shortLabel();
            map["longLabel"] = item->longLabel();
            map["iconName"] = item->iconName();
            map["qmlComponentPath"] = item->qmlComponentPath();
            map["isEnabled"] = item->isEnabled();
            map["isUndocked"] = item->isUndocked();
            map["undockedWidth"] = item->undockedWidth();
            map["undockedHeight"] = item->undockedHeight();
            map["undockedX"] = item->undockedX();
            map["undockedY"] = item->undockedY();
            map["options"] = item->options().toInt();
            map["canUndock"] = item->isUndockable();
            return map;
        }
    }
    return QVariantMap();
}

const NavigationModelItem *NavigationModel::getItem(int row) const
{
    if (row < 0 || row >= m_modelData.size())
    {
        return nullptr;
    }
    return m_modelData.at(row);
}

void NavigationModel::loadSettings(QSettings *settings)
{
    for (int i = 0; i < m_modelData.size(); ++i)
    {
        auto item = m_modelData.at(i);
        if (item && item->isUndockable())
        {
            settings->beginGroup(QString("NavigationModel/%1").arg(static_cast<int>(item->id())));
            item->setUndockedX(settings->value("undockedX", -1).toInt());
            item->setUndockedY(settings->value("undockedY", -1).toInt());
            item->setUndockedWidth(settings->value("undockedWidth", -1).toInt());
            item->setUndockedHeight(settings->value("undockedHeight", -1).toInt());
            settings->endGroup();
        }
    }
}

void NavigationModel::saveSettings(QSettings *settings)
{
    for (int i = 0; i < m_modelData.size(); ++i)
    {
        auto item = m_modelData.at(i);
        if (item && item->isUndockable())
        {
            settings->beginGroup(QString("NavigationModel/%1").arg(static_cast<int>(item->id())));
            settings->setValue("undockedX", item->undockedX());
            settings->setValue("undockedY", item->undockedY());
            settings->setValue("undockedWidth", item->undockedWidth());
            settings->setValue("undockedHeight", item->undockedHeight());
            settings->endGroup();
        }
    }
}
