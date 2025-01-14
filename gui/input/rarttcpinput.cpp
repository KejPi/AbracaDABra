#include "rarttcpinput.h"

#include <QDebug>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(rartTcpInput, "RaRTTcpInput", QtInfoMsg)

#if defined(_WIN32)
class SocketInitialiseWrapper
{
public:
    SocketInitialiseWrapper()
    {
        WSADATA wsa;
        qDebug() << "RART-TCP: Initialising Winsock...";

        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            qDebug() << "RART-TCP: Winsock init failed. Error Code:" << WSAGetLastError();
        }
    }

    ~SocketInitialiseWrapper() { WSACleanup(); }

    SocketInitialiseWrapper(SocketInitialiseWrapper &) = delete;
    SocketInitialiseWrapper &operator=(SocketInitialiseWrapper &) = delete;
};

static SocketInitialiseWrapper socketInitialiseWrapper;
#endif

RartTcpInput::RartTcpInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDevice::Id::RARTTCP;

    m_worker = nullptr;
    m_frequency = 0;
    m_sock = INVALID_SOCKET;
    m_address = "127.0.0.1";
    m_port = 1235;

    connect(&m_watchdogTimer, &QTimer::timeout, this, &RartTcpInput::onWatchdogTimeout);
}

RartTcpInput::~RartTcpInput()
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

        while (!m_worker->isFinished())
        {
            qCWarning(rartTcpInput) << "Worker thread not finished after timeout - this should not happen :-(";

            // reset buffer - and tell the thread it is empty - buffer will be reset in any case
            pthread_mutex_lock(&inputBuffer.countMutex);
            inputBuffer.count = 0;
            pthread_cond_signal(&inputBuffer.countCondition);
            pthread_mutex_unlock(&inputBuffer.countMutex);
            m_worker->wait(2000);
        }
    }
}

bool RartTcpInput::openDevice(const QVariant &hwId)
{
    Q_UNUSED(hwId);

    if (nullptr != m_worker)
    {  // device already opened
        return true;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0; /* Any protocol */

    QString port_str = QString().number(m_port);

    struct addrinfo *result;
    int s = getaddrinfo(m_address.toLatin1(), port_str.toLatin1(), &hints, &result);
    if (s != 0)
    {
#if defined(_WIN32)
        qCCritical(rartTcpInput) << "getaddrinfo error:" << gai_strerrorA(s);
#else
        qCCritical(rartTcpInput) << "getaddrinfo error:" << gai_strerror(s);
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
            qCWarning(rartTcpInput) << "Failed to set non-blocking socket";
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
        qCInfo(rartTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        // https://docs.microsoft.com/en-us/previous-versions/windows/embedded/aa450263(v=msdn.10)
        //  It is normal for WSAEWOULDBLOCK to be reported as the result from calling connect (Windows Sockets)
        // on a nonblocking SOCK_STREAM socket, since some time must elapse for the connection to be established.
#if (_WIN32_WINNT >= 0x0600)
        struct pollfd pfd;
        pfd.fd = sfd;
        pfd.events = POLLIN;
        if (WSAPoll(&pfd, 1, 2000) > 0)
        {
            qCInfo(rartTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            flags = 0;  // If flags != 0, non-blocking mode is enabled.
            if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
            {
                qCWarning(rartTcpInput) << "Failed to set blocking socket";
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rartTcpInput) << "Unable to connect";
        }
#else  // (_WIN32_WINNT < 0x0600) \
    // poll API does not exist :-( \
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
            qCInfo(rartTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            flags = 0;  // If flags != 0, non-blocking mode is enabled.
            if (NO_ERROR != ioctlsocket(sfd, FIONBIO, &flags))
            {
                qCWarning(rartTcpInput) << "Failed to set blocking socket";
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rartTcpInputrartTcpInput) << "Unable to connect";
        }
#endif

#else  // not defined(_WIN32)
        long arg;
        if ((arg = fcntl(sfd, F_GETFL, NULL)) < 0)
        {
            qCWarning(rartTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
        }
        arg |= O_NONBLOCK;
        if (fcntl(sfd, F_SETFL, arg) < 0)
        {
            qCWarning(rartTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
        }

        struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
        qCInfo(rartTcpInput, "Trying to connect to: %s:%d", inet_ntoa(sa->sin_addr), m_port);
        ::connect(sfd, rp->ai_addr, rp->ai_addrlen);

        struct pollfd pfd;
        pfd.fd = sfd;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 2000) > 0)
        {
            qCInfo(rartTcpInput, "Connected to: %s:%d", inet_ntoa(sa->sin_addr), m_port);

            // set bloking mode again
            if ((arg = fcntl(sfd, F_GETFL, NULL)) < 0)
            {
                qCWarning(rartTcpInput, "Error fcntl(..., F_GETFL) (%s)", strerror(errno));
            }
            arg &= (~O_NONBLOCK);
            if (fcntl(sfd, F_SETFL, arg) < 0)
            {
                qCWarning(rartTcpInput, "Error fcntl(..., F_SETFL) (%s)", strerror(errno));
            }

            m_sock = sfd;
            break; /* Success */
        }
        else
        {  // -1 is error, 0 is timeout
            qCCritical(rartTcpInput) << "Unable to connect";
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
        qCCritical(rartTcpInput) << "Could not connect";
        return false;
    }

    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    // get information about the device
#if defined(_WIN32)
#if (_WIN32_WINNT >= 0x0600)
    struct pollfd fd;
    fd.fd = m_sock;
    fd.events = POLLIN;
    if (WSAPoll(&fd, 1, 2000) > 0)
    {
        ::recv(m_sock, (char *)&dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rartTcpInput) << "Unable to get RTL dongle infomation";
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
    if (::select(sock + 1, nullptr, &readFd, nullptr, &Timeout) > 0)
    {
        ::recv(sock, (char *)&dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rartTcpInput) << "Unable to get RTL dongle infomation";
        return false;
    }
#endif
#else
    struct pollfd fd;
    fd.fd = m_sock;
    fd.events = POLLIN;
    if (poll(&fd, 1, 2000) > 0)
    {
        ::recv(m_sock, (char *)&dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {  // -1 is error, 0 is timeout
        qCCritical(rartTcpInput) << "Unable to get RTL dongle infomation";
        return false;
    }
#endif

    // Convert the byte order
    dongleInfo.tunerType = ntohl(dongleInfo.tunerType);
    dongleInfo.tunerGainCount = ntohl(dongleInfo.tunerGainCount);
    if (dongleInfo.magic[0] == 'R' && dongleInfo.magic[1] == 'a' && dongleInfo.magic[2] == 'R' && dongleInfo.magic[3] == 'T')
    {
        m_deviceDescription.device.name = "rart_tcp";
        m_deviceDescription.device.model = "RaRT";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 16;
        m_deviceDescription.sample.containerBits = 16;
        m_deviceDescription.sample.channelContainer = "int16";

        // need to create worker, server is pushing samples
        m_worker = new RartTcpWorker(m_sock, this);
        connect(m_worker, &RartTcpWorker::dataReady, this, [=]() { emit tuned(m_frequency); }, Qt::QueuedConnection);
        connect(m_worker, &RartTcpWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
        connect(m_worker, &RartTcpWorker::finished, this, &RartTcpInput::onReadThreadStopped, Qt::QueuedConnection);
        connect(m_worker, &RartTcpWorker::finished, m_worker, &QObject::deleteLater);
        connect(m_worker, &RartTcpWorker::destroyed, this, [=]() { m_worker = nullptr; });
        m_worker->start();
        m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
        emit deviceReady();
    }
    else
    {
        qDebug() << "RART-TCP: \"RaRT\" magic key not found. Server not supported";
        return false;
    }
    return true;
}

void RartTcpInput::tune(uint32_t frequency)
{
    m_frequency = frequency;

    if ((m_frequency > 0) && (nullptr != m_worker))
    {  // Tune to new frequency
        sendCommand(RartTcpCommand::SET_FREQ, m_frequency * 1000);

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

void RartTcpInput::setTcpIp(const QString &address, int port)
{
    m_address = address;
    m_port = port;
}

void RartTcpInput::onReadThreadStopped()
{
    qCCritical(rartTcpInput) << "server disconnected.";

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

    emit error(InputDevice::ErrorCode::DeviceDisconnected);
}

void RartTcpInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        if (!m_worker->isRunning())
        {  // some problem in data input
            qCCritical(rartTcpInput) << "watchdog timeout";
            inputBuffer.fillDummy();
            emit error(InputDevice::ErrorCode::NoDataAvailable);
        }
    }
    else
    {
        m_watchdogTimer.stop();
    }
}

void RartTcpInput::startStopRecording(bool start)
{
    m_worker->startStopRecording(start);
}

void RartTcpInput::sendCommand(const RartTcpCommand &cmd, uint32_t param)
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

    ::send(m_sock, (char *)cmdBuffer, 5, 0);
}

RartTcpWorker::RartTcpWorker(SOCKET sock, QObject *parent) : QThread(parent)
{
    m_isRecording = false;
    m_enaCaptureIQ = false;
    m_sock = sock;
}

void RartTcpWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void RartTcpWorker::run()
{
    m_watchdogFlag = false;  // first callback sets it to true

    // read samples
    while (INVALID_SOCKET != m_sock)
    {
        size_t read = 0;
        do
        {
            ssize_t ret = ::recv(m_sock, (char *)m_bufferIQ + read, RARTTCP_CHUNK_SIZE - read, 0);
            if (0 == ret)
            {  // disconnected => finish thread operation
                qDebug() << "RART-TCP: socket disconnected";
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
                    qDebug() << "RART-TCP: socket disconnected";
                    goto worker_exit;
                }
                else if ((WSAECONNRESET == WSAGetLastError()) || (WSAEBADF == WSAGetLastError()))
                {  // disconnected => finish thread operation
                    qDebug() << "RART-TCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
                else
                {
                    qDebug() << "RART-TCP: socket read error:" << strerror(WSAGetLastError());
                }
#else
                if ((EAGAIN == errno) || (EINTR == errno))
                {
                    continue;
                }
                else if ((ECONNRESET == errno) || (EBADF == errno))
                {  // disconnected => finish thread operation
                    qDebug() << "RART-TCP: error: " << strerror(errno);
                    goto worker_exit;
                }
                else
                {
                    qDebug() << "RART-TCP: socket read error:" << strerror(errno);
                }
#endif
            }
            else
            {
                read += ret;
            }
        } while (RARTTCP_CHUNK_SIZE > read);

        // reset watchDog flag, timer sets it to true
        m_watchdogFlag = true;

        // full chunk is read at this point
        if (m_enaCaptureIQ)
        {  // process data
            if (m_captureStartCntr > 0)
            {  // reset procedure
                if (0 == --m_captureStartCntr)
                {  // restart finished

                    // clear buffer to avoid mixing of channels
                    inputBuffer.reset();

                    emit dataReady();
                }
                else
                {  // only reecord if recording
                    if (m_isRecording)
                    {
                        emit recordBuffer(m_bufferIQ, RARTTCP_CHUNK_SIZE);
                    }
                    else
                    { /* not recording */
                    }

                    // done
                    continue;
                }
            }
            processInputData(m_bufferIQ, RARTTCP_CHUNK_SIZE);
        }
    }

worker_exit:
    // single exit point
    return;
}

void RartTcpWorker::captureIQ(bool ena)
{
    if (ena)
    {
        m_captureStartCntr = RARTTCP_START_COUNTER_INIT;
    }
    m_enaCaptureIQ = ena;
}

bool RartTcpWorker::isRunning()
{
    bool flag = m_watchdogFlag;
    m_watchdogFlag = false;
    return flag;
}

void RartTcpWorker::processInputData(unsigned char *buf, uint32_t len)
{
    if (m_isRecording)
    {
        emit recordBuffer(buf, len);
    }

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    uint32_t numSamples = len >> 1;  // number of I and Q samples, one I or Q sample is 2 bytes (int16)
    if ((INPUT_FIFO_SIZE - count) < numSamples * sizeof(float))
    {
        qCWarning(rartTcpInput) << "dropping" << numSamples << "samples...";
        return;
    }

    // input samples are IQ = [int16_t int16_t]
    // going to transform them to [float float] = float _Complex

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    int16_t *inPtr = (int16_t *)buf;
    if (bytesTillEnd >= numSamples * sizeof(float))
    {
        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < numSamples; k++)
        {                                 // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }
        inputBuffer.head = (inputBuffer.head + numSamples * sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2;  // / sizeof(float);

        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < samplesTillEnd; ++k)
        {                                 // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k = 0; k < numSamples - samplesTillEnd; ++k)
        {                                 // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }
        inputBuffer.head = (numSamples - samplesTillEnd) * sizeof(float);
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + numSamples * sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
