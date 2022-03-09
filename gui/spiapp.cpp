#include "spiapp.h"

SPIApp::SPIApp(RadioControl * radioControlPtr, QObject *parent) : UserApplication(radioControlPtr, parent)
{
    decoder = nullptr;
    connect(radioControl, &RadioControl::userAppData, this, &SPIApp::onUserAppData);
}

SPIApp::~SPIApp()
{
    if (nullptr != decoder)
    {
        delete decoder;
    }
}

void SPIApp::start()
{   // does nothing is application is alraeady running
    if (isRunning)
    {   // do nothign, application is running
        return;
    }
    else
    { /* not running */ }

    // create new decoder
    decoder = new MOTDecoder();
    connect(decoder, &MOTDecoder::newMOTObject, this, &SPIApp::onNewMOTObject);

    isRunning = true;
}

void SPIApp::stop()
{
    if (nullptr != decoder)
    {
        delete decoder;
        decoder = nullptr;
    }
    isRunning = false;

    // ask HMI to clear SLS
    emit resetTerminal();
}

void SPIApp::restart()
{
    stop();
    start();
}

void SPIApp::onUserAppData(const RadioControlUserAppData & data)
{
    if ((DabUserApplicationType::SPI == data.userAppType) && (isRunning))
    {
        // application is running and user application type matches
        // data is for this application
        // send data to decoder
        decoder->newDataGroup(data.data);
    }
    else
    { /* do nothing */ }
}

void SPIApp::onNewMOTObject(const MOTObject & obj)
{
    qDebug() << Q_FUNC_INFO;
}
