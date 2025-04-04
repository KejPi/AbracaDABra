#ifndef RARTTCPINPUT_H
#define RARTTCPINPUT_H

#include <stdio.h>

#include <QObject>
#include <QThread>
#include <QTimer>

#include "inputdevice.h"

// socket
#if defined(_WIN32)
// clang-format off
// keep order of includes for Windows
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
// clang-format on
#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

#define RARTTCP_CHUNK_SIZE (16384 * 100)

#define RARTTCP_START_COUNTER_INIT 2  // init value of the counter used to reset buffer after tune

class RartTcpWorker : public QThread
{
    Q_OBJECT
public:
    explicit RartTcpWorker(SOCKET sock, QObject *parent = nullptr);
    void captureIQ(bool ena);
    void startStopRecording(bool ena);
    bool isRunning();

protected:
    void run() override;
signals:
    void recordBuffer(const uint8_t *buf, uint32_t len);
    void dataReady();

private:
    SOCKET m_sock;

    const float m_int2float = 1.0 / 0x8000;

    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_enaCaptureIQ;
    std::atomic<bool> m_watchdogFlag;
    std::atomic<int8_t> m_captureStartCntr;

    // input buffer
    uint8_t m_bufferIQ[RARTTCP_CHUNK_SIZE];

    void processInputData(unsigned char *buf, uint32_t len);
};

class RartTcpInput : public InputDevice
{
    Q_OBJECT

    enum class RartTcpCommand
    {
        SET_FREQ = 0x01,
    };

public:
    explicit RartTcpInput(QObject *parent = nullptr);
    ~RartTcpInput();
    bool openDevice(const QVariant &hwId = QVariant(), bool fallbackConnection = true) override;
    void tune(uint32_t frequency) override;
    InputDevice::Capabilities capabilities() const override { return LiveStream | Recording; }
    void setTcpIp(const QString &address, int port);
    void startStopRecording(bool start) override;

private:
    uint32_t m_frequency;
    SOCKET m_sock;
    QString m_address;
    int m_port;

    RartTcpWorker *m_worker;
    QTimer m_watchdogTimer;

    // private function
    void sendCommand(const RartTcpCommand &cmd, uint32_t param);
private slots:
    void onReadThreadStopped();
    void onWatchdogTimeout();
};

#endif  // RARTTCPINPUT_H
