#include <QDir>
#include <QDebug>

#include "rtltcpinput.h"

void rtltcpCb(unsigned char *buf, uint32_t len, void *ctx);

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
const int RtlTcpInput::unknown_gains[] = { 0 /* no gain values */ };

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

RtlTcpInput::RtlTcpInput(QObject *parent) : InputDevice(parent)
{
    id = InputDeviceId::RTLTCP;

    deviceUnplugged = true;
    gainList = nullptr;
    dumpFile = nullptr;
    worker = nullptr;
    frequency = 0;
    sock = INVALID_SOCKET;
    address = "127.0.0.1";
    port = 1234;

#if (RTLTCP_WDOG_ENABLE)
    connect(&watchDogTimer, &QTimer::timeout, this, &RtlTcpInput::watchDogTimeout);
#endif
}

RtlTcpInput::~RtlTcpInput()
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
    if (nullptr != gainList)
    {
        delete gainList;
    }
}

bool RtlTcpInput::openDevice()
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

    QString portStr = QString().number(port);

    struct addrinfo *result;
    int s = getaddrinfo(address.toLatin1(), portStr.toLatin1(), &hints, &result);
    if (s != 0)
    {
#if defined(_WIN32)
        qDebug() << "RTLTCP: getaddrinfo error:" << gai_strerrorA(s);
#else
        qDebug() << "RTLTCP: getaddrinfo error:" << gai_strerror(s);
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
        qDebug() << "RTLTCP: Trying to connect to:" << inet_ntoa(sa->sin_addr);
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
        qDebug() << "RTLSDR: Could not connect";
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
    fd.fd = sock;
    fd.events = POLLIN;
    if (WSAPoll(&fd, 1, 2000) > 0)
    {
        ::recv(sock, (char *) &dongleInfo, sizeof(dongleInfo), 0);
    }
    else
    {   // -1 is error, 0 is timeout
        qDebug() << "Unable to get RTL dongle infomation";
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
        qDebug() << "Unable to get RTL dongle infomation";
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
        qDebug() << "Unable to get RTL dongle infomation";
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
        const int * gains = unknown_gains;
        int numGains = 0;
        switch(dongleInfo.tunerType)
        {
        case RTLSDR_TUNER_E4000:
            qDebug() << "RTLSDR_TUNER_E4000";
            gains = e4k_gains;
            numGains = *(&e4k_gains + 1) - e4k_gains;
            break;
        case RTLSDR_TUNER_FC0012:
            qDebug() << "RTLSDR_TUNER_FC0012";
            gains = fc0012_gains;
            numGains = *(&fc0012_gains + 1) - fc0012_gains;
            break;
        case RTLSDR_TUNER_FC0013:
            qDebug() << "RTLSDR_TUNER_FC0013";
            gains = fc0013_gains;
            numGains = *(&fc0013_gains + 1) - fc0013_gains;
            break;
        case RTLSDR_TUNER_FC2580:
            qDebug() << "RTLSDR_TUNER_FC2580";
            gains = fc2580_gains;
            numGains = *(&fc2580_gains + 1) - fc2580_gains;
            break;
        case RTLSDR_TUNER_R820T:
            qDebug() << "RTLSDR_TUNER_R820T";
            gains = r82xx_gains;
            numGains = *(&r82xx_gains + 1) - r82xx_gains;
            break;
        case RTLSDR_TUNER_R828D:
            qDebug() << "RTLSDR_TUNER_R828D";
            gains = r82xx_gains;
            numGains = *(&r82xx_gains + 1) - r82xx_gains;
            break;
        case RTLSDR_TUNER_UNKNOWN:
        default:
        {
            qDebug() << "RTLSDR_TUNER_UNKNOWN";
            dongleInfo.tunerGainCount = 0;
        }
        }

        if (dongleInfo.tunerGainCount != numGains)
        {
            qDebug() << "RTL_TCP WARNING: unexpected number of gain values reported by server" << dongleInfo.tunerGainCount;
            if (dongleInfo.tunerGainCount > numGains)
            {
                dongleInfo.tunerGainCount = numGains;
            }
        }


        if (nullptr != gainList)
        {
            delete gainList;
        }
        gainList = new QList<int>();
        for (int i = 0; i < dongleInfo.tunerGainCount; i++) {
            //qDebug() << "RTL_TCP: gain " << (gains[i] / 10.0);
            gainList->append(gains[i]);
        }
        emit gainListAvailable(gainList);

        // set sample rate
        sendCommand(RtlTcpCommand::SET_SAMPLE_RATE, 2048000);

        // set automatic gain
        setGainMode(GainMode::Software);

        // need to create worker, server is pushing samples
        worker = new RtlTcpWorker(sock, this);
        connect(worker, &RtlTcpWorker::agcLevel, this, &RtlTcpInput::updateAgc, Qt::QueuedConnection);
        connect(worker, &RtlTcpWorker::finished, this, &RtlTcpInput::readThreadStopped, Qt::QueuedConnection);
        connect(worker, &RtlTcpWorker::finished, worker, &QObject::deleteLater);
        worker->start();
#if (RTLTCP_WDOG_ENABLE)
        watchDogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);
#endif
        // device connected
        deviceUnplugged = false;

        emit deviceReady();
    }
    else
    {
        qDebug() << "RTL_TCP: \"RTL0\" magic key not found. Server not supported";
        return false;
    }

    return true;
}

void RtlTcpInput::tune(uint32_t freq)
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

void RtlTcpInput::setTcpIp(const QString &addr, int p)
{
    address = addr;
    port = p;
}

void RtlTcpInput::run()
{
    if (frequency != 0)
    {   // Tune to new frequency
        sendCommand(RtlTcpCommand::SET_FREQ, frequency*1000);

        // does nothing if not SW AGC
        resetAgc();

        if (nullptr != worker)
        {
            worker->catureIQ(true);
        }
    }
}

void RtlTcpInput::stop()
{
    if (nullptr != worker)
    {        
        worker->catureIQ(false);        
    }
}

void RtlTcpInput::setGainMode(GainMode mode, int gainIdx)
{
    if (mode != gainMode)
    {
        // set automatic gain 0 or manual 1
        sendCommand(RtlTcpCommand::SET_GAIN_MODE, (GainMode::Hardware != mode));

        gainMode = mode;
    }

    if (GainMode::Manual == gainMode)
    {
        setGain(gainIdx);
    }

    if (GainMode::Hardware == gainMode)
    {   // signalize that gain is not available
        emit agcGain(INPUTDEVICE_AGC_GAIN_NA);
    }
    // does nothing in (GainMode::Software != mode)
    resetAgc();
}

void RtlTcpInput::setGain(int gIdx)
{
    // force index validity
    if (gIdx < 0)
    {
        gIdx = 0;
    }
    if (gIdx >= gainList->size())
    {
        gIdx = gainList->size() - 1;
    }

    if (gIdx != gainIdx)
    {
        gainIdx = gIdx;
        sendCommand(RtlTcpCommand::SET_GAIN_IDX, gainIdx);
        //qDebug() << "RTL_TCP: Tuner gain set to" << gainList->at(gainIdx)/10.0;
        emit agcGain(gainList->at(gainIdx));
    }
}

void RtlTcpInput::resetAgc()
{
    if (GainMode::Software == gainMode)
    {
        setGain(gainList->size() >> 1);
    }
}

void RtlTcpInput::setDAGC(bool ena)
{
    sendCommand(RtlTcpCommand::SET_AGC_MODE, ena);
    //qDebug() << "RTL_TCP: DAGC enable:" << ena;
}

void RtlTcpInput::updateAgc(float level, int maxVal)
{
    if (GainMode::Software == gainMode)
    {
        // AGC correction
        if (maxVal >= 127)
        {
            setGain(gainIdx-1);
        }
        else if ((level < 50) && (maxVal < 100))
        {  // (maxVal < 100) is required to avoid toggling => change gain only if there is some headroom
            // this could be problem on E4000 tuner with big AGC gain steps
            setGain(gainIdx+1);
        }
    }
}

void RtlTcpInput::readThreadStopped()
{
    qDebug() << "RTL-TCP server disconnected.";

    // close socket
#if defined(_WIN32)
    closesocket(sock);
#else
    ::close(sock);
#endif
    sock = INVALID_SOCKET;

#if (RTLTCP_WDOG_ENABLE)
    watchDogTimer.stop();
#endif

    deviceUnplugged = true;

    // fill buffer (artificially to avoid blocking of the DAB processing thread)
    inputBuffer.fillDummy();

    emit error(InputDeviceErrorCode::DeviceDisconnected);
}

#if (RTLTCP_WDOG_ENABLE)
void RtlTcpInput::watchDogTimeout()
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

void RtlTcpInput::startDumpToFile(const QString & filename)
{
    dumpFile = fopen(QDir::toNativeSeparators(filename).toUtf8().data(), "wb");
    if (nullptr != dumpFile)
    {
        worker->dumpToFileStart(dumpFile);
        emit dumpingToFile(true);
    }
}

void RtlTcpInput::stopDumpToFile()
{
    worker->dumpToFileStop();

    fclose(dumpFile);

    emit dumpingToFile(false);
}

void RtlTcpInput::sendCommand(const RtlTcpCommand & cmd, uint32_t param)
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

RtlTcpWorker::RtlTcpWorker(SOCKET socket, QObject *parent) : QThread(parent)
{
    enaDumpToFile = false;
    enaCaptureIQ = false;
    dumpFile = nullptr;
    sock = socket;
}

void RtlTcpWorker::run()
{
    //qDebug() << "RtlTcpWorker thread start" << QThread::currentThreadId() << sock;

    dcI = 0.0;
    dcQ = 0.0;
    agcLev = 0.0;
    wdogIsRunningFlag = false;  // first callback sets it to true

    // read samples
    while (INVALID_SOCKET != sock)
    {
        size_t read = 0;
        do
        {
            ssize_t ret = ::recv(sock, (char *) bufferIQ+read, RTLTCP_CHUNK_SIZE - read, 0);
            if (0 == ret)
            {   // disconnected => finish thread operation
                qDebug() << Q_FUNC_INFO << "RTLTCP: socket disconnected";
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
                    qDebug() << Q_FUNC_INFO << "RTLTCP: socket disconnected";
                    goto worker_exit;
                }
                else if ((WSAECONNRESET == WSAGetLastError()) || (WSAEBADF == WSAGetLastError()))
                {   // disconnected => finish thread operation
                    qDebug() << "RTLTCP: socket read error:" << strerror(WSAGetLastError());
                    goto worker_exit;
                }
                else
                {
                    qDebug() << "RTLTCP: socket read error:" << strerror(WSAGetLastError());
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
                    qDebug() << "RTLTCP: socket read error:" << strerror(errno);
                }
#endif
            }
            else
            {
                read += ret;
            }
        } while (RTLTCP_CHUNK_SIZE > read);

#if (RTLTCP_WDOG_ENABLE)
        // reset watchDog flag, timer sets it to true
        wdogIsRunningFlag = true;
#endif

        // full chunk is read at this point
        if (enaCaptureIQ)
        {   // process data
            if (captureStartCntr > 0)
            {   // clear buffer to avoid mixing of channels
                captureStartCntr--;
                memset(bufferIQ, 0, RTLTCP_CHUNK_SIZE);
            }
            rtltcpCb(bufferIQ, RTLTCP_CHUNK_SIZE, this);
        }
    }

worker_exit:    
    // single exit point
    // qDebug() << "RtlTcpWorker thread end" << QThread::currentThreadId();
    return;
}

void RtlTcpWorker::catureIQ(bool ena)
{
    if (ena)
    {
        inputBuffer.reset();
        captureStartCntr = RTLTCP_START_COUNTER_INIT;
    }
    enaCaptureIQ = ena;
}

void RtlTcpWorker::dumpToFileStart(FILE * f)
{
    fileMutex.lock();
    dumpFile = f;
    enaDumpToFile = true;
    fileMutex.unlock();
}

void RtlTcpWorker::dumpToFileStop()
{
    fileMutex.lock();
    enaDumpToFile = false;
    dumpFile = nullptr;
    fileMutex.unlock();
}

void RtlTcpWorker::dumpBuffer(unsigned char *buf, uint32_t len)
{
    fileMutex.lock();
    if (nullptr != dumpFile)
    {
        ssize_t bytes = fwrite(buf, 1, len, dumpFile);
        emit dumpedBytes(bytes);
    }
    fileMutex.unlock();
}

bool RtlTcpWorker::isRunning()
{
    bool flag = wdogIsRunningFlag;
    wdogIsRunningFlag = false;
    return flag;
}

void rtltcpCb(unsigned char *buf, uint32_t len, void * ctx)
{
#if (RTLTCP_DOC_ENABLE > 0)
    int_fast32_t sumI = 0;
    int_fast32_t sumQ = 0;
#define DC_C 0.05
#endif

#if (RTLTCP_AGC_ENABLE > 0)
    int maxVal = 0;
#define LEV_CATT 0.1
#define LEV_CREL 0.0001
#endif

    RtlTcpWorker * rtlTcpWorker = static_cast<RtlTcpWorker *>(ctx);
    if (rtlTcpWorker->isDumpingIQ())
    {
        rtlTcpWorker->dumpBuffer(buf, len);
    }

    // retrieving memories
#if (RTLTCP_DOC_ENABLE > 0)
    float dcI = rtlTcpWorker->dcI;
    float dcQ = rtlTcpWorker->dcQ;
#endif
#if (RTLTCP_AGC_ENABLE > 0)
    float agcLev = rtlTcpWorker->agcLev;
#endif

    // len is number of I and Q samples
    // get FIFO space
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    Q_ASSERT(count <= INPUT_FIFO_SIZE);

    pthread_mutex_unlock(&inputBuffer.countMutex);

    if ((INPUT_FIFO_SIZE - count) < len*sizeof(float))
    {
        qDebug() << Q_FUNC_INFO << "dropping" << len << "bytes...";
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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

            // catch maximum value (used to avoid overflow)
            if (absTmp > maxVal)
            {
                maxVal = absTmp;
            }

            // calculate signal level (rectifier, fast attack slow release)
            float c = LEV_CREL;
            if (absTmp > agcLev)
            {
                c = LEV_CATT;
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
#endif  // RTLTCP_DOC_ENABLE
#endif  // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
        }
        inputBuffer.head = (len-samplesTillEnd)*sizeof(float);

#if RTLTCP_DOC_ENABLE
        //qDebug() << dcI << dcQ;
#endif
#if (RTLTCP_AGC_ENABLE > 0)
        //qDebug() << agcLev << maxVal;
#endif

    }

#if (RTLTCP_DOC_ENABLE > 0)
    // calculate correction values for next input buffer
    rtlTcpWorker->dcI = sumI * DC_C / (len >> 1) + dcI - DC_C * dcI;
    rtlTcpWorker->dcQ = sumQ * DC_C / (len >> 1) + dcQ - DC_C * dcQ;
#endif

#if (RTLTCP_AGC_ENABLE > 0)
    // store memory
    rtlTcpWorker->agcLev = agcLev;

    rtlTcpWorker->emitAgcLevel(agcLev, maxVal);
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

