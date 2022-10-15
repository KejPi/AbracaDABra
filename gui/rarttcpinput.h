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

#define RARTTCP_CHUNK_SIZE (16384*100)

#define RARTTCP_START_COUNTER_INIT 2 // init value of the counter used to reset buffer after tune

class RartTcpWorker : public QThread
{
    Q_OBJECT
public:
    explicit RartTcpWorker(SOCKET sock, QObject *parent = nullptr);
    void dumpToFileStart(FILE * dumpFile);
    void dumpToFileStop();
    void catureIQ(bool ena);
    bool isRunning();
protected:
    void run() override;
signals:
    void dumpedBytes(ssize_t bytes);

private:
    SOCKET m_sock;

    std::atomic<bool> m_enaDumpToFile;
    std::atomic<bool> m_enaCaptureIQ;
    std::atomic<bool> m_watchdogFlag;
    FILE * m_dumpFile;
    QMutex m_dumpFileMutex;
    int m_captureStartCntr;

    // input buffer
    uint8_t m_bufferIQ[RARTTCP_CHUNK_SIZE];

    bool isDumpingIQ() const { return m_enaDumpToFile; }
    void dumpBuffer(unsigned char *buf, uint32_t len);
    void processInputData(unsigned char *buf, uint32_t len);
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
    bool openDevice() override;

public slots:
    void tune(uint32_t frequency) override;
    void setTcpIp(const QString & address, int port);

    void startDumpToFile(const QString & filename) override;
    void stopDumpToFile() override;

private:    
    uint32_t m_frequency;
    bool m_deviceUnpluggedFlag;
    SOCKET m_sock;
    QString m_address;
    int m_port;

    RartTcpWorker * m_worker;
    QTimer m_watchdogTimer;
    FILE * m_dumpFile;

    void run();
    void stop();

    // private function
    void sendCommand(const RartTcpCommand & cmd, uint32_t param);
private slots:
    void onReadThreadStopped();
    void onWatchdogTimeout();
};


#endif // RARTTCPINPUT_H
