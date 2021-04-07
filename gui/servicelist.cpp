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

void ServiceList::addService(const RadioControlServiceListEntry & s)
{
    bool newService = false;
    bool newEnsemble = false;

    qDebug("\tService: %s SID = 0x%X, SCIdS = %d", s.label.toLocal8Bit().data(), s.SId.value, s.SCIdS);

    ServiceListItem * pService = nullptr;
    uint64_t servId = ServiceListItem::getId(s);
    ServiceListIterator sit = m_serviceList.find(servId);
    if (m_serviceList.end() == sit)
    {  // not found
        pService = new ServiceListItem(s);
        m_serviceList.insert(servId, pService);
        newService = true;
    }
    else
    {  // found
        pService = *sit;
    }

    EnsembleListItem * pEns = nullptr;
    uint64_t ensId = EnsembleListItem::getId(s);
    EnsembleListIterator eit = m_ensembleList.find(ensId);
    if (m_ensembleList.end() == eit)
    {  // not found
        pEns = new EnsembleListItem(s);
        m_ensembleList.insert(ensId, pEns);
        newEnsemble = true;
    }
    else
    {  // found
        pEns = *eit;
    }

    // we have ens and service item => lets link them together
    pService->addEnsemble(pEns);
    pEns->addService(pService);

    if (newService)
    {   // new service
        emit serviceAdded(pService);
    }
    if (newEnsemble)
    {   // new ensemble
        emit ensembleAdded(pEns);
    }
}

void ServiceList::save(QSettings & settings)
{
    settings.beginWriteArray("ServiceList", m_serviceList.size());
    int n = 0;
    for (ServiceListIterator it = m_serviceList.begin(); it != m_serviceList.end(); ++it)
    {
        settings.setArrayIndex(n++);
        settings.setValue("SID", (*it)->SId().value);
        settings.setValue("SCIdS", (*it)->SCIdS());
        settings.setValue("Label", (*it)->label());
        settings.setValue("ShortLabel", (*it)->shortLabel());
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
    RadioControlServiceListEntry item;
    for (int s = 0; s < numServ; ++s)
    {
        bool ok = true;
        settings.setArrayIndex(s);
        // filling only what is necessary
        item.SId.value = settings.value("SID").toUInt(&ok);
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

        int numEns = settings.beginReadArray("Ensemble");
        for (int e = 0; e < numEns; ++e)
        {
            settings.setArrayIndex(e);
            item.ensemble.ueid = settings.value("UEID").toUInt(&ok);
            if (!ok)
            {
                qDebug() << "Problem loading service" << s << "ensemble UEID" << e;
                continue;
            }
            item.ensemble.frequency = settings.value("Frequency").toUInt(&ok);
            if (!ok)
            {
                qDebug() << "Problem loading service" << s << "ensemble frequency" << e;
                continue;
            }
            item.ensemble.label = settings.value("Label").toString();
            item.ensemble.labelShort = settings.value("ShortLabel").toString();
            //float snr = settings.value("SNR").toFloat();
            addService(item);

        }
        settings.endArray();
    }
    settings.endArray();
}

ServiceListItem::ServiceListItem(const RadioControlServiceListEntry & item)
{
    m_sid = item.SId;
    m_scids = item.SCIdS;
    m_label = item.label;
    m_shortLabel = item.labelShort;
}

void ServiceListItem::addEnsemble(EnsembleListItem * ensPtr)
{
    QList<EnsembleListItem *>::iterator it = findEnsemble(ensPtr->getId());
    if (m_ensembleList.end() == it)
    {
        m_ensembleList.append(ensPtr);
    }
}


bool ServiceListItem::operator==(const ServiceListItem & other)
{
    return getId() == other.getId();
}

QList<EnsembleListItem *>::iterator ServiceListItem::findEnsemble(uint64_t id)
{
    QList<EnsembleListItem *>::iterator it;
    for (it = m_ensembleList.begin(); it < m_ensembleList.end(); ++it)
    {
        if ((*it)->getId() == id)
        {
            return it;
        }
    }

    return it;
}

const EnsembleListItem * ServiceListItem::getEnsemble(int num) const
{
    if (num < 0)
    {   // best ensemble
        if (m_ensembleList.size())
        {
            return m_ensembleList.at(0);
        }
//        EnsembleListItem * ensPtr = m_ensembleList.at(0);
//        for (int e = 0; e < m_ensembleList.size(); ++e)
//        {
//            //if (m_ensembleList.at(e)->)
//        }
    }
    else
    {
        if (num < m_ensembleList.size())
        {
            return m_ensembleList.at(num);
        }
    }
    return nullptr;
}


EnsembleListItem::EnsembleListItem(const RadioControlServiceListEntry & item)
{
    m_frequency = item.ensemble.frequency;
    m_ueid = item.ensemble.ueid;
    m_label = item.ensemble.label;
    m_shortLabel = item.ensemble.labelShort;
}

bool EnsembleListItem::operator==(const EnsembleListItem & other)
{
    return getId() == other.getId();
}

void EnsembleListItem::addService(ServiceListItem * servPtr)
{
    QList<ServiceListItem *>::iterator it = findService(servPtr->getId());
    if (m_serviceList.end() == it)
    {
        m_serviceList.append(servPtr);
    }
}

QList<ServiceListItem *>::iterator EnsembleListItem::findService(uint64_t id)
{
    QList<ServiceListItem *>::iterator it;
    for (it = m_serviceList.begin(); it < m_serviceList.end(); ++it)
    {
        if ((*it)->getId() == id)
        {
            return it;
        }
    }

    return it;
}

const ServiceListItem * EnsembleListItem::getService(int num) const
{
    if (num < m_serviceList.size())
    {
        return m_serviceList.at(num);
    }
    return nullptr;
}

