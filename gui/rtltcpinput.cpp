/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDir>
#include <QDebug>
#include <QLoggingCategory>
#include "rtltcpinput.h"

Q_LOGGING_CATEGORY(rtlTcpInput, "RtlTcpInput", QtInfoMsg)

const int RtlTcpInput::e4k_gains[] = { -10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420 };
const int RtlTcpInput::fc0012_gains[] = { -99, -40, 71, 179, 192 };
const int RtlTcpInput::fc0013_gains[] = { -99, -73, -65, -63, -60, -58, -54, 58, 61,
                                         63, 65, 67, 68, 70, 71, 179, 181, 182,
                                         184, 186, 188, 191, 197 };
const int RtlTcpInput::fc2580_gains[] = { 0 /* no gain values */ };
const int RtlTcpInput::r82xx_gains[] = { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
                                        166, 197, 207, 229, 254, 280, 297, 328,
                                        338, 364, 372, 386, 402, 421, 434, 439,
                                        445, 480, 496 };
const int RtlTcpInput::r82xx_gains_olddab[] = {
                                        0,34,68,102,137,171,207,240,278,312,346,382,416,453,488,527};
const int RtlTcpInput::unknown_gains[] = { 0 /* no gain values */ };

#if defined(_WIN32)
class SocketInitialiseWrapper {
public:
    SocketInitialiseWrapper() {
        WSADATA wsa;
        qCInfo(rtlTcpInput) << "RTL-TCP: Initialising Winsock...";

        if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
        {
            qCCritical(rtlTcpInput) << "RTL-TCP: Winsock init failed. Error Code:" << WSAGetLastError();
        }
    }

    ~SocketInitialiseWrapper() {
        WSACleanup();
    }

    SocketInitialiseWrapper(SocketInitialiseWrapper&) = delete;
    SocketInitialiseWrapper& operator=(SocketInitialiseWrapper&) = delete;
};

static SocketInitialiseWrapper socketInitialiseWrapper;
#endif

RtlTcpInput::RtlTcpInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDeviceId::RTLTCP;

    m_gainList = nullptr;    
    m_worker = nullptr;
    m_agcLevelMinFactorList = nullptr;
    m_agcLevelMax = RTLTCP_AGC_LEVEL_MAX_DEFAULT;
    m_agcLevelMin = 60;

    m_frequency = 0;
    m_sock = INVALID_SOCKET;
    m_address = "127.0.0.1";
    m_port = 1234;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &RtlTcpInput::onWatchdogTimeout);
}

RtlTcpInput::~RtlTcpInput()
{
    // need to end worker thread and close socket
    if (nullptr != m_worker)
    {
        m_worker->captureIQ(false);

        // close socket
#if defined(_WIN32)
        closesocket(m_sock);
#else
        ::close(m_sock);
#endif
        m_sock = INVALID_SOCKET;

        m_worker->wait(2000);

        while  (!m_worker->isFinished())
        {
            qCWarning(rtlTcpInput) << "Worker thread not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
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
}


bool RtlTcpInput::openDevice()
{
    if (nullptr != m_worker)
    {   // device already opened
        return true;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

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
        return false;
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

        struct sockaddr_in *sa = (struct sockaddr_in *) rp->ai_addr;
        qCInfo(rtlTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        // https://docs.microsoft.com/en-us/previous-versions/windows/embedded/aa450263(v=msdn.10)
        //  It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect (Windows Sockets)
        // on a nonblocking SOCK_STREAM socket, since some time must elapse for the connection to be established.
#if (_WIN32_WINNT >= 0x0600)
        struct pollfd  pfd;
        pfd.fd = sfd;
        pfd.events = POLLIN;
        if (WSAPoll(&pfd, 1, 2000) > 0)
        {
            qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            flags = 0;  // If flags != 0, non-blocking mode is enabled.
            if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
            {
                qCWarning(rtlTcpInput) << "Failed to set blocking socket";
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {   // -1 is error, 0 is timeout
            qCCritical(rtlTcpInput) << "Unable to connect";
        }
#else   // (_WIN32_WINNT < 0x0600)
        // poll API does not exist :-(
        // this part was not tested
        fd_set connFd;
        FD_ZERO(&connFd);
        FD_SET(sfd, &connFd);

        // check if the socket is ready
        TIMEVAL connTimeout;
        connTimeout.tv_sec = 2;
        connTimeout.tv_usec = 0;
        if (::select(sfd+1, nullptr, &connFd, nullptr, &connTimeout) > 0)
        {
            qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            flags = 0;  // If flags != 0, non-blocking mode is enabled.
            if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
            {
                qCWarning(rtlTcpInput) << "Failed to set blocking socket";
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {   // -1 is error, 0 is timeout
            qCCritical(rtlTcpInput) << "Unable to connect";
        }
#endif

#else   // not defined(_WIN32)
        long arg;
        if( (arg = fcntl(sfd, F_GETFL, NULL)) < 0)
        {
            qCWarning(rtlTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
        }
        arg |= O_NONBLOCK;
        if( fcntl(sfd, F_SETFL, arg) < 0)
        {
            qCWarning(rtlTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
        }

        struct sockaddr_in *sa = (struct sockaddr_in *) rp->ai_addr;
        qCInfo(rtlTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);

        struct pollfd pfd;
        pfd.fd = sfd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 2000) > 0)
        {
            qCInfo(rtlTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            // set bloking mode again
            if( (arg = fcntl(sfd, F_GETFL, NULL)) < 0)
            {
                qCWarning(rtlTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
            }
            arg &= (~O_NONBLOCK);
            if( fcntl(sfd, F_SETFL, arg) < 0)
            {
                qCWarning(rtlTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {   // -1 is error, 0 is timeout
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
    {   /* No address succeeded */
        qCCritical(rtlTcpInput) << "Could not connect";
        return false;
    }

    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    // get information about RTL stick
#if defined(_WIN32)
#if (_WIN32_WINNT >= 0x0600)
    struct pollfd  fd;
    fd.fd = m_sock;
    fd.events = POLLIN;
    if (WSAPoll(&fd, 1, 2000) > 0)
    {
        ::recv(m_sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        return false;
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
    if (::select(sock+1, nullptr, &readFd, nullptr, &Timeout) > 0)
    {
        ::recv(sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        return false;
    }
#endif
#else
    struct pollfd fd;
    fd.fd = m_sock;
    fd.events = POLLIN;
    if (poll(&fd, 1, 2000) > 0)
    {
        ::recv(m_sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        return false;
    }
#endif

    // Convert the byte order
    dongleInfo.tunerType = ntohl(dongleInfo.tunerType);
    dongleInfo.tunerGainCount = ntohl(dongleInfo.tunerGainCount);
    if(dongleInfo.magic[0] == 'R' &&
        dongleInfo.magic[1] == 'T' &&
        dongleInfo.magic[2] == 'L' &&
        dongleInfo.magic[3] == '0')
    {
        m_deviceDescription.device.name = "rtl_tcp";
        m_deviceDescription.device.model = "Generic RTL2832U OEM";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 8;
        m_deviceDescription.sample.containerBits = 8;
        m_deviceDescription.sample.channelContainer = "uint8";

        const int * gains = unknown_gains;
        int numGains = 0;
        switch(dongleInfo.tunerType)
        {
        case RTLSDR_TUNER_E4000:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_E4000";
            gains = e4k_gains;
            numGains = *(&e4k_gains + 1) - e4k_gains;
            m_deviceDescription.device.name += " [E4000]";
            break;
        case RTLSDR_TUNER_FC0012:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0012";
            gains = fc0012_gains;
            numGains = *(&fc0012_gains + 1) - fc0012_gains;
            m_deviceDescription.device.name += " [FC0012]";
            break;
        case RTLSDR_TUNER_FC0013:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0013";
            gains = fc0013_gains;
            numGains = *(&fc0013_gains + 1) - fc0013_gains;
            m_deviceDescription.device.name += " [FC0013]";
            break;
        case RTLSDR_TUNER_FC2580:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC2580";
            gains = fc2580_gains;
            numGains = *(&fc2580_gains + 1) - fc2580_gains;
            m_deviceDescription.device.name += " [FC2580]";
            break;
        case RTLSDR_TUNER_R820T:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R820T";
            numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
            if (dongleInfo.tunerGainCount == numGains) {
                gains = r82xx_gains_olddab;
            }
            else {
                numGains = *(&r82xx_gains + 1) - r82xx_gains;
                gains = r82xx_gains;
            }
            m_deviceDescription.device.name += " [R820T]";
            break;
        case RTLSDR_TUNER_R828D:
            qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R828D";
            numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
            if (dongleInfo.tunerGainCount == numGains) {
                gains = r82xx_gains_olddab;
            }
            else {
                numGains = *(&r82xx_gains + 1) - r82xx_gains;
                gains = r82xx_gains;
            }
            m_deviceDescription.device.name += " [R828D]";
            break;
        case RTLSDR_TUNER_UNKNOWN:
        default:
        {
            qCWarning(rtlTcpInput) << "RTLSDR_TUNER_UNKNOWN";
            dongleInfo.tunerGainCount = 0;
        }
        }

        if (dongleInfo.tunerGainCount != numGains)
        {
            qCWarning(rtlTcpInput) << "unexpected number of gain values reported by server" << dongleInfo.tunerGainCount;
            if (dongleInfo.tunerGainCount > numGains)
            {
                dongleInfo.tunerGainCount = numGains;
            }
        }


        if (nullptr != m_gainList)
        {
            delete m_gainList;
        }
        m_gainList = new QList<int>();
        for (int i = 0; i < dongleInfo.tunerGainCount; i++)
        {
            m_gainList->append(gains[i]);
        }

        m_agcLevelMinFactorList = new QList<float>();
        for (int i = 1; i < m_gainList->count(); i++) {
            // up step + 0.5dB
            m_agcLevelMinFactorList->append(qPow(10.0, (m_gainList->at(i-1) - m_gainList->at(i) - 5)/200.0));
        }
        // last factor does not matter
        m_agcLevelMinFactorList->append(qPow(10.0, -5.0 / 20.0));

        // set sample rate
        sendCommand(RtlTcpCommand::SET_SAMPLE_RATE, 2048000);

        // set automatic gain
        //setGainMode(RtlGainMode::Software);

        // need to create worker, server is pushing samples
        m_worker = new RtlTcpWorker(m_sock, this);
        connect(m_worker, &RtlTcpWorker::agcLevel, this, &RtlTcpInput::onAgcLevel, Qt::QueuedConnection);
        connect(m_worker, &RtlTcpWorker::dataReady, this, [=](){ emit tuned(m_frequency); }, Qt::QueuedConnection);
        connect(m_worker, &RtlTcpWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
        connect(m_worker, &RtlTcpWorker::finished, this, &RtlTcpInput::onReadThreadStopped, Qt::QueuedConnection);
        connect(m_worker, &RtlTcpWorker::finished, m_worker, &QObject::deleteLater);
        connect(m_worker, &RtlTcpWorker::destroyed, this, [=]() { m_worker = nullptr; } );
        m_worker->start();
        m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
        emit deviceReady();
    }
    else
    {
        qCCritical(rtlTcpInput) << "RTL-TCP: \"RTL0\" magic key not found. Server not supported";
        return false;
    }

    return true;
}

void RtlTcpInput::tune(uint32_t frequency)
{
    m_frequency = frequency;

    if ((m_frequency > 0) && (nullptr != m_worker))
    {   // Tune to new frequency
        sendCommand(RtlTcpCommand::SET_FREQ, m_frequency*1000);

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

void RtlTcpInput::setTcpIp(const QString &address, int port)
{
    m_address = address;
    m_port = port;
}

void RtlTcpInput::setGainMode(RtlGainMode gainMode, int gainIdx)
{
    if (gainMode != m_gainMode)
    {
        // set automatic gain 0 or manual 1
        sendCommand(RtlTcpCommand::SET_GAIN_MODE, (RtlGainMode::Hardware != gainMode));
        setDAGC(RtlGainMode::Hardware == gainMode);   // enable for HW, disable otherwise

        m_gainMode = gainMode;

        // does nothing in (GainMode::Software != mode)
        resetAgc();
    }

    if (RtlGainMode::Manual == m_gainMode)
    {
        setGain(gainIdx);

        // always emit gain when switching mode to manual
        emit agcGain(m_gainList->at(gainIdx)*0.1);
    }

    if (RtlGainMode::Hardware == m_gainMode)
    {   // signalize that gain is not available
        emit agcGain(NAN);
    }
}

void RtlTcpInput::setAgcLevelMax(float agcLevelMax)
{
    if (agcLevelMax <= 0)
    {   // default value
        agcLevelMax = RTLTCP_AGC_LEVEL_MAX_DEFAULT;
    }
    m_agcLevelMax = agcLevelMax;
    m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;

    //qDebug() << m_agcLevelMax << m_agcLevelMin;
}

void RtlTcpInput::setGain(int gainIdx)
{
    if (!m_gainList->empty())
    {
        // force index validity
        if (gainIdx < 0)
        {
            gainIdx = 0;
        }
        if (gainIdx >= m_gainList->size())
        {
            gainIdx = m_gainList->size() - 1;
        }

        if (gainIdx != m_gainIdx)
        {
            m_gainIdx = gainIdx;
            m_agcLevelMin = m_agcLevelMinFactorList->at(m_gainIdx) * m_agcLevelMax;
            sendCommand(RtlTcpCommand::SET_GAIN_IDX, m_gainIdx);
            emit agcGain(m_gainList->at(m_gainIdx)*0.1);
        }
    }
    else { /* empy gain list => do nothing */}
}

void RtlTcpInput::resetAgc()
{
    setDAGC(RtlGainMode::Hardware == m_gainMode);   // enable for HW, disable otherwise

    if (RtlGainMode::Software == m_gainMode)
    {
        setGain(m_gainList->size() >> 1);
    }
}

void RtlTcpInput::setDAGC(bool ena)
{
    sendCommand(RtlTcpCommand::SET_AGC_MODE, ena);
}

void RtlTcpInput::onAgcLevel(float agcLevel)
{
    if (RtlGainMode::Software == m_gainMode)
    {
        if (agcLevel < m_agcLevelMin)
        {
            setGain(m_gainIdx+1);
        }
        if (agcLevel > m_agcLevelMax)
        {
            setGain(m_gainIdx-1);
        }
    }
}

void RtlTcpInput::onReadThreadStopped()
{
    qCCritical(rtlTcpInput) << "server disconnected.";

    // close socket
#if defined(_WIN32)
    closesocket(m_sock);
#else
    ::close(m_sock);
#endif
    m_sock = INVALID_SOCKET;
    m_watchdogTimer.stop();

    // fill buffer (artificially to avoid blocking of the DAB processing thread)
    inputBuffer.fillDummy();

    emit error(InputDeviceErrorCode::DeviceDisconnected);
}

void RtlTcpInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        if (!m_worker->isRunning())
        {  // some problem in data input
            qCCritical(rtlTcpInput) << "watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDeviceErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void RtlTcpInput::startStopRecording(bool start)
{
    m_worker->startStopRecording(start);
}

QList<float> RtlTcpInput::getGainList() const
{
    QList<float> ret;
    for (int g = 0; g < m_gainList->size(); ++g)
    {
        ret.append(m_gainList->at(g)/10.0);
    }
    return ret;
}

void RtlTcpInput::sendCommand(const RtlTcpCommand & cmd, uint32_t param)
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

    ::send(m_sock, (char *) cmdBuffer, 5, 0);
}

RtlTcpWorker::RtlTcpWorker(SOCKET sock, QObject *parent) : QThread(parent)
{
    m_isRecording = false;
    m_enaCaptureIQ = false;
    m_sock = sock;
}

void RtlTcpWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void RtlTcpWorker::run()
{
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_watchdogFlag = false;  // first callback sets it to true

    // read samples
    while (INVALID_SOCKET != m_sock)
    {
        size_t read = 0;
        do
        {
            ssize_t ret = ::recv(m_sock, (char *) m_bufferIQ+read, RTLTCP_CHUNK_SIZE - read, 0);
            if (0 == ret)
            {   // disconnected => finish thread operation
                qCCritical(rtlTcpInput) << "socket disconnected";
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
                {   // disconnected => finish thread operation
                    // when socket is diconnected under Win, recv returns -1 but error code is 0
                    qCCritical(rtlTcpInput) << "RTL-TCP: socket disconnected";
                    goto worker_exit;
                }
                else if ((WSAECONNRESET == WSAGetLastError()) || (WSAEBADF == WSAGetLastError()))
                {   // disconnected => finish thread operation
                    qCCritical(rtlTcpInput) << "RTL-TCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
                else
                {
                    qCWarning(rtlTcpInput) << "RTL-TCP: socket read error:" << strerror(WSAGetLastError());
                }
#else
                if ((EAGAIN == errno) || (EINTR == errno))
                {
                    continue;
                }
                else if ((ECONNRESET == errno) || (EBADF == errno))
                {   // disconnected => finish thread operation
                    qCCritical(rtlTcpInput) << "error: " << strerror(errno);
                    goto worker_exit;
                }
                else
                {
                    qCWarning(rtlTcpInput) << "socket read error:" << strerror(errno);
                }
#endif
            }
            else
            {
                read += ret;
            }
        } while (RTLTCP_CHUNK_SIZE > read);

        // reset watchDog flag, timer sets it to true
        m_watchdogFlag = true;

        // full chunk is read at this point
        if (m_enaCaptureIQ)
        {   // process data
            if (m_captureStartCntr > 0)
            {   // reset procedure
                if (0 == --m_captureStartCntr)
                {   // restart finished

                    // clear buffer to avoid mixing of channels
                    inputBuffer.reset();

                    m_dcI = 0.0;
                    m_dcQ = 0.0;
                    //m_agcLevel = 0.0;

                    emit dataReady();
                }
                else
                {   // only reecord if recording
                    if (m_isRecording)
                    {
                        emit recordBuffer(m_bufferIQ, RTLTCP_CHUNK_SIZE);
                    }
                    else { /* not recording */ }

                    // done
                    continue;
                }
            }
            processInputData(m_bufferIQ, RTLTCP_CHUNK_SIZE);
        }
    }

worker_exit:    
    // single exit point
    return;
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

    if ((INPUT_FIFO_SIZE - count) < len*sizeof(float))
    {
        qCWarning(rtlTcpInput) << "dropping" << len << "bytes...";
        return;
    }

    // input samples are IQ = [uint8_t uint8_t]
    // going to transform them to [float float] = float _Complex
    // on uint8_t will be transformed to one float

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    uint8_t * inPtr = buf;
    if (bytesTillEnd >= len*sizeof(float))
    {
        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<len; k++)
        {   // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLTCP_AGC_ENABLE > 0)

#if (RTLTCP_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLTCP_DOC_ENABLE
#endif  // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (inputBuffer.head + len*sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2; // / sizeof(float);

        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<samplesTillEnd; ++k)
        {   // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLTCP_AGC_ENABLE > 0)

#if (RTLTCP_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLTCP_DOC_ENABLE
#endif  // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k=0; k<len-samplesTillEnd; ++k)
        {   // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128; // I or Q

#if (RTLTCP_AGC_ENABLE > 0)
            int_fast8_t absTmp = abs(tmp);

            // calculate signal level (rectifier, fast attack slow release)
            float c = m_agcLevel_crel;
            if (absTmp > agcLev)
            {
                c = m_agcLevel_catt;
            }
            agcLev = c * absTmp + agcLev - c * agcLev;
#endif  // (RTLTCP_AGC_ENABLE > 0)

#if (RTLTCP_DOC_ENABLE > 0)
            // subtract DC
            if (k & 0x1)
            {   // Q
                sumQ += tmp;
                *outPtr++ = float(tmp) - dcQ;
            }
            else
            {  // I
                sumI += tmp;
                *outPtr++ = float(tmp) - dcI;
            }
#else
            *outPtr++ = float(tmp);
#endif  // RTLTCP_DOC_ENABLE
#endif  // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);
    }

#if (RTLTCP_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    m_dcI = sumI * m_doc_c / (len >> 1) + dcI - m_doc_c * dcI;
    m_dcQ = sumQ * m_doc_c / (len >> 1) + dcQ - m_doc_c * dcQ;
#endif

#if (RTLTCP_AGC_ENABLE > 0)
    // store memory
    m_agcLevel = agcLev;
    emit agcLevel(agcLev);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

