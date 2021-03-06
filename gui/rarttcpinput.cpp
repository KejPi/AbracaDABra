#include <QDir>
#include <QDebug>

#include "rarttcpinput.h"

void rarttcpCb(unsigned char *buf, uint32_t len, void *ctx);

#if defined(_WIN32)
class SocketInitialiseWrapper {
public:
    SocketInitialiseWrapper() {
        WSADATA wsa;
        qDebug() << "RTL_TCP: Initialising Winsock...";

        if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
            qDebug() << "RTL_TCP: Winsock init failed. Error Code:" << WSAGetLastError();
    }

    ~SocketInitialiseWrapper() {
        WSACleanup();
    }

    SocketInitialiseWrapper(SocketInitialiseWrapper&) = delete;
    SocketInitialiseWrapper& operator=(SocketInitialiseWrapper&) = delete;
};

static SocketInitialiseWrapper socketInitialiseWrapper;
#endif

RartTcpInput::RartTcpInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::RARTTCP;

    deviceUnplugged = true;
    dumpFile = nullptr;
    worker = nullptr;
    frequency = 0;
    sock = INVALID_SOCKET;
    address = "127.0.0.1";
    port = 1235;

#if (RARTTCP_WDOG_ENABLE)
    connect(&watchDogTimer, &QTimer::timeout, this, &RartTcpInput::watchDogTimeout);
#endif
}

RartTcpInput::~RartTcpInput()
{
    //qDebug() << Q_FUNC_INFO;
    if (!deviceUnplugged)
    {
        // need to end worker thread and close socket
        if (nullptr != worker)
        {
            worker->catureIQ(false);

            // close socket
#if defined(_WIN32)
            closesocket(sock);
#else
            ::close(sock);
#endif
            sock = INVALID_SOCKET;

            worker->wait(2000);

            while  (!worker->isFinished())
            {
                qDebug() << "Worker thread not finished after timeout - this should not happen :-(";

                // reset buffer - and tell the thread it is empty - buffer will be reset in any case
                pthread_mutex_lock(&inputBuffer.countMutex);
                inputBuffer.count = 0;
                pthread_cond_signal(&inputBuffer.countCondition);
                pthread_mutex_unlock(&inputBuffer.countMutex);
                worker->wait(2000);
            }
        }
    }
}

bool RartTcpInput::openDevice()
{
    if (!deviceUnplugged)
    {   // device already opened
        return true;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    QString port_str = QString().number(port);

    struct addrinfo *result;
    int s = getaddrinfo(address.toLatin1(), port_str.toLatin1(), &hints, &result);
    if (s != 0)
    {
#if defined(_WIN32)
        qDebug() << "RARTTCP: getaddrinfo error:" << gai_strerrorA(s);
#else
        qDebug() << "RARTTCP: getaddrinfo error:" << gai_strerror(s);
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

        struct sockaddr_in *sa = (struct sockaddr_in *) rp->ai_addr;
        qDebug() << "RARTTCP: Trying to connect to:" << inet_ntoa(sa->sin_addr);
        if (0 == ::connect(sfd, rp->ai_addr, rp->ai_addrlen))
        {   // connected
            sock = sfd;
            break; /* Success */
        }

#if defined(_WIN32)
        closesocket(sfd);
#else
        ::close(sfd);
#endif
    }

    if (NULL == rp)
    {   /* No address succeeded */
        qDebug() << "RARTTCP: Could not connect";
        return false;
    }

    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    // get information about RTL stick
    // get information about RTL stick
#if defined(_WIN32)
#if (_WIN32_WINNT >= 0x0600)
    struct pollfd  fd;
    fd.fd = sock;
    fd.events = POLLIN;
    if (WSAPoll(&fd, 1, 2000) > 0)
    {
        ::recv(sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qDebug() << "Unable to get RART infomation";
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
        qDebug() << "Unable to get RART infomation";
        return false;
    }
#endif
#else
    struct pollfd fd;
    fd.fd = sock;
    fd.events = POLLIN;
    if (poll(&fd, 1, 2000) > 0)
    {
        ::recv(sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qDebug() << "Unable to get RaRT infomation";
        return false;
    }
#endif

    // Convert the byte order
    dongleInfo.tunerType = ntohl(dongleInfo.tunerType);
    dongleInfo.tunerGainCount = ntohl(dongleInfo.tunerGainCount);

    if(dongleInfo.magic[0] == 'R' &&
        dongleInfo.magic[1] == 'a' &&
        dongleInfo.magic[2] == 'R' &&
        dongleInfo.magic[3] == 'T')
    {
#if 0 //todo tuner detection
        switch(dongleInfo.tunerType)
        {
        case RTLSDR_TUNER_E4000:
        case RTLSDR_TUNER_UNKNOWN:
        default:
        {
            qDebug() << "RTLSDR_TUNER_UNKNOWN";
            dongleInfo.tunerGainCount = 0;
        }
        }
#endif
        qDebug() << "Found RaRT TCP server";

        // need to create worker, server is pushing samples
        worker = new RartTcpWorker(sock, this);
        connect(worker, &RartTcpWorker::dumpedBytes, this, &InputDevice::dumpedBytes, Qt::QueuedConnection);
        connect(worker, &RartTcpWorker::finished, this, &RartTcpInput::readThreadStopped, Qt::QueuedConnection);
        connect(worker, &RartTcpWorker::finished, worker, &QObject::deleteLater);
        worker->start();        
#if (RARTTCP_WDOG_ENABLE)
        watchDogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
#endif
        // device connected
        deviceUnplugged = false;

        emit deviceReady();
    }
    else
    {
        qDebug() << "RART_TCP: \"RaRT\" magic key not found. Server not supported";
        return false;
    }    
    return true;
}

void RartTcpInput::tune(uint32_t freq)
{
    frequency = freq;
    if ((frequency > 0) && (!deviceUnplugged))
    {
        run();
    }
    else
    {
        stop();
    }
    emit tuned(frequency);
}

void RartTcpInput::setTcpIp(const QString &addr, int p)
{
    address = addr;
    port = p;
}

void RartTcpInput::run()
{
    if (frequency != 0)
    {   // Tune to new frequency
        sendCommand(RartTcpCommand::SET_FREQ, frequency*1000);

        if (nullptr != worker)
        {
            worker->catureIQ(true);
        }
    }
}

void RartTcpInput::stop()
{
    if (nullptr != worker)
    {
        worker->catureIQ(false);
    }
}

void RartTcpInput::readThreadStopped()
{
    qDebug() << "RARTTCP server disconnected.";

    // close socket
#if defined(_WIN32)
    closesocket(sock);
#else
    ::close(sock);
#endif
    sock = INVALID_SOCKET;

#if (RARTTCP_WDOG_ENABLE)
    watchDogTimer.stop();
#endif

    deviceUnplugged = true;

    // fill buffer (artificially to avoid blocking of the DAB processing thread)
    inputBuffer.fillDummy();

    emit error(InputDeviceErrorCode::DeviceDisconnected);
}

#if (RARTTCP_WDOG_ENABLE)
void RartTcpInput::watchDogTimeout()
{
    if (nullptr != worker)
    {
        if (!deviceUnplugged)
        {
            bool isRunning = worker->isRunning();
            if (!isRunning)
            {  // some problem in data input
                qDebug() << Q_FUNC_INFO << "watchdog timeout";
                inputBuffer.fillDummy();
                emit error(InputDeviceErrorCode::NoDataAvailable);
            }
        }
    }
    else
    {
        watchDogTimer.stop();
    }
}
#endif

void RartTcpInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpingToFile(true, 4);
    }
}

void RartTcpInput::stopDumpToFile()
{
    worker->dumpToFileStop();

    fclose(dumpFile);

    emit dumpingToFile(false);
}

void RartTcpInput::sendCommand(const RartTcpCommand & cmd, uint32_t param)
{
    if (deviceUnplugged)
    {
        return;
    }

    uint8_t cmdBuffer[5];

    cmdBuffer[0] = uint8_t(cmd);
    cmdBuffer[4] = param & 0xFF;
    cmdBuffer[3] = (param >> 8) & 0xFF;
    cmdBuffer[2] = (param >> 16) & 0xFF;
    cmdBuffer[1] = (param >> 24) & 0xFF;

    ::send(sock, (char *) cmdBuffer, 5, 0);
}

RartTcpWorker::RartTcpWorker(SOCKET socket, QObject *parent) : QThread(parent)
{
    enaDumpToFile = false;
    enaCaptureIQ = false;
    dumpFile = nullptr;
    sock = socket;
}

void RartTcpWorker::run()
{
    //qDebug() << "RartTcpWorker thread start" << QThread::currentThreadId() << sock;

    wdogIsRunningFlag = false;  // first callback sets it to true

    // read samples
    while (INVALID_SOCKET != sock)
    {
        size_t read = 0;
        do
        {
            ssize_t ret = ::recv(sock, (char *) bufferIQ+read, RARTTCP_CHUNK_SIZE - read, 0);
            if (0 == ret)
            {   // disconnected => finish thread operation
                qDebug() << Q_FUNC_INFO << "RARTTCP: socket disconnected";
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
                    qDebug() << Q_FUNC_INFO << "RARTTCP: socket disconnected";
                    goto worker_exit;
                }
                else if ((WSAECONNRESET == WSAGetLastError()) || (WSAEBADF == WSAGetLastError()))
                {   // disconnected => finish thread operation
                    qDebug() << "RARTTCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
                else
                {
                    qDebug() << "RARTTCP: socket read error:" << strerror(WSAGetLastError());
                }
#else
                if ((EAGAIN == errno) || (EINTR == errno))
                {
                    continue;
                }
                else if ((ECONNRESET == errno) || (EBADF == errno))
                {   // disconnected => finish thread operation
                    qDebug() << Q_FUNC_INFO << "error: " << strerror(errno);
                    goto worker_exit;
                }
                else
                {
                    qDebug() << "RARTTCP: socket read error:" << strerror(errno);
                }
#endif
            }
            else
            {
                read += ret;
            }
        } while (RARTTCP_CHUNK_SIZE > read);

#if (RARTTCP_WDOG_ENABLE)
        // reset watchDog flag, timer sets it to true
        wdogIsRunningFlag = true;
#endif

        // full chunk is read at this point
        if (enaCaptureIQ)
        {   // process data
            if (captureStartCntr > 0)
            {   // clear buffer to avoid mixing of channels
                captureStartCntr--;
                memset(bufferIQ, 0, RARTTCP_CHUNK_SIZE);
            }
            rarttcpCb(bufferIQ, RARTTCP_CHUNK_SIZE, this);
        }
    }

worker_exit:
    // single exit point
    //qDebug() << "RartTcpWorker thread end" << QThread::currentThreadId();
    return;
}

void RartTcpWorker::catureIQ(bool ena)
{
    if (ena)
    {
        inputBuffer.reset();
        captureStartCntr = RARTTCP_START_COUNTER_INIT;
    }
    enaCaptureIQ = ena;
}

void RartTcpWorker::dumpToFileStart(FILE * f)
{
    fileMutex.lock();
    dumpFile = f;
    enaDumpToFile = true;
    fileMutex.unlock();
}

void RartTcpWorker::dumpToFileStop()
{
    fileMutex.lock();
    enaDumpToFile = false;
    dumpFile = nullptr;
    fileMutex.unlock();
}

void RartTcpWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

bool RartTcpWorker::isRunning()
{
    bool flag = wdogIsRunningFlag;
    wdogIsRunningFlag = false;
    return flag;
}

void rarttcpCb(unsigned char *buf, uint32_t len, void * ctx)
{
    RartTcpWorker * rartTcpWorker = static_cast<RartTcpWorker *>(ctx);
    if (rartTcpWorker->isDumpingIQ())
    {
        rartTcpWorker->dumpBuffer(buf, len);
    }

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    uint32_t numSamples = len >> 1;  // number of I and Q samples, one I or Q sample is 2 bytes (int16)
    if ((INPUT_FIFO_SIZE - count) < numSamples*sizeof(float))
    {
        qDebug() << Q_FUNC_INFO << "dropping" << numSamples << "samples...";
        return;
    }

    // input samples are IQ = [int16_t int16_t]
    // going to transform them to [float float] = float _Complex

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    int16_t * inPtr = (int16_t *) buf;
    if (bytesTillEnd >= numSamples*sizeof(float))
    {
        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<numSamples; k++)
        {   // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }
        inputBuffer.head = (inputBuffer.head + numSamples*sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2; // / sizeof(float);

        float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k=0; k<samplesTillEnd; ++k)
        {   // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }

        outPtr = (float *)(inputBuffer.buffer);
        for (uint64_t k=0; k<numSamples-samplesTillEnd; ++k)
        {   // convert to float
            *outPtr++ = float(*inPtr++);  // I or Q
        }
        inputBuffer.head = (numSamples-samplesTillEnd)*sizeof(float);
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + numSamples*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

