/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include "rtltcpinput.h"

#include <QDebug>
#include <QDir>
#include <QLoggingCategory>
#include <QtEndian>
#include <cerrno>
#include <cstring>

Q_LOGGING_CATEGORY(rtlTcpInput, "RtlTcpInput", QtInfoMsg)

const int RtlTcpInput::e4k_gains[] = {-10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420};
const int RtlTcpInput::e4k_gains_olddab[] = {0, 29, 60, 89, 119, 147, 176, 206, 235, 264, 294, 323, 353, 382, 408, 436, 466, 495, 521, 548};
const int RtlTcpInput::fc0012_gains[] = {-99, -40, 71, 179, 192};
const int RtlTcpInput::fc0013_gains[] = {-99, -73, -65, -63, -60, -58, -54, 58, 61, 63, 65, 67, 68, 70, 71, 179, 181, 182, 184, 186, 188, 191, 197};
const int RtlTcpInput::fc2580_gains[] = {0 /* no gain values */};
const int RtlTcpInput::r82xx_gains[] = {0,   9,   14,  27,  37,  77,  87,  125, 144, 157, 166, 197, 207, 229, 254,
                                        280, 297, 328, 338, 364, 372, 386, 402, 421, 434, 439, 445, 480, 496};
const int RtlTcpInput::r82xx_gains_olddab[] = {0, 34, 68, 102, 137, 171, 207, 240, 278, 312, 346, 382, 416, 453, 488, 527};
const int RtlTcpInput::unknown_gains[] = {0 /* no gain values */};

#if defined(_WIN32)
class SocketInitialiseWrapper
{
public:
    SocketInitialiseWrapper()
    {
        WSADATA wsa;
        qCInfo(rtlTcpInput) << "RTL-TCP: Initialising Winsock...";

        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            qCCritical(rtlTcpInput) << "RTL-TCP: Winsock init failed. Error Code:" << WSAGetLastError();
        }
    }

    ~SocketInitialiseWrapper() { WSACleanup(); }

    SocketInitialiseWrapper(SocketInitialiseWrapper &) = delete;
    SocketInitialiseWrapper &operator=(SocketInitialiseWrapper &) = delete;
};

static SocketInitialiseWrapper socketInitialiseWrapper;
#endif

RtlTcpInput::RtlTcpInput(bool useNativeSocket, QObject *parent) : InputDevice(parent), m_useNativeSocket(useNativeSocket)
{
    m_deviceDescription.id = InputDevice::Id::RTLTCP;

    m_gainList = nullptr;
    m_worker = nullptr;
    m_controlSocket = nullptr;
    m_controlSocketEna = false;
    m_haveControlSocket = false;
    m_agcLevelMinFactorList = nullptr;
    m_agcLevelMax = RTLTCP_AGC_LEVEL_MAX_DEFAULT;
    m_agcLevelMin = 60;
    m_levelCalcCntr = 0;
    m_rfLevelOffset = 0.0;

    m_frequency = 0;
    m_address = "127.0.0.1";
    m_port = 1234;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &RtlTcpInput::onWatchdogTimeout);

    if (m_useNativeSocket)
    {
        qCInfo(rtlTcpInput) << "Using native socket implementation";
    }
    else
    {
        qCInfo(rtlTcpInput) << "Using QTcpSocket implementation";
    }
}

RtlTcpInput::~RtlTcpInput()
{
    // need to end worker thread and close socket
    if (nullptr != m_worker)
    {
        m_worker->captureIQ(false);
        m_worker->requestStop();
        m_worker->wait(2000);
        while (!m_worker->isFinished())
        {
            qCWarning(rtlTcpInput) << "Worker thread not finished after timeout - this should not happen :-(";

            inputBuffer.flush();
            m_worker->wait(2000);
        }
    }
    if (nullptr != m_gainList)
    {
        delete m_gainList;
    }
    if (nullptr != m_agcLevelMinFactorList)
    {
        delete m_agcLevelMinFactorList;
    }
    if (nullptr != m_controlSocket)
    {
        m_controlSocket->disconnectFromHost();
        if (m_controlSocket->state() != QAbstractSocket::UnconnectedState && !m_controlSocket->waitForDisconnected(2000))
        {
            m_controlSocket->abort();
        }
        delete m_controlSocket;
    }
}

bool RtlTcpInput::openDevice(const QVariant &hwId, bool fallbackConnection)
{
    Q_UNUSED(hwId)
    Q_UNUSED(fallbackConnection)

    if (nullptr != m_worker)
    {  // device already opened
        return true;
    }

    m_worker = new RtlTcpWorker(m_address, m_port, m_useNativeSocket, this);

    connect(m_worker, &RtlTcpWorker::serverInfo, this, &RtlTcpInput::onServerInfo, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::agcLevel, this, &RtlTcpInput::onAgcLevel, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::dataReady, this, [=]() { emit tuned(m_frequency); }, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &RtlTcpWorker::destroyed, this, [=]() { m_worker = nullptr; });

    if (m_useNativeSocket)
    {
        connect(m_worker, &RtlTcpWorker::finished, this, &RtlTcpInput::onReadThreadStopped, Qt::QueuedConnection);
    }
    else
    {
        connect(m_worker, &RtlTcpWorker::errorOccurred, this, &RtlTcpInput::onStreamSocketError, Qt::QueuedConnection);
    }

    m_worker->start();

    if (m_controlSocketEna)
    {  // try to connect to control socket
        QTimer::singleShot(100, this, [this]() { initControlSocket(); });
    }

    return true;
}

void RtlTcpInput::onServerInfo(uint32_t tunerType, uint32_t tunerGainCount)
{
    // Convert the byte order
    const int *gains = unknown_gains;
    if (tunerType != RTLSDR_TUNER_UNKNOWN)
    {
        m_deviceDescription.device.name = "rtl_tcp";
        m_deviceDescription.device.model = "Generic RTL2832U";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 8;
        m_deviceDescription.sample.containerBits = 8;
        m_deviceDescription.sample.channelContainer = "uint8";

        int numGains = 0;
        switch (tunerType)
        {
            case RTLSDR_TUNER_E4000:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_E4000";
                numGains = *(&e4k_gains_olddab + 1) - e4k_gains_olddab;
                if (tunerGainCount == numGains)
                {
                    gains = e4k_gains_olddab;
                }
                else
                {
                    numGains = *(&e4k_gains + 1) - e4k_gains;
                    gains = e4k_gains;
                }
                m_deviceDescription.device.tuner = "E4000";
                break;
            case RTLSDR_TUNER_FC0012:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0012";
                gains = fc0012_gains;
                numGains = *(&fc0012_gains + 1) - fc0012_gains;
                m_deviceDescription.device.tuner = "FC0012";
                break;
            case RTLSDR_TUNER_FC0013:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0013";
                gains = fc0013_gains;
                numGains = *(&fc0013_gains + 1) - fc0013_gains;
                m_deviceDescription.device.tuner = "FC0013";
                break;
            case RTLSDR_TUNER_FC2580:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC2580";
                gains = fc2580_gains;
                numGains = *(&fc2580_gains + 1) - fc2580_gains;
                m_deviceDescription.device.tuner = "FC2580";
                break;
            case RTLSDR_TUNER_R820T:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R820T";
                numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
                if (tunerGainCount == numGains)
                {
                    gains = r82xx_gains_olddab;
                }
                else
                {
                    numGains = *(&r82xx_gains + 1) - r82xx_gains;
                    gains = r82xx_gains;
                }
                m_deviceDescription.device.tuner = "R820T";
                break;
            case RTLSDR_TUNER_R828D:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R828D";
                numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
                if (tunerGainCount == numGains)
                {
                    gains = r82xx_gains_olddab;
                }
                else
                {
                    numGains = *(&r82xx_gains + 1) - r82xx_gains;
                    gains = r82xx_gains;
                }
                m_deviceDescription.device.tuner = "R828D";
                break;
            case RTLSDR_TUNER_UNKNOWN:
            default:
            {
                qCWarning(rtlTcpInput) << "RTLSDR_TUNER_UNKNOWN";
                tunerGainCount = 0;
                m_deviceDescription.device.tuner = "Unknown";
            }
        }
        m_deviceDescription.device.name += QString(" [%1]").arg(m_deviceDescription.device.tuner);

        if (tunerGainCount != numGains)
        {
            qCWarning(rtlTcpInput) << "Unexpected number of gain values reported by server" << tunerGainCount;
            if (tunerGainCount > numGains)
            {
                tunerGainCount = numGains;
            }
        }
    }
    else
    {  // this is connection to unknown server => lets try and cross the fingers
        qCWarning(rtlTcpInput) << "\"RTL0\" magic key not found. Unknown server.";

        m_deviceDescription.device.name = "TCP server";
        m_deviceDescription.device.model = "Unknown";
        m_deviceDescription.device.tuner = "Unknown";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 8;
        m_deviceDescription.sample.containerBits = 8;
        m_deviceDescription.sample.channelContainer = "uint8";

        tunerGainCount = 0;
    }

    if (nullptr != m_gainList)
    {
        delete m_gainList;
    }
    m_gainList = new QList<int>();
    for (int i = 0; i < tunerGainCount; i++)
    {
        m_gainList->append(gains[i]);
    }

    if (nullptr != m_agcLevelMinFactorList)
    {
        delete m_agcLevelMinFactorList;
    }
    m_agcLevelMinFactorList = new QList<float>();
    for (int i = 1; i < m_gainList->count(); i++)
    {
        // up step + 0.5dB
        m_agcLevelMinFactorList->append(qPow(10.0, (m_gainList->at(i - 1) - m_gainList->at(i) - 5) / 200.0));
    }
    // last factor does not matter
    m_agcLevelMinFactorList->append(qPow(10.0, -5.0 / 20.0));

    // set sample rate
    sendCommand(RtlTcpCommand::SET_SAMPLE_RATE, 2048000);

    // set automatic gain
    // setGainMode(RtlGainMode::Software);
    m_gainIdx = -1;

    m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);

    emit deviceReady();
}

void RtlTcpInput::tune(uint32_t frequency)
{
    m_frequency = frequency;

    if ((m_frequency > 0) && (nullptr != m_worker))
    {  // Tune to new frequency
        sendCommand(RtlTcpCommand::SET_FREQ, m_frequency * 1000);

        // does nothing if not SW AGC
        resetAgc();

        m_worker->captureIQ(true);

        // tuned(m_frequency) is emited when dataReady() from worker
    }
    else
    {
        if (nullptr != m_worker)
        {
            m_worker->captureIQ(false);
        }

        emit tuned(0);
    }
}

void RtlTcpInput::setTcpIp(const QString &address, int port, bool controlSockEna)
{
    m_address = address;
    m_port = port;
    m_controlSocketEna = controlSockEna;
}

void RtlTcpInput::setGainMode(RtlGainMode gainMode, int gainIdx)
{
    if (gainMode != m_gainMode)
    {
        // set automatic gain 0 or manual 1
        sendCommand(RtlTcpCommand::SET_GAIN_MODE, (RtlGainMode::Hardware != gainMode));
        setDAGC(RtlGainMode::Hardware == gainMode);  // enable for HW, disable otherwise

        m_gainMode = gainMode;

        // does nothing in (GainMode::Software != mode)
        resetAgc();
    }

    if (RtlGainMode::Manual == m_gainMode)
    {
        setGain(gainIdx);  // this limits gain index to valid range and sets m_gainIdx

        // always emit gain when switching mode to manual
        if (m_gainIdx >= 0)
        {
            emit agcGain(m_gainList->at(m_gainIdx) * 0.1);
        }
    }

    if (RtlGainMode::Hardware == m_gainMode)
    {  // signalize that gain is not available
        emit agcGain(NAN);
    }
    emit rfLevel(NAN, NAN);
}

void RtlTcpInput::setAgcLevelMax(float agcLevelMax)
{
    if (agcLevelMax <= 0)
    {  // default value
        agcLevelMax = RTLTCP_AGC_LEVEL_MAX_DEFAULT;
    }
    m_agcLevelMax = agcLevelMax;
    if (m_gainIdx >= 0)
    {
        m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;
    }
    else
    {
        m_agcLevelMin = 0.6 * agcLevelMax;
    }
}

void RtlTcpInput::setPPM(int ppm)
{
    if (ppm != m_ppm)
    {
        sendCommand(RtlTcpCommand::SET_FREQ_CORR, ppm);
        qCInfo(rtlTcpInput) << "Frequency correction PPM:" << ppm;
        m_ppm = ppm;
    }
}

void RtlTcpInput::setGain(int gIdx)
{
    if (!m_gainList->empty())
    {
        // force index validity
        if (gIdx < 0)
        {
            gIdx = 0;
        }
        if (gIdx >= m_gainList->size())
        {
            gIdx = m_gainList->size() - 1;
        }

        if (gIdx != m_gainIdx)
        {
            m_gainIdx = gIdx;
            m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;
            sendCommand(RtlTcpCommand::SET_GAIN_IDX, m_gainIdx);
            emit agcGain(m_gainList->at(m_gainIdx) * 0.1);
            emit gainIdx(m_gainIdx);
        }
    }
    else
    { /* empy gain list => do nothing */
    }
}

void RtlTcpInput::resetAgc()
{
    setDAGC(RtlGainMode::Hardware == m_gainMode);  // enable for HW, disable otherwise

    if (RtlGainMode::Software == m_gainMode)
    {
        setGain(m_gainList->size() >> 1);
    }
    m_levelCalcCntr = 0;
    emit rfLevel(NAN, NAN);
}

void RtlTcpInput::setDAGC(bool ena)
{
    sendCommand(RtlTcpCommand::SET_AGC_MODE, ena);
}

void RtlTcpInput::onAgcLevel(float agcLevel)
{
    if (m_haveControlSocket && (RtlGainMode::Hardware != m_gainMode))
    {
        if (++m_levelCalcCntr > 2)
        {
            m_levelCalcCntr = 0;
            emit rfLevel(m_20log10[static_cast<int>(std::roundf(agcLevel))] - m_deviceGain - 46 + m_rfLevelOffset, m_deviceGain);
        }
    }

    if (RtlGainMode::Software == m_gainMode)
    {
        if (agcLevel < m_agcLevelMin)
        {
            setGain(m_gainIdx + 1);
        }
        if (agcLevel > m_agcLevelMax)
        {
            setGain(m_gainIdx - 1);
        }
    }
}

void RtlTcpInput::onReadThreadStopped()
{
    qCCritical(rtlTcpInput) << "Server disconnected.";

    m_watchdogTimer.stop();

    // flush buffer to avoid blocking of the DAB processing thread
    inputBuffer.flush();

    emit error(InputDevice::ErrorCode::DeviceDisconnected);
}

void RtlTcpInput::onStreamSocketError(QAbstractSocket::SocketError e)
{
    qCCritical(rtlTcpInput) << "Server disconnected:" << e;

    m_watchdogTimer.stop();

    if (m_worker)
    {
        m_worker->requestStop();
        m_worker->wait(2000);
        while (!m_worker->isFinished())
        {
            qCWarning(rtlTcpInput) << "Worker thread not finished after timeout - this should not happen :-(";

            inputBuffer.flush();
            m_worker->wait(2000);
        }
    }

    // flush buffer to avoid blocking of the DAB processing thread
    inputBuffer.flush();

    emit error(InputDevice::ErrorCode::DeviceDisconnected);
}

void RtlTcpInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        if (!m_worker->isRunning())
        {  // some problem in data input
            qCCritical(rtlTcpInput) << "Watchdog timeout";

            m_worker->requestStop();
            m_worker->wait(2000);
            while (!m_worker->isFinished())
            {
                qCWarning(rtlTcpInput) << "Worker thread not finished after timeout - this should not happen :-(";

                inputBuffer.flush();
                m_worker->wait(2000);
            }

            inputBuffer.flush();
            emit error(InputDevice::ErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void RtlTcpInput::initControlSocket()
{
    m_controlSocket = new QTcpSocket(this);
    connect(m_controlSocket, &QAbstractSocket::connected, this,
            [this]()
            {
                m_haveControlSocket = true;
                qCInfo(rtlTcpInput) << "Control socket connected";
            });
    connect(m_controlSocket, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError error)
            {
                m_haveControlSocket = false;
                qCWarning(rtlTcpInput) << "Control socket error" << error;
            });
    connect(m_controlSocket, &QAbstractSocket::readyRead, this, &RtlTcpInput::readControlSocketData);

    m_controlSocket->connectToHost(QHostAddress(m_address), m_port + 1);
}

void RtlTcpInput::readControlSocketData()
{
    QByteArray data = m_controlSocket->readAll();
    if ((data.size() > 10) && (data.at(2) == 0) && (data.at(3) == 0) && (data.at(4) == 2))
    {
        int16_t val = static_cast<uint8_t>(data.at(5)) << 8;
        val = val | static_cast<uint8_t>(data.at(6));
        m_deviceGain = val * 0.1;
    }
}

void RtlTcpInput::startStopRecording(bool start)
{
    if (nullptr != m_worker)
    {
        m_worker->startStopRecording(start);
    }
}

QList<float> RtlTcpInput::getGainList() const
{
    QList<float> ret;
    for (int g = 0; g < m_gainList->size(); ++g)
    {
        ret.append(m_gainList->at(g) / 10.0);
    }
    return ret;
}

void RtlTcpInput::sendCommand(const RtlTcpCommand &cmd, uint32_t param)
{
    if (nullptr == m_worker)
    {
        return;
    }

    uint8_t cmdBuffer[5];

    cmdBuffer[0] = uint8_t(cmd);
    cmdBuffer[4] = param & 0xFF;
    cmdBuffer[3] = (param >> 8) & 0xFF;
    cmdBuffer[2] = (param >> 16) & 0xFF;
    cmdBuffer[1] = (param >> 24) & 0xFF;

    m_worker->writeData(QByteArray((char *)cmdBuffer, 5));
}

RtlTcpWorker::RtlTcpWorker(const QString &address, int port, bool useNativeSocket, QObject *parent)
    : QThread{parent}, m_address{address}, m_port(port), m_useNativeSocket(useNativeSocket)
{
    m_isRecording = false;
    m_enaCaptureIQ = false;
    m_stopRequested = false;

    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_agcLevelEmitCntr = 0;
    m_watchdogFlag = false;
}

void RtlTcpWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void RtlTcpWorker::writeData(const QByteArray &data)
{
    QMutexLocker locker(&m_commandMutex);
    m_commandQueue.enqueue(data);
}

void RtlTcpWorker::run()
{
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_agcLevelEmitCntr = 0;
    m_watchdogFlag = false;

    if (m_useNativeSocket)
    {
        runNativeSocket();
    }
    else
    {
        runQtSocket();
    }
}

void RtlTcpWorker::runNativeSocket()
{
    SOCKET sock = INVALID_SOCKET;

    // connect
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    QString portStr = QString().number(m_port);

    struct addrinfo *result;
    int s = getaddrinfo(m_address.toLatin1(), portStr.toLatin1(), &hints, &result);
    if (s != 0)
    {
#if defined(_WIN32)
        qCCritical(rtlTcpInput) << "getaddrinfo error:" << gai_strerrorA(s);
#else
        qCCritical(rtlTcpInput) << "getaddrinfo error:" << gai_strerror(s);
#endif
        return;
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */
    struct addrinfo *rp;
    int sfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }

        // Set non-blocking
#if defined(_WIN32)
        /// Windows sockets are created in blocking mode by default
        // currently on windows, there is no easy way to obtain the socket's current blocking mode since WSAIsBlocking was deprecated
        // https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket
        u_long flags = 1;  // If flags != 0, non-blocking mode is enabled.
        if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
        {
            qCWarning(rtlTcpInput) << "Failed to set non-blocking socket";
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
        qCInfo(rtlTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        // https://docs.microsoft.com/en-us/previous-versions/windows/embedded/aa450263(v=msdn.10)
        //  It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect (Windows Sockets)
        // on a nonblocking SOCK_STREAM socket, since some time must elapse for the connection to be established.
#if (_WIN32_WINNT >= 0x0600)
        struct pollfd pfd;
        pfd.fd = sfd;
        pfd.events = POLLOUT;
        if (WSAPoll(&pfd, 1, 5000) > 0)
        {
            int sockErr = 0;
            int sockErrLen = sizeof(sockErr);
            if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, (char *)&sockErr, &sockErrLen) != 0 || sockErr != 0)
            {
                qCCritical(rtlTcpInput, "Connection failed: %d", sockErr ? sockErr : WSAGetLastError());
            }
            else
            {
                qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

                flags = 0;  // If flags != 0, non-blocking mode is enabled.
                if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
                {
                    qCWarning(rtlTcpInput) << "Failed to set blocking socket";
                }

                sock = sfd;
                break; /* Success */
            }
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rtlTcpInput) << "Unable to connect";
        }
#else  // (_WIN32_WINNT < 0x0600)
       // poll API does not exist :-(
       // this part was not tested
        fd_set connFd;
        FD_ZERO(&connFd);
        FD_SET(sfd, &connFd);

        // check if the socket is ready
        TIMEVAL connTimeout;
        connTimeout.tv_sec = 2;
        connTimeout.tv_usec = 0;
        if (::select(sfd + 1, nullptr, &connFd, nullptr, &connTimeout) > 0)
        {
            qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            flags = 0;  // If flags != 0, non-blocking mode is enabled.
            if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
            {
                qCWarning(rtlTcpInput) << "Failed to set blocking socket";
            }

            sock = sfd;
            break; /* Success */
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rtlTcpInput) << "Unable to connect";
        }
#endif

#else  // not defined(_WIN32)
        long arg;
        if ((arg = fcntl(sfd, F_GETFL, NULL)) < 0)
        {
            qCWarning(rtlTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
        }
        arg |= O_NONBLOCK;
        if (fcntl(sfd, F_SETFL, arg) < 0)
        {
            qCWarning(rtlTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
        qCInfo(rtlTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);

        struct pollfd pfd;
        pfd.fd = sfd;
        pfd.events = POLLOUT;
        if (poll(&pfd, 1, 5000) > 0)
        {
            int sockErr = 0;
            socklen_t sockErrLen = sizeof(sockErr);
            if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, &sockErr, &sockErrLen) < 0 || sockErr != 0)
            {
                qCCritical(rtlTcpInput, "Connection failed: %s", strerror(sockErr ? sockErr : errno));
            }
            else
            {
                qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

                // set bloking mode again
                if ((arg = fcntl(sfd, F_GETFL, NULL)) < 0)
                {
                    qCWarning(rtlTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
                }
                arg &= (~O_NONBLOCK);
                if (fcntl(sfd, F_SETFL, arg) < 0)
                {
                    qCWarning(rtlTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
                }

                sock = sfd;
                break; /* Success */
            }
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rtlTcpInput) << "Unable to connect";
        }
#endif

#if defined(_WIN32)
        closesocket(sfd);
#else
        ::close(sfd);
#endif
    }

    if (NULL == rp)
    { /* No address succeeded */
        qCCritical(rtlTcpInput) << "Could not connect";
        freeaddrinfo(result);
        return;
    }

    freeaddrinfo(result);

    // read dongle info
    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    // get information about RTL stick
#if defined(_WIN32)
#if (_WIN32_WINNT >= 0x0600)
    struct pollfd fd;
    fd.fd = sock;
    fd.events = POLLIN;
    if (WSAPoll(&fd, 1, 10000) > 0)
    {
        ::recv(sock, (char *)&dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        goto worker_exit;
    }
#else
    // poll API does not exist :-(
    fd_set readFd;
    FD_ZERO(&readFd);
    FD_SET(sock, &readFd);

    // check if the socket is ready
    TIMEVAL Timeout;
    Timeout.tv_sec = 2;
    Timeout.tv_usec = 0;
    if (::select(sock + 1, nullptr, &readFd, nullptr, &Timeout) > 0)
    {
        ::recv(sock, (char *)&dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        goto worker_exit;
    }
#endif
#else
    struct pollfd fd;
    fd.fd = sock;
    fd.events = POLLIN;
    if (poll(&fd, 1, 10000) > 0)
    {
        if (::recv(sock, (char *)&dongleInfo, sizeof(dongleInfo), 0) <= 0)
        {
            qCCritical(rtlTcpInput) << "Server not responding.";
            goto worker_exit;
        }
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        goto worker_exit;
    }
#endif

    // we are here when dongle information is received
    if (dongleInfo.magic[0] == 'R' && dongleInfo.magic[1] == 'T' && dongleInfo.magic[2] == 'L' && dongleInfo.magic[3] == '0')
    {
        emit serverInfo(ntohl(dongleInfo.tunerType), ntohl(dongleInfo.tunerGainCount));
    }
    else
    {
        emit serverInfo(RTLSDR_TUNER_UNKNOWN, 0);
    }

    // store socket to be used to force close
    m_sock = sock;

    // read samples
    while (!m_stopRequested && INVALID_SOCKET != sock)
    {
        size_t read = 0;
        do
        {
            ssize_t ret = ::recv(sock, (char *)m_bufferIQ + read, RTLTCP_CHUNK_SIZE - read, 0);
            if (0 == ret)
            {  // disconnected => finish thread operation
                qCCritical(rtlTcpInput) << "Socket disconnected";
                goto worker_exit;
            }
            else if (-1 == ret)
            {
#if _WIN32
                if (WSAEINTR == WSAGetLastError())
                {
                    continue;
                }
                else if (WSAECONNABORTED == WSAGetLastError())
                {  // disconnected => finish thread operation
                    // when socket is diconnected under Win, recv returns -1 but error code is 0
                    qCCritical(rtlTcpInput) << "RTL-TCP: socket disconnected";
                    goto worker_exit;
                }
                else if ((WSAECONNRESET == WSAGetLastError()) || (WSAEBADF == WSAGetLastError()))
                {  // disconnected => finish thread operation
                    qCCritical(rtlTcpInput) << "RTL-TCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
                else
                {
                    qCCritical(rtlTcpInput) << "RTL-TCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
#else
                if ((EAGAIN == errno) || (EINTR == errno))
                {
                    continue;
                }
                else if ((ECONNRESET == errno) || (EBADF == errno))
                {  // disconnected => finish thread operation
                    qCCritical(rtlTcpInput) << "Error: " << strerror(errno);
                    goto worker_exit;
                }
                else
                {
                    qCCritical(rtlTcpInput) << "Socket read error:" << strerror(errno);
                    goto worker_exit;
                }
#endif
            }
            else
            {
                read += ret;
            }
        } while (RTLTCP_CHUNK_SIZE > read);

        // reset watchDog flag, timer sets it to false
        m_watchdogFlag = true;

        flushCommandQueue(sock);

        // full chunk is read at this point
        if (m_enaCaptureIQ)
        {  // process data
            if (m_captureStartCntr > 0)
            {  // reset procedure
                if (0 == --m_captureStartCntr)
                {  // restart finished

                    // clear buffer to avoid mixing of channels
                    inputBuffer.reset();

                    m_dcI = 0.0;
                    m_dcQ = 0.0;
                    m_bufferFillCntr = RTLTCP_START_COUNTER_FILL_BUFFER;
                    processInputData(m_bufferIQ, RTLTCP_CHUNK_SIZE);

                    // dataReady() is emitted from processInputData when buffer is filled to avoid audio dropouts
                    // emit dataReady()
                }
                else
                {
                    if (m_isRecording)
                    {
                        emit recordBuffer(m_bufferIQ, RTLTCP_CHUNK_SIZE);
                    }
                    else
                    { /* not recording */
                    }

                    // done
                    continue;
                }
            }
            processInputData(m_bufferIQ, RTLTCP_CHUNK_SIZE);
        }
    }

worker_exit:
#if defined(_WIN32)
    closesocket(sock);
#else
    ::close(sock);
#endif
}

void RtlTcpWorker::runQtSocket()
{
    // --- open connection
    QTcpSocket socket;
    connect(&socket, &QAbstractSocket::errorOccurred, this, &RtlTcpWorker::errorOccurred);

    socket.setReadBufferSize(4 * RTLTCP_CHUNK_SIZE);  // 1 MB Qt buffer

    qCInfo(rtlTcpInput) << "Connecting to server...";
    socket.connectToHost(QHostAddress(m_address), m_port);
    if (socket.waitForConnected(10000) == false)
    {
        qCCritical(rtlTcpInput) << "Unable to connect";
        return;
    }

    // Increase OS receive buffer to 8 MB to avoid TCP backpressure at 4 MB/s with
    // 256 KB chunks. Default SO_RCVBUF on many systems (~128-256 KB) is too small.
    socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 8 * 1024 * 1024);

    QVariant actual = socket.socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption);
    qCInfo(rtlTcpInput) << "Actual SO_RCVBUF:" << actual.toInt();

    // --- read dongle info (blocking) ---
    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    while (socket.bytesAvailable() < (qint64)sizeof(dongleInfo))
    {
        if (!socket.waitForReadyRead(10000))
        {
            qCCritical(rtlTcpInput) << "Server not responding.";
            return;
        }
    }
    socket.read((char *)&dongleInfo, sizeof(dongleInfo));

    if (dongleInfo.magic[0] == 'R' && dongleInfo.magic[1] == 'T' && dongleInfo.magic[2] == 'L' && dongleInfo.magic[3] == '0')
    {
        emit serverInfo(qFromBigEndian<quint32>(dongleInfo.tunerType), qFromBigEndian<quint32>(dongleInfo.tunerGainCount));
    }
    else
    {
        emit serverInfo(RTLSDR_TUNER_UNKNOWN, 0);
    }

    // --- main sample loop ---
    size_t accumulated = 0;
    while (!m_stopRequested)
    {
        if (!socket.waitForReadyRead(3000))
        {
            qCCritical(rtlTcpInput) << "Socket disconnected";
            return;
        }

        m_watchdogFlag = true;

        flushCommandQueue(socket);

        while (socket.bytesAvailable() > 0 && !m_stopRequested)
        {
            qint64 n = socket.read((char *)m_bufferIQ + accumulated, RTLTCP_CHUNK_SIZE - accumulated);
            if (n <= 0)
            {
                qCCritical(rtlTcpInput) << "Socket read error";
                return;
            }
            accumulated += (size_t)n;

            if (accumulated >= RTLTCP_CHUNK_SIZE)
            {
                accumulated = 0;

                if (m_enaCaptureIQ)
                {
                    if (m_captureStartCntr > 0)
                    {
                        if (0 == --m_captureStartCntr)
                        {
                            inputBuffer.reset();

                            m_dcI = 0.0;
                            m_dcQ = 0.0;
                            m_bufferFillCntr = RTLTCP_START_COUNTER_FILL_BUFFER;
                            processInputData(m_bufferIQ, RTLTCP_CHUNK_SIZE);

                            // dataReady() is emitted from processInputData when buffer is filled to avoid audio dropouts
                            // emit dataReady()
                        }
                        else
                        {
                            if (m_isRecording)
                            {
                                emit recordBuffer(m_bufferIQ, RTLTCP_CHUNK_SIZE);
                            }
                        }
                    }
                    else
                    {
                        processInputData(m_bufferIQ, RTLTCP_CHUNK_SIZE);
                    }
                }
            }
        }
    }
    socket.disconnectFromHost();
}

void RtlTcpWorker::requestStop()
{
    m_stopRequested = true;
    if (m_useNativeSocket && m_sock != INVALID_SOCKET)
    {  // force closing of the socket to interrupt blocking recv() call
#if defined(_WIN32)
        closesocket(m_sock);
#else
        ::close(m_sock);
#endif
    }
}

void RtlTcpWorker::flushCommandQueue(SOCKET &socket)
{
    QMutexLocker locker(&m_commandMutex);
    if (m_commandQueue.isEmpty())
    {
        return;
    }
    while (!m_commandQueue.isEmpty())
    {
        ::send(socket, (char *)m_commandQueue.dequeue().constData(), 5, 0);
    }
}

void RtlTcpWorker::flushCommandQueue(QTcpSocket &socket)
{
    QMutexLocker locker(&m_commandMutex);
    if (m_commandQueue.isEmpty())
    {
        return;
    }
    while (!m_commandQueue.isEmpty())
    {
        socket.write(m_commandQueue.dequeue());
    }
    locker.unlock();
    socket.flush();
}

void RtlTcpWorker::captureIQ(bool ena)
{
    if (ena)
    {
        m_captureStartCntr = RTLTCP_START_COUNTER_INIT;
    }
    m_enaCaptureIQ = ena;
}

bool RtlTcpWorker::isRunning()
{
    bool flag = m_watchdogFlag;
    m_watchdogFlag = false;
    return flag;
}

void RtlTcpWorker::processInputData(unsigned char *buf, uint32_t len)
{
#if (RTLTCP_DOC_ENABLE > 0)
    int_fast32_t sumI = 0;
    int_fast32_t sumQ = 0;
#endif

    if (m_isRecording)
    {
        emit recordBuffer(buf, len);
    }

    // retrieving memories
#if (RTLTCP_DOC_ENABLE > 0)
    float dcI = m_dcI;
    float dcQ = m_dcQ;
#endif
#if (RTLTCP_AGC_ENABLE > 0)
    float agcLev = m_agcLevel;
#endif

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    if ((INPUT_FIFO_SIZE - count) < len * sizeof(float))
    {
        qCWarning(rtlTcpInput) << "Dropping" << len << "bytes...";
        return;
    }

    // input samples are IQ = [uint8_t uint8_t]
    // going to transform them to [float float] = float _Complex
    // one uint8_t will be transformed to one float

    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    uint8_t *inPtr = buf;
    if (bytesTillEnd >= len * sizeof(float))
    {
        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < len; k++)
        {
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);
#else
            int_fast8_t tmp = *inPtr++ - 128;

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif

#if (RTLTCP_DOC_ENABLE > 0)
            if (k & 0x1)
            {
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif
#endif
        }
        inputBuffer.head = (inputBuffer.head + len * sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2;

        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < samplesTillEnd; ++k)
        {
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);
#else
            int_fast8_t tmp = *inPtr++ - 128;

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif

#if (RTLTCP_DOC_ENABLE > 0)
            if (k & 0x1)
            {
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif
#endif
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k = 0; k < len - samplesTillEnd; ++k)
        {
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);
#else
            int_fast8_t tmp = *inPtr++ - 128;

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif

#if (RTLTCP_DOC_ENABLE > 0)
            if (k & 0x1)
            {
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif
#endif
        }
        inputBuffer.head = (len - samplesTillEnd) * sizeof(float);
    }

#if (RTLTCP_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    m_dcI = sumI * m_doc_c / (len >> 1) + dcI - m_doc_c * dcI;
    m_dcQ = sumQ * m_doc_c / (len >> 1) + dcQ - m_doc_c * dcQ;
#endif

#if (RTLTCP_AGC_ENABLE > 0)
    // store memory
    m_agcLevel = agcLev;
    if (0 == (++m_agcLevelEmitCntr & 0x03))
    {
        emit agcLevel(agcLev);
    }
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len * sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);

    if (m_bufferFillCntr > 0)
    {
        m_bufferFillCntr -= 1;
        if (m_bufferFillCntr <= 0)
        {
            emit dataReady();
        }
    }
}
