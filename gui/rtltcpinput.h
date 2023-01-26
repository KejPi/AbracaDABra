#ifndef RTLTCPINPUT_H
#define RTLTCPINPUT_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <rtl-sdr.h>
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

#define RTLTCP_CHUNK_SIZE (16384*100)

#define RTLTCP_DOC_ENABLE 1         // enable DOC
#define RTLTCP_AGC_ENABLE 1         // enable AGC
#define RTLTCP_START_COUNTER_INIT 2 // init value of the counter used to reset buffer after tune

class RtlTcpWorker : public QThread
{
    Q_OBJECT
public:
    explicit RtlTcpWorker(SOCKET sock, QObject *parent = nullptr);
    void catureIQ(bool ena);
    void startStopRecording(bool ena);
    bool isRunning();
protected:
    void run() override;
signals:
    void agcLevel(float level, int maxVal);
    void recordBuffer(const uint8_t * buf, uint32_t len);
private:
    SOCKET m_sock;

    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_enaCaptureIQ;
    std::atomic<bool> m_watchdogFlag;
    int m_captureStartCntr;   

    // DOC memory
    float m_dcI = 0.0;
    float m_dcQ = 0.0;

    // AGC memory
    float m_agcLevel = 0.0;

    // input buffer
    uint8_t m_bufferIQ[RTLTCP_CHUNK_SIZE];

    void processInputData(unsigned char *buf, uint32_t len);
};

class RtlTcpInput : public InputDevice
{
    Q_OBJECT

    enum class RtlTcpCommand
    {
        SET_FREQ             = 0x01,
        SET_SAMPLE_RATE      = 0x02,
        SET_GAIN_MODE        = 0x03,
        SET_GAIN             = 0x04,
        SET_FREQ_CORR        = 0x05,
        SET_IF_GAIN          = 0x06,
        SET_TEST_MODE        = 0x07,
        SET_AGC_MODE         = 0x08,
        SET_DIRECT_SAMPLING  = 0x09,
        SET_OFFSET_TUNING    = 0x0A,
        SET_RTL_XTAL_FREQ    = 0x0B,
        SET_TUNER_XTAL_FREQ  = 0x0C,
        SET_GAIN_IDX         = 0x0D,
        SET_BIAS_TEE         = 0x0E
    };

    /* taken from rtlsdr_get_tuner_gains() implementation */
    /* all gain values are expressed in tenths of a dB */
    static const int e4k_gains[];
    static const int fc0012_gains[];
    static const int fc0013_gains[];
    static const int fc2580_gains[];
    static const int r82xx_gains[];
    static const int unknown_gains[];

public:
    explicit RtlTcpInput(QObject *parent = nullptr);
    ~RtlTcpInput();
    bool openDevice() override;
    void tune(uint32_t frequency) override;
    void setTcpIp(const QString & address, int port);
    void setGainMode(RtlGainMode gainMode, int gainIdx = 0);
    void setDAGC(bool ena);
    void startStopRecording(bool start) override;
    QList<float> getGainList() const;
private:    
    uint32_t m_frequency;
    bool m_deviceUnpluggedFlag;
    SOCKET m_sock;
    QString m_address;
    int m_port;

    RtlTcpWorker * m_worker;
    QTimer m_watchdogTimer;
    RtlGainMode m_gainMode = RtlGainMode::Hardware;
    int m_gainIdx;
    QList<int> * m_gainList;

    void run();
    void stop();
    void resetAgc();

    // private function
    // gain is set from outside using setGainMode() function
    void setGain(int gainIdx);

    // used by worker
    void onAgcLevel(float agcLevel, int maxVal);

    void sendCommand(const RtlTcpCommand & cmd, uint32_t param);
private slots:
    void onReadThreadStopped();
    void onWatchdogTimeout();
};


#endif // RTLTCPINPUT_H
