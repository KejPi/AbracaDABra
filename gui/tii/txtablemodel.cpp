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

#include "txtablemodel.h"

#include <QColor>
#include <QFile>
#include <QFont>

#include "txdataloader.h"
#include "txlocallist.h"

TxTableModel::TxTableModel(QObject *parent) : QAbstractTableModel{parent}
{
    TxDataLoader::loadTable(m_txList);

    connect(this, &QAbstractListModel::rowsInserted, this, &TxTableModel::rowCountChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &TxTableModel::rowCountChanged);
    connect(this, &QAbstractListModel::modelReset, this, &TxTableModel::rowCountChanged);
}

TxTableModel::~TxTableModel()
{
    qDeleteAll(m_txList);

    if (m_localTxList != nullptr)
    {
        delete m_localTxList;
    }
}

int TxTableModel::rowCount(const QModelIndex &parent) const
{
    return m_modelData.count();
}

int TxTableModel::columnCount(const QModelIndex &parent) const
{
    return NumCols;
}

int TxTableModel::activeCount() const
{
    int ret = 0;
    for (const auto &item : m_modelData)
    {
        ret += (item.isActive());
    }
    return ret;
}

QVariant TxTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (index.row() >= m_modelData.size() || index.row() < 0)
    {
        return QVariant();
    }

    const auto &item = m_modelData.at(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
        {
            switch (index.column())
            {
                case ColTime:
                    if (m_displayTimeInUTC)
                    {
                        return item.rxTime().toUTC().toString("yy-MM-dd hh:mm:ss");
                    }
                    else
                    {
                        return item.rxTime().toString("yy-MM-dd hh:mm:ss");
                    }
                case ColChannel:
                    return DabTables::channelList.value(item.ensId().freq(), 0);
                case ColFreq:
                    return QString("%1 kHz").arg(item.ensId().freq());  // QString::number(item.ensId().freq());
                case ColEnsId:
                    return QString("%1").arg(item.ensId().ueid(), 6, 16, QChar(' ')).toUpper();
                case ColEnsLabel:
                    return item.ensLabel();
                case ColNumServices:
                    return item.numServices();
                case ColSnr:
                    return QString("%1 dB").arg(static_cast<double>(item.snr()), 0, 'f', 1);
                case ColMainId:
                    return (item.mainId() != -1) ? QString::number(item.mainId()) : "";
                case ColSubId:
                    return (item.subId() != -1) ? QString::number(item.subId()) : "";
                case ColLevel:
                    if (item.isActive() == false)
                    {
                        return QString();
                    }
                    return QString("%1 dB").arg(static_cast<double>(item.level()), 5, 'f',
                                                1);  // QString::number(static_cast<double>(item.level()), 'f', 3);
                case ColLocation:
                    return item.hasTxData() ? item.transmitterData().location() : "";
                case ColDist:
                    if (item.hasTxData() && item.distance() >= 0.0)
                    {
                        return QString("%1 km").arg(static_cast<double>(item.distance()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColAzimuth:
                    if (item.hasTxData() && item.azimuth() >= 0.0)
                    {
                        return QString("%1°").arg(static_cast<double>(item.azimuth()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColPower:
                    if (item.hasTxData() && item.power() > 0.0)
                    {
                        return QString("%1 kW").arg(static_cast<double>(item.power()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColRxCoordinatesLat:
                    return QString("%1").arg(static_cast<double>(m_coordinates.latitude()), 0, 'f');
                case ColTxCoordinatesLat:
                    if (item.hasTxData())
                    {
                        return QString("%1").arg(static_cast<double>(item.transmitterData().coordinates().latitude()), 0, 'f');
                    }
                    return QVariant("");
                case ColRxCoordinatesLon:
                    return QString("%1").arg(static_cast<double>(m_coordinates.longitude()), 0, 'f');
                case ColTxCoordinatesLon:
                    if (item.hasTxData())
                    {
                        return QString("%1").arg(static_cast<double>(item.transmitterData().coordinates().longitude()), 0, 'f');
                    }
                    return QVariant("");
            }
        }
        break;
        case Qt::FontRole:
        {
            QFont font;
            if (m_localTxList != nullptr)
            {
                font.setItalic(m_localTxList->get(item.ensId(), item.id()));
            }
            return QVariant(font);
        }
        break;
        case TxTableModelRoles::ExportRole:
        case TxTableModelRoles::ExportRoleUTC:
        {
            switch (index.column())
            {
                case ColTime:
                    if (role == TxTableModelRoles::ExportRoleUTC)
                    {
                        return item.rxTime().toUTC().toString("yy-MM-dd hh:mm:ss");
                    }
                    else
                    {
                        return item.rxTime().toString("yy-MM-dd hh:mm:ss");
                    }
                case ColChannel:
                    return DabTables::channelList.value(item.ensId().freq(), 0);
                case ColFreq:
                    return QString::number(item.ensId().freq());
                case ColEnsId:
                    return QString("%1").arg(item.ensId().ueid(), 6, 16, QChar(' ')).toUpper();
                case ColEnsLabel:
                    return item.ensLabel();
                case ColNumServices:
                    return item.numServices();
                case ColSnr:
                    return QString("%1").arg(static_cast<double>(item.snr()), 0, 'f', 1);
                case ColMainId:
                    return (item.mainId() != -1) ? QString::number(item.mainId()) : "";
                case ColSubId:
                    return (item.subId() != -1) ? QString::number(item.subId()) : "";
                case ColLevel:
                    return QString("%1").arg(static_cast<double>(item.level()), 5, 'f', 1);
                case ColLocation:
                    return item.transmitterData().location();
                case ColDist:
                    if (item.hasTxData() && item.distance() >= 0.0)
                    {
                        return QString("%1").arg(static_cast<double>(item.distance()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColAzimuth:
                    if (item.hasTxData() && item.azimuth() >= 0.0)
                    {
                        return QString("%1").arg(static_cast<double>(item.azimuth()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColPower:
                    if (item.hasTxData() && item.power() > 0)
                    {
                        return QString("%1").arg(static_cast<double>(item.power()), 0, 'f', 1);
                    }
                    return QVariant("");
                case ColRxCoordinatesLat:
                    return QString("%1").arg(static_cast<double>(m_coordinates.latitude()), 0, 'f');
                case ColTxCoordinatesLat:
                    if (item.hasTxData())
                    {
                        return QString("%1").arg(static_cast<double>(item.transmitterData().coordinates().latitude()), 0, 'f');
                    }
                    return QVariant("");
                case ColRxCoordinatesLon:
                    return QString("%1").arg(static_cast<double>(m_coordinates.longitude()), 0, 'f');
                case ColTxCoordinatesLon:
                    if (item.hasTxData())
                    {
                        return QString("%1").arg(static_cast<double>(item.transmitterData().coordinates().longitude()), 0, 'f');
                    }
                    return QVariant("");
            }
        }
        break;
        case TxTableModelRoles::CoordinatesRole:
            return QVariant().fromValue(item.transmitterData().coordinates());
        case TxTableModelRoles::MainIdRole:
            return item.mainId();
        case TxTableModelRoles::SubIdRole:
            return item.subId();
        case TxTableModelRoles::TiiRole:
            return (item.mainId() != -1) ? QVariant(QString("%1-%2").arg(item.mainId()).arg(item.subId())) : "";
        case TxTableModelRoles::LevelColorRole:
            if (item.isActive() == false)
            {
                return QVariant(QColor(Qt::gray));
            }
            if (item.level() > -6)
            {
                // return QVariant(QColor(0x5b, 0xc2, 0x14));
                return QVariant(QColor(Qt::green));
            }
            if (item.level() > -12)
            {
                return QVariant(QColor(0xff, 0xb5, 0x27));
            }
            return QVariant(QColor(0xff, 0x4b, 0x4b));
        case TxTableModelRoles::ItemRole:
            return QVariant().fromValue(item);
        case TxTableModelRoles::IdRole:
            return QVariant(item.id());
        case TxTableModelRoles::SelectedTxRole:
            return m_selectedRows.contains(index.row());
        case TxTableModelRoles::IsActiveRole:
            return QVariant(item.isActive());
        case TxTableModelRoles::IsLocalRole:
            if (m_localTxList != nullptr)
            {
                return QVariant(m_localTxList->get(item.ensId(), item.id()));
            }
            return false;
        default:
            break;
    }

    return QVariant();
}

QVariant TxTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
    {
        return QVariant();
    }

    switch (role)
    {
        case Qt::DisplayRole:
        {
            switch (section)
            {
                case ColTime:
                    if (m_displayTimeInUTC)
                    {
                        return QString(tr("Time (UTC)"));
                    }
                    else
                    {
                        return QString(tr("Time"));
                    }
                case ColChannel:
                    return tr("Channel");
                case ColFreq:
                    return tr("Frequency");
                case ColEnsId:
                    return tr("UEID");
                case ColEnsLabel:
                    return tr("Label");
                case ColNumServices:
                    return tr("Services");
                case ColSnr:
                    return tr("SNR");
                case ColMainId:
                    return tr("Main");
                case ColSubId:
                    return tr("Sub");
                case ColLevel:
                    return tr("Level");
                case ColLocation:
                    return tr("Location");
                case ColPower:
                    return tr("Power");
                case ColDist:
                    return tr("Distance");
                case ColAzimuth:
                    return tr("Azimuth");
                default:
                    break;
            }
        }
        break;
        case TxTableModelRoles::ExportRole:
        case TxTableModelRoles::ExportRoleUTC:
        {
            switch (section)
            {
                case ColTime:
                    if (role == TxTableModelRoles::ExportRoleUTC)
                    {
                        return QString(tr("Time") + " (UTC)");
                    }
                    else
                    {
                        return QString(tr("Time")) + QString(" (%1)").arg(QDateTime::currentDateTime().timeZoneAbbreviation());
                    }
                case ColChannel:
                    return tr("Channel");
                case ColFreq:
                    return tr("Frequency [kHz]");
                case ColEnsId:
                    return tr("UEID");
                case ColEnsLabel:
                    return tr("Label");
                case ColNumServices:
                    return tr("Services");
                case ColSnr:
                    return tr("SNR [dB]");
                case ColMainId:
                    return tr("Main");
                case ColSubId:
                    return tr("Sub");
                case ColLevel:
                    return tr("Level [dB]");
                case ColLocation:
                    return tr("Location");
                case ColPower:
                    return tr("Power [kW]");
                case ColDist:
                    return tr("Distance [km]");
                case ColAzimuth:
                    return tr("Azimuth [deg]");
                case ColTxCoordinatesLat:
                    return tr("Latitude (TX)");
                case ColTxCoordinatesLon:
                    return tr("Longitude (TX)");
                case ColRxCoordinatesLat:
                    return tr("Latitude (RX)");
                case ColRxCoordinatesLon:
                    return tr("Longitude (RX)");
                default:
                    break;
            }
        }
        break;
    }
    return QVariant();
}

QHash<int, QByteArray> TxTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[Qt::DisplayRole] = "display";
    roles[TxTableModelRoles::CoordinatesRole] = "coordinates";
    roles[TxTableModelRoles::TiiRole] = "tiiString";
    roles[TxTableModelRoles::MainIdRole] = "mainId";
    roles[TxTableModelRoles::SubIdRole] = "subId";
    roles[TxTableModelRoles::LevelColorRole] = "levelColor";
    roles[TxTableModelRoles::SelectedTxRole] = "selectedTx";
    roles[TxTableModelRoles::IsActiveRole] = "isActive";
    roles[TxTableModelRoles::IsLocalRole] = "isLocal";

    return roles;
}

const TxTableModelItem &TxTableModel::itemAt(int row) const
{
    Q_ASSERT(row >= 0 && row < m_modelData.size());

    return m_modelData.at(row);
}

void TxTableModel::clear()
{
    beginResetModel();
    m_modelData.clear();
    m_selectedRows.clear();
    endResetModel();
}

void TxTableModel::reloadTxTable()
{
    qDeleteAll(m_txList);
    m_txList.clear();
    TxDataLoader::loadTable(m_txList);
}

void TxTableModel::setSelectedRows(const QSet<int> &rows)
{
    if (m_selectedRows != rows)
    {
        m_selectedRows = rows;
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {TxTableModelRoles::SelectedTxRole});
    }
}

void TxTableModel::updateTiiData(const QList<dabsdrTii_t> &data, const ServiceListId &ensId, const QString &ensLabel, int numServices, float snr)
{
    QDateTime time = QDateTime::currentDateTime();

    // add new items and remove old
    int row = 0;
    QList<TxTableModelItem> appendList;
    for (int dataIdx = 0; dataIdx < data.count(); ++dataIdx)
    {
        // create new item
        TxTableModelItem item(data.at(dataIdx).main, data.at(dataIdx).sub, data.at(dataIdx).level, m_coordinates, m_txList.values(ensId));
        item.setEnsData(ensId, ensLabel, numServices, snr);
        item.setRxTime(time);

        if (row < m_modelData.size())
        {
            while ((row < m_modelData.size()) && (m_modelData.at(row).id() < item.id()))
            {  // inactivate items
                if (m_modelData.at(row).isActive() == true)
                {
                    m_modelData[row].setInactive();
                    emit dataChanged(index(row, ColMainId), index(row, ColAzimuth));
                }
                row += 1;
            }

            if (row < m_modelData.size())
            {
                if (m_modelData.at(row).id() == item.id())
                {  // equal ID ==> update item
                    m_modelData[row] = item;
                    // inform views
                    emit dataChanged(index(row, ColMainId), index(row, ColAzimuth));
                }
                else
                {  // insert item
                    beginInsertRows(QModelIndex(), row, row);
                    m_modelData.insert(row, item);
                    endInsertRows();
                }
                row += 1;
            }
            else
            {  // we are at the end of m_model data -> insert remaining items
                appendList.append(item);
            }
        }
        else
        {  // we are at the end of m_model data -> insert remaining items
            appendList.append(item);
        }
    }

    // final clean up
    if (appendList.isEmpty())
    {  // nothing to append
        while (row < m_modelData.size())
        {
            // qDebug() << "Current item:" << m_modelData.at(row).mainId() << m_modelData.at(row).subId();
            if (m_modelData.at(row).isActive() == true)
            {  // inactivate item
                m_modelData[row].setInactive();
                emit dataChanged(index(row, ColMainId), index(row, ColAzimuth));
            }
            row += 1;
        }
    }
    else
    {  // rows to append
        beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size() + appendList.count() - 1);
        m_modelData.append(appendList);
        endInsertRows();
    }

    // #warning "Remove this check"
    // Q_ASSERT(m_modelData.size() == data.size());
    // for (int n = 0; n < m_modelData.size(); ++n) {
    //     Q_ASSERT(m_modelData.at(n).mainId() == data.at(n).main);
    //     Q_ASSERT(m_modelData.at(n).subId() == data.at(n).sub);
    //     Q_ASSERT(m_modelData.at(n).level() == data.at(n).level);
    // }
}

void TxTableModel::removeInactive(qint64 timeoutSec)
{
    // qDebug() << Q_FUNC_INFO;
    auto currentTime = QDateTime::currentDateTime();
    int row = 0;
    while (row < m_modelData.count())
    {
        if (m_modelData.at(row).isActive() == false && m_modelData.at(row).rxTime().secsTo(currentTime) > timeoutSec)
        {
            // qDebug() << "Removing:" << m_modelData.at(row).mainId() << m_modelData.at(row).subId()
            //          << "sec to current time:" << m_modelData.at(row).rxTime().secsTo(currentTime);
            beginRemoveRows(QModelIndex(), row, row);
            m_modelData.remove(row);
            endRemoveRows();
        }
        else
        {
            row += 1;
        }
    }
}

void TxTableModel::appendEnsData(const QDateTime &time, const QList<dabsdrTii_t> &data, const ServiceListId &ensId, const QString &ensLabel,
                                 const QString &ensConfig, const QString &ensCSV, int numServices, float snr)
{
    if (!data.empty())
    {
        for (auto it = data.cbegin(); it != data.cend(); ++it)
        {
            // create new item
            TxTableModelItem item(it->main, it->sub, it->level, m_coordinates, m_txList.values(ensId));
            item.setEnsData(ensId, ensLabel, numServices, snr);
            item.setEnsConfig(ensConfig, ensCSV);
            item.setRxTime(time);
            beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
            m_modelData.append(item);
            endInsertRows();
        }
    }
    else
    {
        // create new item
        TxTableModelItem item(-1, -1, 0, m_coordinates, m_txList.values(ensId));
        item.setEnsData(ensId, ensLabel, numServices, snr);
        item.setEnsConfig(ensConfig, ensCSV);
        item.setRxTime(time);
        beginInsertRows(QModelIndex(), m_modelData.size(), m_modelData.size());
        m_modelData.append(item);
        endInsertRows();
    }
}

void TxTableModel::setCoordinates(const QGeoCoordinate &newCoordinates)
{
    if (newCoordinates != m_coordinates)
    {
        m_coordinates = newCoordinates;

        int row = 0;
        for (auto &item : m_modelData)
        {
            if (item.hasTxData())
            {
                item.updateGeo(m_coordinates);
                emit dataChanged(index(row, ColDist), index(row, ColAzimuth));
            }
            row += 1;
        }
    }
}

void TxTableModel::setDisplayTimeInUTC(bool newDisplayTimeInUTC)
{
    if (m_displayTimeInUTC != newDisplayTimeInUTC)
    {
        m_displayTimeInUTC = newDisplayTimeInUTC;
        emit headerDataChanged(Qt::Horizontal, ColTime, ColTime);
        emit dataChanged(index(0, ColTime), index(m_modelData.count() - 1, ColTime));
    }
}

void TxTableModel::loadLocalTxList(const QString &filename)
{
    if (m_localTxList != nullptr)
    {
        delete m_localTxList;
    }
    m_localTxList = new TxLocalList(filename);
}

void TxTableModel::setAsLocalTx(const QModelIndex &idx, bool setAsLocal)
{
    if (!idx.isValid() || idx.row() >= m_modelData.size() || idx.row() < 0)
    {
        return;
    }

    Q_ASSERT(m_localTxList != nullptr);

    auto ensId = m_modelData[idx.row()].ensId();
    auto id = m_modelData[idx.row()].id();
    if (m_localTxList->get(ensId, id) != setAsLocal)
    {
        m_localTxList->set(ensId, id, setAsLocal);

        // go through the model and emit data changed signal
        for (int row = 0; row < m_modelData.count(); ++row)
        {
            if (m_modelData.at(row).ensId() == ensId && m_modelData.at(row).id() == id)
            {
                emit dataChanged(index(row, 0), index(row, NumColsWithoutCoordinates - 1), {TxTableModelRoles::IsLocalRole});
            }
        }
    }
}

void TxTableModel::clearLocalTx()
{
    Q_ASSERT(m_localTxList != nullptr);
    m_localTxList->clear();
    emit dataChanged(index(0, 0), index(m_modelData.count() - 1, NumColsWithoutCoordinates - 1), {TxTableModelRoles::IsLocalRole});
}
