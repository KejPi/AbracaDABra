#ifndef RARTTCPINPUT_H
#define RARTTCPINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <stdio.h>
#include "inputdevice.h"

// socket
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

#define RARTTCP_PORT 1234
#define RARTTCP_ADDRESS "127.0.0.1"
#define RARTTCP_CHUNK_SIZE (16384*100)

#define RARTTCP_WDOG_ENABLE 1        // enable watchdog timer
#define RARTTCP_WDOG_TIMEOUT_SEC 1   // watchdow timeout in seconds

class RartTcpWorker : public QThread
{
    Q_OBJECT
public:
    explicit RartTcpWorker(SOCKET socket, QObject *parent = nullptr);
    void dumpToFileStart(FILE * f);
    void dumpToFileStop();
    void catureIQ(bool ena);
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
private:
    SOCKET sock;

    std::atomic<bool> enaDumpToFile;
    std::atomic<bool> enaCaptureIQ;
    std::atomic<bool> wdogIsRunningFlag;
    FILE * dumpFile;
    QMutex fileMutex;           

    // DOC memory
    float dcI = 0.0;
    float dcQ = 0.0;

    // AGC memory
    float agcLev = 0.0;

    // input buffer
    uint8_t bufferIQ[RARTTCP_CHUNK_SIZE];

    bool isDumpingIQ() const { return enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);
    void emitAgcLevel(float level, int maxVal) { emit agcLevel(level, maxVal); }

    friend void rarttcpCb(unsigned char *buf, uint32_t len, void *ctx);
};

class RartTcpInput : public InputDevice
{
    Q_OBJECT

    enum class RartTcpCommand
    {
        SET_FREQ             = 0x01,
    };

public:
    explicit RartTcpInput(QObject *parent = nullptr);
    ~RartTcpInput();
    bool isAvailable() override;

public slots:
    void tune(uint32_t freq) override;
    void stop() override;
    void openDevice();

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

private:    
    uint32_t frequency;
    bool deviceUnplugged;
    SOCKET sock;

    RartTcpWorker * worker;
#if (RARTTCP_WDOG_ENABLE)
    QTimer watchDogTimer;
#endif
    FILE * dumpFile;

    void run();

    // private function
    void sendCommand(const RartTcpCommand & cmd, uint32_t param);
private slots:
    void readThreadStopped();
#if (RARTTCP_WDOG_ENABLE)
    void watchDogTimeout();
#endif
};


#endif // RARTTCPINPUT_H
