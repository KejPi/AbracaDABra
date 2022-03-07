#include "servicelist.h"

ServiceList::ServiceList(QObject * parent) : QObject(parent)
{

}

ServiceList::~ServiceList()
{
    clear();
}

void ServiceList::clear()
{
    foreach (ServiceListItem * item, m_serviceList)
    {
        delete item;
    }
    m_serviceList.clear();

    foreach (EnsembleListItem * item, m_ensembleList)
    {
        delete item;
    }
    m_ensembleList.clear();

    emit empty();
}

void ServiceList::addService(const RadioControlEnsemble & e, const RadioControlServiceComponent & s, bool fav, int currentEns)
{
    bool newService = false;
    bool newEnsemble = false;

    qDebug("\tService: %s SID = 0x%X, SCIdS = %d", s.label.toLocal8Bit().data(), s.SId.value(), s.SCIdS);

    ServiceListItem * pService = nullptr;
    uint64_t servId = ServiceListItem::getId(s);
    ServiceListIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() == sit)
    {  // not found
        pService = new ServiceListItem(s, fav, currentEns);
        m_serviceList.insert(servId, pService);
        newService = true;
    }
    else
    {  // found
        pService = *sit;
    }

    EnsembleListItem * pEns = nullptr;
    uint64_t ensId = EnsembleListItem::getId(e);
    EnsembleListIterator eit = m_ensembleList.find(ensId);
    if (m_ensembleList.end() == eit)
    {  // not found
        pEns = new EnsembleListItem(e);
        m_ensembleList.insert(ensId, pEns);
        newEnsemble = true;
    }
    else
    {  // found
        pEns = *eit;
    }

    // we have ens and service item => lets link them together
    pService->addEnsemble(pEns);
    if (pEns->addService(pService))
    {   // new service in ensemble
        emit serviceAddedToEnsemble(pEns, pService);
    }
    if (newService)
    {   // emit signal when new service is added
        emit serviceAdded(pService);
    }
}

void ServiceList::setServiceFavorite(uint64_t servId, bool ena)
{
    ServiceListIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {   // found
        (*sit)->setFavorite(ena);
    }
}

bool ServiceList::isServiceFavorite(uint64_t servId) const
{
    ServiceListConstIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {   // found
        return (*sit)->isFavorite();
    }
    return false;
}

int ServiceList::numEnsembles(uint64_t servId) const
{
    if (0 == servId)
    {
        return m_ensembleList.size();
    }
    else
    {
        ServiceListConstIterator sit = m_serviceList.find(servId);
        if (m_serviceList.end() != sit)
        {   // found
            return (*sit)->numEnsembles();
        }
        return 0;
    }
}

int ServiceList::currentEnsembleIdx(uint64_t servId) const
{
    ServiceListConstIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() != sit)
    {   // found
        return (*sit)->currentEnsembleIdx();
    }
    return 0;
}

void ServiceList::save(QSettings & settings)
{
    // first sort service list by ID
    // this is needed to restore secondary services correctly
    QVector<uint64_t> idVect;
    for (auto & s : m_serviceList)
    {
        idVect.append(s->getId());
    }
    std::sort(idVect.begin(), idVect.end());

    settings.beginWriteArray("ServiceList", m_serviceList.size());          
    int n = 0;
    for (auto id : idVect)
    {
        ServiceListIterator it = m_serviceList.find(id);

        settings.setArrayIndex(n++);
        settings.setValue("SID", (*it)->SId().value());
        settings.setValue("SCIdS", (*it)->SCIdS());
        settings.setValue("Label", (*it)->label());
        settings.setValue("ShortLabel", (*it)->shortLabel());
        settings.setValue("Fav", (*it)->isFavorite());
        settings.setValue("LastEns", (*it)->currentEnsembleIdx());
        settings.beginWriteArray("Ensemble", (*it)->numEnsembles());
        for (int e = 0; e < (*it)->numEnsembles(); ++e)
        {
            settings.setArrayIndex(e);
            settings.setValue("UEID", (*it)->getEnsemble(e)->ueid());
            settings.setValue("Frequency", (*it)->getEnsemble(e)->frequency());
            settings.setValue("Label", (*it)->getEnsemble(e)->label());
            settings.setValue("ShortLabel", (*it)->getEnsemble(e)->shortLabel());
            //settings.setValue("SNR", (*it)->getEnsemble(e)->snr());
        }
        settings.endArray();
    }
    settings.endArray();
}

void ServiceList::load(QSettings & settings)
{
    int numServ = settings.beginReadArray("ServiceList");
    RadioControlServiceComponent item;
    RadioControlEnsemble ens;

    for (int s = 0; s < numServ; ++s)
    {
        bool ok = true;
        settings.setArrayIndex(s);
        // filling only what is necessary
        item.SId.set(settings.value("SID").toUInt(&ok));
        if (!ok)
        {
            qDebug() << "Problem loading SID item:" << s;
            continue;
        }
        item.SCIdS = settings.value("SCIdS").toUInt(&ok);
        if (!ok)
        {
            qDebug() << "Problem loading SCIdS item:" << s;
            continue;
        }
        item.label = settings.value("Label").toString();
        item.labelShort = settings.value("ShortLabel").toString();

        bool fav = settings.value("Fav").toBool();
        int currentEns = settings.value("LastEns").toInt();
        int numEns = settings.beginReadArray("Ensemble");
        for (int e = 0; e < numEns; ++e)
        {
            settings.setArrayIndex(e);
            ens.ueid = settings.value("UEID").toUInt(&ok);
            if (!ok)
            {
                qDebug() << "Problem loading service" << s << "ensemble UEID" << e;
                continue;
            }
            ens.frequency = settings.value("Frequency").toUInt(&ok);
            if (!ok)
            {
                qDebug() << "Problem loading service" << s << "ensemble frequency" << e;
                continue;
            }
            ens.label = settings.value("Label").toString();
            ens.labelShort = settings.value("ShortLabel").toString();
            //float snr = settings.value("SNR").toFloat();
            addService(ens, item, fav, currentEns);

        }       
        settings.endArray();
    }
    settings.endArray();
}



