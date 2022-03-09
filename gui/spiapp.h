#ifndef SPIAPP_H
#define SPIAPP_H

#include <QObject>
#include "radiocontrol.h"
#include "motdecoder.h"
#include "userapplication.h"

class SPIApp : public UserApplication
{
    Q_OBJECT
public:
    SPIApp(RadioControl * radioControlPtr, QObject *parent = nullptr);
    ~SPIApp();
    void onNewMOTObject(const MOTObject & obj) override;
    void onUserAppData(const RadioControlUserAppData & data) override;
    void start() override;
    void stop() override;
    void restart() override;
private:
    MOTDecoder * decoder;
};

#endif // SPIAPP_H
