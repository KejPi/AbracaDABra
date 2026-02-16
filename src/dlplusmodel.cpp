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

#include "dlplusmodel.h"

DLPlusModel::DLPlusModel(QObject *parent) : QAbstractListModel{parent}
{
    connect(this, &QAbstractListModel::rowsInserted, this, &DLPlusModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &DLPlusModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &DLPlusModel::rowCountChanged);
}

DLPlusModel::~DLPlusModel()
{
    qDeleteAll(m_modelData);
}

QVariant DLPlusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
    {
        return QVariant();
    }

    switch (role)
    {
        case Roles::TagRole:
            return QVariant(m_modelData.at(index.row())->tagLabel());
        case Roles::ContentRole:
            return QVariant(m_modelData.at(index.row())->tagText());
        case Roles::ToolTipRole:
            return QVariant(m_modelData.at(index.row())->tagTooltip());
        case Roles::IsVisibleRole:
            return QVariant(m_modelData.at(index.row())->isVisible());
        default:
            break;
    }
    return QVariant();
}

QHash<int, QByteArray> DLPlusModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Roles::TagRole] = "tag";
    roles[Roles::ContentRole] = "content";
    roles[Roles::ToolTipRole] = "tooltip";
    roles[Roles::IsVisibleRole] = "isVisible";
    return roles;
}

void DLPlusModel::setDlPlusObject(const DLPlusObject &object)
{
    auto type = object.getType();
    if (object.isDelete())
    {  // [ETSI TS 102 980 V2.1.2] 5.3.2 Deleting objects
        // Objects are deleted by transmitting a "delete" object
        for (int row = 0; row < m_modelData.count(); ++row)
        {
            if (m_modelData.at(row)->dlPlusObject().getType() == type)
            {
                // found -> delete
                beginRemoveRows(QModelIndex(), row, row);
                delete m_modelData.at(row);
                m_modelData.removeAt(row);
                endRemoveRows();
                return;
            }
        }
        // not found -> do nothing
        return;
    }
    else
    {
        for (int row = 0; row < m_modelData.count(); ++row)
        {
            if (m_modelData.at(row)->dlPlusObject().getType() == type)
            {  // found -> update
                m_modelData.at(row)->update(object);
                emit dataChanged(index(row, 0), index(row, 0), {Roles::ContentRole});
                return;
            }
        }
        // not found -> insert at correct position
        int row = 0;
        while (row < m_modelData.count())
        {
            if (m_modelData.at(row)->dlPlusObject().getType() > type)
            {
                break;
            }
            ++row;
        }

        auto *item = new DLPlusModelItem(object);
        if (item->isValid())
        {
            beginInsertRows(QModelIndex(), row, row);
            m_modelData.insert(row, item);
            endInsertRows();
        }
        else
        {
            delete item;
        }
    }
}

void DLPlusModel::itemsRunning(bool isRunning)
{
    for (int row = 0; row < m_modelData.count(); ++row)
    {
        if (m_modelData.at(row)->dlPlusObject().getType() < DLPlusContentType::INFO_NEWS)
        {
            if (m_modelData.at(row)->isVisible() != isRunning)
            {
                m_modelData.at(row)->setIsVisible(isRunning);
                emit dataChanged(index(row, 0), index(row, 0), {Roles::IsVisibleRole});
            }
        }
        else
        {  // no more items of ITEM type
            return;
        }
    }
}

void DLPlusModel::itemsToggle()
{
    // delete all ITEMS.*
    auto it = m_modelData.cbegin();
    while (it != m_modelData.cend())
    {
        if ((*it)->dlPlusObject().getType() < DLPlusContentType::INFO_NEWS)
        {
            beginRemoveRows(QModelIndex(), 0, 0);
            auto item = *it;
            it = m_modelData.erase(it);
            delete item;
            endRemoveRows();
        }
        else
        {  // no more items of ITEM type
            return;
        }
    }

    for (int row = 0; row < m_modelData.count(); ++row)
    {
        if (m_modelData.at(row)->dlPlusObject().getType() < DLPlusContentType::INFO_NEWS)
        {
            beginRemoveRows(QModelIndex(), row, row);
            delete m_modelData.at(row);
            m_modelData.removeAt(row);
            endRemoveRows();
        }
        else
        {  // no more items of ITEM type
            return;
        }
    }
}

void DLPlusModel::reset()
{
    beginResetModel();
    qDeleteAll(m_modelData);
    m_modelData.clear();
    endResetModel();
}

DLPlusModelItem::DLPlusModelItem(const DLPlusObject &obj) : m_dlPlusObject(obj)
{
    m_tagLabel = typeToLabel(obj.getType());
    if (m_tagLabel.isEmpty())
    {  // do nothing -> objects that we do not display: Descriptors, PROGRAMME_FREQUENCY, INFO_DATE_TIME, PROGRAMME_SUBCHANNEL
        return;
    }
    else
    {
        switch (obj.getType())
        {
            case DLPlusContentType::PROGRAMME_HOMEPAGE:
            case DLPlusContentType::INFO_URL:
                m_tagText = QString("<a href=\"%1\">%1</a>").arg(obj.getTag());
                m_tagToolTip = QString(QObject::tr("Open link"));
                break;
            case DLPlusContentType::EMAIL_HOTLINE:
            case DLPlusContentType::EMAIL_STUDIO:
            case DLPlusContentType::EMAIL_OTHER:
                m_tagText = QString("<a href=\"mailto:%1\">%1</a>").arg(obj.getTag());
                m_tagToolTip = QString(QObject::tr("Open link"));
                break;
            default:
                m_tagText = obj.getTag();
        }
    }
}

void DLPlusModelItem::update(const DLPlusObject &obj)
{
    if (obj != m_dlPlusObject)
    {
        m_dlPlusObject = obj;
        switch (obj.getType())
        {
            case DLPlusContentType::PROGRAMME_HOMEPAGE:
            case DLPlusContentType::INFO_URL:
            {
                m_tagText = QString("<a href=\"%1\">%1</a>").arg(obj.getTag());
            }
            break;
            case DLPlusContentType::EMAIL_HOTLINE:
            case DLPlusContentType::EMAIL_STUDIO:
            case DLPlusContentType::EMAIL_OTHER:
            {
                m_tagText = QString("<a href=\"mailto:%1\">%1</a>").arg(obj.getTag());
            }
            break;
            default:
                m_tagText = obj.getTag();
        }
    }
}

void DLPlusModelItem::setIsVisible(bool visible)
{
    m_isVisible = visible;
}

const DLPlusObject &DLPlusModelItem::dlPlusObject() const
{
    return m_dlPlusObject;
}

bool DLPlusModelItem::isVisible() const
{
    return m_isVisible;
}

const QString &DLPlusModelItem::tagLabel() const
{
    return m_tagLabel;
}

const QString &DLPlusModelItem::tagText() const
{
    return m_tagText;
}

const QString &DLPlusModelItem::tagTooltip() const
{
    return m_tagToolTip;
}

QString DLPlusModelItem::typeToLabel(DLPlusContentType type) const
{
    QString label;

    switch (type)
    {
        // case DLPlusContentType::DUMMY: label = "DUMMY"; break;
        case DLPlusContentType::ITEM_TITLE:
            label = QObject::tr("Title");
            break;
        case DLPlusContentType::ITEM_ALBUM:
            label = QObject::tr("Album");
            break;
        case DLPlusContentType::ITEM_TRACKNUMBER:
            label = QObject::tr("Track Number");
            break;
        case DLPlusContentType::ITEM_ARTIST:
            label = QObject::tr("Artist");
            break;
        case DLPlusContentType::ITEM_COMPOSITION:
            label = QObject::tr("Composition");
            break;
        case DLPlusContentType::ITEM_MOVEMENT:
            label = QObject::tr("Movement");
            break;
        case DLPlusContentType::ITEM_CONDUCTOR:
            label = QObject::tr("Conductor");
            break;
        case DLPlusContentType::ITEM_COMPOSER:
            label = QObject::tr("Composer");
            break;
        case DLPlusContentType::ITEM_BAND:
            label = QObject::tr("Band");
            break;
        case DLPlusContentType::ITEM_COMMENT:
            label = QObject::tr("Comment");
            break;
        case DLPlusContentType::ITEM_GENRE:
            label = QObject::tr("Genre");
            break;
        case DLPlusContentType::INFO_NEWS:
            label = QObject::tr("News");
            break;
        case DLPlusContentType::INFO_NEWS_LOCAL:
            label = QObject::tr("News (local)");
            break;
        case DLPlusContentType::INFO_STOCKMARKET:
            label = QObject::tr("Stock Market");
            break;
        case DLPlusContentType::INFO_SPORT:
            label = QObject::tr("Sport");
            break;
        case DLPlusContentType::INFO_LOTTERY:
            label = QObject::tr("Lottery");
            break;
        case DLPlusContentType::INFO_HOROSCOPE:
            label = QObject::tr("Horoscope");
            break;
        case DLPlusContentType::INFO_DAILY_DIVERSION:
            label = QObject::tr("Daily Diversion");
            break;
        case DLPlusContentType::INFO_HEALTH:
            label = QObject::tr("Health");
            break;
        case DLPlusContentType::INFO_EVENT:
            label = QObject::tr("Event");
            break;
        case DLPlusContentType::INFO_SCENE:
            label = QObject::tr("Scene");
            break;
        case DLPlusContentType::INFO_CINEMA:
            label = QObject::tr("Cinema");
            break;
        case DLPlusContentType::INFO_TV:
            label = QObject::tr("TV");
            break;
            // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
            // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
            // case DLPlusContentType::INFO_DATE_TIME: label = "INFO_DATE_TIME"; break;
        case DLPlusContentType::INFO_WEATHER:
            label = QObject::tr("Weather");
            break;
        case DLPlusContentType::INFO_TRAFFIC:
            label = QObject::tr("Traffic");
            break;
        case DLPlusContentType::INFO_ALARM:
            label = QObject::tr("Alarm");
            break;
        case DLPlusContentType::INFO_ADVERTISEMENT:
            label = QObject::tr("Advertisment");
            break;
        case DLPlusContentType::INFO_URL:
            label = QObject::tr("URL");
            break;
        case DLPlusContentType::INFO_OTHER:
            label = QObject::tr("Other");
            break;
        case DLPlusContentType::STATIONNAME_SHORT:
            label = QObject::tr("Station (short)");
            break;
        case DLPlusContentType::STATIONNAME_LONG:
            label = QObject::tr("Station");
            break;
        case DLPlusContentType::PROGRAMME_NOW:
            label = QObject::tr("Now");
            break;
        case DLPlusContentType::PROGRAMME_NEXT:
            label = QObject::tr("Next");
            break;
        case DLPlusContentType::PROGRAMME_PART:
            label = QObject::tr("Programme Part");
            break;
        case DLPlusContentType::PROGRAMME_HOST:
            label = QObject::tr("Host");
            break;
        case DLPlusContentType::PROGRAMME_EDITORIAL_STAFF:
            label = QObject::tr("Editorial");
            break;
            // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
            // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
            // case DLPlusContentType::PROGRAMME_FREQUENCY: label = "Frequency"; break;
        case DLPlusContentType::PROGRAMME_HOMEPAGE:
            label = QObject::tr("Homepage");
            break;
            // [ETSI TS 102 980 V2.1.2] Annex A (normative): List of DL Plus content types
            // NOTE 6 : Intended for RT+ receivers; DL Plus equipped receivers ignore this content type.
            // case DLPlusContentType::PROGRAMME_SUBCHANNEL: label = "Subchannel"; break;
        case DLPlusContentType::PHONE_HOTLINE:
            label = QObject::tr("Phone (Hotline)");
            break;
        case DLPlusContentType::PHONE_STUDIO:
            label = QObject::tr("Phone (Studio)");
            break;
        case DLPlusContentType::PHONE_OTHER:
            label = QObject::tr("Phone (Other)");
            break;
        case DLPlusContentType::SMS_STUDIO:
            label = QObject::tr("SMS (Studio)");
            break;
        case DLPlusContentType::SMS_OTHER:
            label = QObject::tr("SMS (Other)");
            break;
        case DLPlusContentType::EMAIL_HOTLINE:
            label = QObject::tr("E-mail (Hotline)");
            break;
        case DLPlusContentType::EMAIL_STUDIO:
            label = QObject::tr("E-mail (Studio)");
            break;
        case DLPlusContentType::EMAIL_OTHER:
            label = QObject::tr("E-mail (Other)");
            break;
        case DLPlusContentType::MMS_OTHER:
            label = QObject::tr("MMS");
            break;
        case DLPlusContentType::CHAT:
            label = QObject::tr("Chat Message");
            break;
        case DLPlusContentType::CHAT_CENTER:
            label = QObject::tr("Chat");
            break;
        case DLPlusContentType::VOTE_QUESTION:
            label = QObject::tr("Vote Question");
            break;
        case DLPlusContentType::VOTE_CENTRE:
            label = QObject::tr("Vote Here");
            break;
            // rfu = 54
            // rfu = 55
        case DLPlusContentType::PRIVATE_1:
            label = QObject::tr("Private 1");
            break;
        case DLPlusContentType::PRIVATE_2:
            label = QObject::tr("Private 2");
            break;
        case DLPlusContentType::PRIVATE_3:
            label = QObject::tr("Private 3");
            break;
            //    case DLPlusContentType::DESCRIPTOR_PLACE: label = "DESCRIPTOR_PLACE"; break;
            //    case DLPlusContentType::DESCRIPTOR_APPOINTMENT: label = "DESCRIPTOR_APPOINTMENT"; break;
            //    case DLPlusContentType::DESCRIPTOR_IDENTIFIER: label = "DESCRIPTOR_IDENTIFIER"; break;
            //    case DLPlusContentType::DESCRIPTOR_PURCHASE: label = "DESCRIPTOR_PURCHASE"; break;
            //    case DLPlusContentType::DESCRIPTOR_GET_DATA: label = "DESCRIPTOR_GET_DATA"; break;
        default:
            break;
    }

    return label;
}
