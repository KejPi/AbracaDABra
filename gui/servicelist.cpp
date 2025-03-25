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

#include "servicelist.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(serviceList, "ServiceList", QtInfoMsg)

ServiceList::ServiceList(QObject *parent) : QObject(parent)
{}

ServiceList::~ServiceList()
{
    clear();
}

void ServiceList::clear(bool clearFavorites)
{
    foreach (ServiceListItem *item, m_serviceList)
    {
        delete item;
    }
    m_serviceList.clear();

    foreach (EnsembleListItem *item, m_ensembleList)
    {
        delete item;
    }
    m_ensembleList.clear();

    if (clearFavorites)
    {
        m_favoritesList.clear();
    }

    emit empty();
}

void ServiceList::addService(const RadioControlEnsemble &e, const RadioControlServiceComponent &s, bool fav, int currentEns)
{
    if (!e.isValid())
    {  // invalid ensemble -> do nothing
        return;
    }

    bool newService = false;
    bool updatedService = false;
    bool updatedEnsemble = false;

    qCInfo(serviceList, "          [%6.6X @ %6d kHz | %3s] %-18s %X : %d", e.ueid, e.frequency,
           DabTables::channelList.value(e.frequency).toUtf8().data(), s.label.toUtf8().data(), s.SId.value(), s.SCIdS);

    ServiceListItem *pService = nullptr;
    ServiceListId servId(s);
    ServiceListIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() == sit)
    {  // not found
        pService = new ServiceListItem(s, currentEns);
        m_serviceList.insert(servId, pService);
        if (fav)
        {
            m_favoritesList.insert(servId);
        }
        newService = true;
    }
    else
    {  // found
        pService = *sit;
        updatedService = pService->update(s);
    }

    pService->setIsObsolete(false);

    EnsembleListItem *pEns = nullptr;
    ServiceListId ensId(e);
    uint32_t freq = e.frequency;
    EnsembleListIterator eit = m_ensembleList.begin();
    while (eit != m_ensembleList.end())
    {
        if (freq == (*eit)->frequency())
        {  // frequency is equal to ensemble to be added
            if (e.ueid == (*eit)->ueid())
            {  // ensemble is the same
                pEns = *eit;
            }
            else
            {  // different ensemble in channel -> this is not allowed ensemble to be removed
                // construct dummy RadioControlEnsemble so that removeEnsemble interface can be used
                RadioControlEnsemble dummyEnsToBeRemoved;
                dummyEnsToBeRemoved.frequency = (*eit)->frequency();
                dummyEnsToBeRemoved.ueid = (*eit)->ueid();
                dummyEnsToBeRemoved.label = (*eit)->label();
                dummyEnsToBeRemoved.labelShort = (*eit)->shortLabel();

                removeEnsemble(dummyEnsToBeRemoved);

                pEns = nullptr;
            }
            break;
        }
        ++eit;
    }
    if (nullptr == pEns)
    {  // not found
        pEns = new EnsembleListItem(e);
        m_ensembleList.insert(ensId, pEns);
    }
    else
    {
        // update labels
        updatedEnsemble = pEns->update(e);
    }

    // we have ens and service item => lets link them together
    pService->addEnsemble(pEns);
    if (pEns->addService(pService))
    {  // new service in ensemble
        emit serviceAddedToEnsemble(pEns->id(), pService->id());
    }
    if (newService)
    {  // emit signal when new service is added
        emit serviceAdded(pService->id());
    }

    if (updatedService || updatedEnsemble)
    {
        emit serviceUpdated(pService->id());
        emit serviceUpdatedInEnsemble(pEns->id(), pService->id());
    }
}

void ServiceList::setServiceFavorite(const ServiceListId &servId, bool ena)
{
#if 0
    ServiceListIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {   // found
        (*sit)->setFavorite(ena);
    }
#else
    if (ena)
    {
        if (m_serviceList.find(servId) != m_serviceList.cend())
        {  // if service exists in the list
            m_favoritesList.insert(servId);
        }
    }
    else
    {
        m_favoritesList.remove(servId);
    }
#endif
}

bool ServiceList::isServiceFavorite(const ServiceListId &servId) const
{
#if 0
    ServiceListConstIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {   // found
        return (*sit)->isFavorite();
    }
    return false;
#else
    return m_favoritesList.contains(servId);
#endif
}

int ServiceList::numEnsembles(const ServiceListId &servId) const
{
    if (!servId.isValid())
    {
        return 0;
    }
    else
    {
        ServiceListConstIterator sit = m_serviceList.find(servId);
        if (m_serviceList.end() != sit)
        {  // found
            return (*sit)->numEnsembles();
        }
        return 0;
    }
}

int ServiceList::currentEnsembleIdx(const ServiceListId &servId) const
{
    ServiceListConstIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {  // found
        return (*sit)->currentEnsembleIdx();
    }
    return 0;
}

void ServiceList::save(const QString &filename)
{
    QVariantList list;

    // first sort service list by ID
    // this is needed to restore secondary services correctly
    QVector<uint64_t> idVect;
    for (auto &s : m_serviceList)
    {
        idVect.append(s->id().value());
    }
    std::sort(idVect.begin(), idVect.end());

    int n = 0;
    for (auto id : idVect)
    {
        ServiceListIterator it = m_serviceList.find(ServiceListId(id));

        QVariantMap map;

        map["SID"] = (*it)->SId().value();
        map["SCIdS"] = (*it)->SCIdS();
        map["Label"] = (*it)->label();
        map["ShortLabel"] = (*it)->shortLabel();
        map["Fav"] = m_favoritesList.contains(ServiceListId(id));
        map["LastEns"] = (*it)->currentEnsembleIdx();

        QVariantList ensList;

        // settings.beginWriteArray("Ensemble", (*it)->numEnsembles());
        for (int e = 0; e < (*it)->numEnsembles(); ++e)
        {
            QVariantMap ensMap;
            ensMap["UEID"] = (*it)->getEnsemble(e)->ueid();
            ensMap["Frequency"] = (*it)->getEnsemble(e)->frequency();
            ensMap["Label"] = (*it)->getEnsemble(e)->label();
            ensMap["ShortLabel"] = (*it)->getEnsemble(e)->shortLabel();
            // ensMap["SNR"] = (*it)->getEnsemble(e)->snr();
            ensList.append(ensMap);
        }
        map["Ensembles"] = ensList;
        list.append(map);
    }

    // qDebug() << qPrintable(QJsonDocument::fromVariant(list).toJson());
    QDir().mkpath(QFileInfo(filename).path());
    QFile saveFile(filename);
    if (!saveFile.open(QIODevice::WriteOnly))
    {
        qCWarning(serviceList) << "Failed to save service list to file.";
        return;
    }
    saveFile.write(QJsonDocument::fromVariant(list).toJson());
    saveFile.close();
}

void ServiceList::load(const QString &filename)
{
    QFile loadFile(filename);
    if (!loadFile.exists())
    {  // file does not exist -> do nothing
        return;
    }

    // file exists here
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        qCWarning(serviceList) << "Unable to read service list settings file";
        return;
    }

    QByteArray data = loadFile.readAll();
    loadFile.close();

    if (data.isEmpty())
    {  // no data in the file
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isArray())
    {
        qCWarning(serviceList) << "Unable to read service list settings file";
        return;
    }

    RadioControlServiceComponent item;
    RadioControlEnsemble ens;

    int numUniqueServices = 0;
    int numServices = 0;

    // doc is not null and is array
    QVariantList list = doc.toVariant().toList();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {  // loading the list
        auto serviceMap = it->toMap();

        bool ok = true;

        // filling only what is necessary
        item.SId.set(serviceMap.value("SID").toUInt(&ok));
        if (!ok)
        {
            qCWarning(serviceList) << "Problem loading SID";
            continue;
        }
        item.SCIdS = serviceMap.value("SCIdS").toUInt(&ok);
        if (!ok)
        {
            qCWarning(serviceList, "Problem loading SCIdS item: %6.6X", item.SId.value());
            continue;
        }
        item.label = serviceMap.value("Label").toString();
        item.labelShort = serviceMap.value("ShortLabel").toString();

        bool fav = serviceMap.value("Fav").toBool();
        int currentEns = serviceMap.value("LastEns").toInt();

        QVariantList ensList = serviceMap.value("Ensembles").toList();
        if (currentEns >= ensList.count())
        {  // protection from inconsistent JSON caused by manual editing
            currentEns = 0;
        }
        for (auto ensIt = ensList.cbegin(); ensIt != ensList.cend(); ++ensIt)
        {
            auto ensMap = ensIt->toMap();
            ens.ueid = ensMap.value("UEID").toUInt(&ok);
            if (!ok)
            {
                qCWarning(serviceList, "Problem loading ensemble UEID for service %6.6X", item.SId.value());
                continue;
            }
            ens.frequency = ensMap.value("Frequency").toUInt(&ok);
            if (!ok)
            {
                qCWarning(serviceList, "Problem loading ensemble frequency for service %6.6X", item.SId.value());
                continue;
            }
            ens.label = ensMap.value("Label").toString();
            ens.labelShort = ensMap.value("ShortLabel").toString();
            // float snr = settings.value("SNR").toFloat();
            addService(ens, item, fav, currentEns);
            numServices += 1;
        }
        numUniqueServices += 1;
    }
    qCInfo(serviceList, "Service list loaded: number of services in all ensembles %d, unique services %d", numServices, numUniqueServices);
}

void ServiceList::loadFromSettings(QSettings *settings)
{
    int numServ = settings->beginReadArray("ServiceList");

    RadioControlServiceComponent item;
    RadioControlEnsemble ens;

    for (int s = 0; s < numServ; ++s)
    {
        bool ok = true;
        settings->setArrayIndex(s);
        // filling only what is necessary
        item.SId.set(settings->value("SID").toUInt(&ok));
        if (!ok)
        {
            qCWarning(serviceList) << "Problem loading SID item:" << s;
            continue;
        }
        item.SCIdS = settings->value("SCIdS").toUInt(&ok);
        if (!ok)
        {
            qCWarning(serviceList) << "Problem loading SCIdS item:" << s;
            continue;
        }
        item.label = settings->value("Label").toString();
        item.labelShort = settings->value("ShortLabel").toString();

        bool fav = settings->value("Fav").toBool();
        int currentEns = settings->value("LastEns").toInt();
        int numEns = settings->beginReadArray("Ensemble");
        for (int e = 0; e < numEns; ++e)
        {
            settings->setArrayIndex(e);
            ens.ueid = settings->value("UEID").toUInt(&ok);
            if (!ok)
            {
                qCWarning(serviceList) << "Problem loading service" << s << "ensemble UEID" << e;
                continue;
            }
            ens.frequency = settings->value("Frequency").toUInt(&ok);
            if (!ok)
            {
                qCWarning(serviceList) << "Problem loading service" << s << "ensemble frequency" << e;
                continue;
            }
            ens.label = settings->value("Label").toString();
            ens.labelShort = settings->value("ShortLabel").toString();
            // float snr = settings.value("SNR").toFloat();
            addService(ens, item, fav, currentEns);
        }
        settings->endArray();
    }
    settings->endArray();
}

void ServiceList::exportCSV(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        qCWarning(serviceList) << "Failed to export CSV file.";
        return;
    }

    QTextStream out(&file);
    // header
    out << tr("Service name;Short label;SID;SCIdS;Number of ensembles") << Qt::endl;
    // export in alphabetical order
    QStringList labelList;
    for (auto &s : m_serviceList)
    {
        labelList.append(s->label());
    }
    labelList.sort();

    for (const auto &label : labelList)
    {
        auto it =
            std::find_if(m_serviceList.cbegin(), m_serviceList.cend(), [&label](const ServiceListItem *item) { return item->label() == label; });
        if (it != m_serviceList.cend())
        {
            out << it.value()->label() << ";" << it.value()->shortLabel() << ";" << QString::number(it.value()->SId().value(), 16).toUpper() << ";"
                << it.value()->SCIdS() << ";" << it.value()->numEnsembles() << Qt::endl;
        }
    }
    file.close();
}

// this marks all services as obsolete
void ServiceList::beginEnsembleUpdate(const RadioControlEnsemble &e)
{
    EnsembleListItem *pEns = nullptr;
    ServiceListId ensId(e);
    EnsembleListIterator eit = m_ensembleList.find(ensId);
    if (m_ensembleList.end() != eit)
    {  // found
        // ensemble to be updated
        (*eit)->beginUpdate();
    }
    else
    { /* do nothing, ensemble not found */
    }
}

// this removed all services marked as obsolete
void ServiceList::endEnsembleUpdate(const RadioControlEnsemble &e)
{
    EnsembleListItem *pEns = nullptr;
    ServiceListId ensId(e);
    EnsembleListIterator eit = m_ensembleList.find(ensId);
    if (m_ensembleList.end() != eit)
    {  // found
        pEns = *eit;
    }
    else
    {  // do nothing, ensemble not found
        return;
    }

    // this removes all services marked as obsolete from the ensemble list
    pEns->endUpdate();

    // not we need to remove all obsolete services from the service list
    ServiceListIterator it = m_serviceList.begin();
    while (it != m_serviceList.end())
    {
        if ((*it)->isObsolete())
        {  // service is obsolete
            qCInfo(serviceList, "Removing service: [%6.6X] %-18s %X : %d", e.ueid, (*it)->label().toUtf8().data(), (*it)->SId().value(),
                   (*it)->SCIdS());

            emit serviceRemovedFromEnsemble(ensId, (*it)->id());

            if (false == (*it)->removeEnsemble(pEns))
            {  // this was the last ensemble -> remove service completely
                emit serviceRemoved((*it)->id());

                delete *it;                    // release memory
                it = m_serviceList.erase(it);  // remove record from hash
            }
            else
            {
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}

void ServiceList::removeEnsemble(const RadioControlEnsemble &e)
{
    ServiceListId ensId(e);
    EnsembleListIterator eit = m_ensembleList.find(ensId);
    if (m_ensembleList.end() != eit)
    {
        qCInfo(serviceList, "Removing ens %6.6X from service list", e.ueid);

        beginEnsembleUpdate(e);
        endEnsembleUpdate(e);

        delete *eit;
        m_ensembleList.erase(eit);

        emit ensembleRemoved(ensId);
    }
}
