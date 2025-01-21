/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

RtlTcpInput::RtlTcpInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDevice::Id::RTLTCP;

    m_gainList = nullptr;
    m_worker = nullptr;
    m_streamSocket = nullptr;
    m_controlSocket = nullptr;
    m_controlSocketEna = false;
    m_haveControlSocket = false;
    m_agcLevelMinFactorList = nullptr;
    m_agcLevelMax = RTLTCP_AGC_LEVEL_MAX_DEFAULT;
    m_agcLevelMin = 60;
    m_levelCalcCntr = 0;

    m_frequency = 0;
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
        m_worker->quit();
        m_worker->wait(2000);

        while (!m_worker->isFinished())
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

bool RtlTcpInput::openDevice(const QVariant &hwId)
{
    Q_UNUSED(hwId);

    if (nullptr != m_worker)
    {  // device already opened
        return true;
    }

    m_streamSocket = new QTcpSocket();
    m_streamSocket->connectToHost(QHostAddress(m_address), m_port);
    if (!m_streamSocket->waitForConnected(1500))
    {  // failed to connect
        qCCritical(rtlTcpInput) << "Unable to connect";
        return false;
    }

    struct
    {
        char magic[4];
        uint32_t tunerType;
        uint32_t tunerGainCount;
    } dongleInfo;

    // wait 1.5 sec to receive some data
    m_streamSocket->waitForReadyRead(1500);
    if (m_streamSocket->bytesAvailable() < (qint64)sizeof(dongleInfo))
    {  // error
        qCCritical(rtlTcpInput) << "Unable to get RTL dongle infomation";
        return false;
    }

    // get information about RTL stick
    m_streamSocket->read(reinterpret_cast<char *>(&dongleInfo), sizeof(dongleInfo));

    // Convert the byte order
    dongleInfo.tunerType = ntohl(dongleInfo.tunerType);
    dongleInfo.tunerGainCount = ntohl(dongleInfo.tunerGainCount);

    // Convert the byte order
    const int *gains = unknown_gains;
    if (dongleInfo.magic[0] == 'R' && dongleInfo.magic[1] == 'T' && dongleInfo.magic[2] == 'L' && dongleInfo.magic[3] == '0')
    {
        m_deviceDescription.device.name = "rtl_tcp";
        m_deviceDescription.device.model = "Generic RTL2832U";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 8;
        m_deviceDescription.sample.containerBits = 8;
        m_deviceDescription.sample.channelContainer = "uint8";

        int numGains = 0;
        switch (dongleInfo.tunerType)
        {
            case RTLSDR_TUNER_E4000:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_E4000";
                numGains = *(&e4k_gains_olddab + 1) - e4k_gains_olddab;
                if (dongleInfo.tunerGainCount == numGains)
                {
                    gains = e4k_gains_olddab;
                }
                else
                {
                    numGains = *(&e4k_gains + 1) - e4k_gains;
                    gains = e4k_gains;
                }
                m_deviceDescription.device.name += " [E4000]";
                m_deviceDescription.device.tuner = "E4000";
                break;
            case RTLSDR_TUNER_FC0012:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0012";
                gains = fc0012_gains;
                numGains = *(&fc0012_gains + 1) - fc0012_gains;
                m_deviceDescription.device.name += " [FC0012]";
                m_deviceDescription.device.tuner = "FC0012";
                break;
            case RTLSDR_TUNER_FC0013:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC0013";
                gains = fc0013_gains;
                numGains = *(&fc0013_gains + 1) - fc0013_gains;
                m_deviceDescription.device.name += " [FC0013]";
                m_deviceDescription.device.tuner = "FC0013";
                break;
            case RTLSDR_TUNER_FC2580:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_FC2580";
                gains = fc2580_gains;
                numGains = *(&fc2580_gains + 1) - fc2580_gains;
                m_deviceDescription.device.name += " [FC2580]";
                m_deviceDescription.device.tuner = "FC2580";
                break;
            case RTLSDR_TUNER_R820T:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R820T";
                numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
                if (dongleInfo.tunerGainCount == numGains)
                {
                    gains = r82xx_gains_olddab;
                }
                else
                {
                    numGains = *(&r82xx_gains + 1) - r82xx_gains;
                    gains = r82xx_gains;
                }
                m_deviceDescription.device.name += " [R820T]";
                m_deviceDescription.device.tuner = "R820T";
                break;
            case RTLSDR_TUNER_R828D:
                qCInfo(rtlTcpInput) << "RTLSDR_TUNER_R828D";
                numGains = *(&r82xx_gains_olddab + 1) - r82xx_gains_olddab;
                if (dongleInfo.tunerGainCount == numGains)
                {
                    gains = r82xx_gains_olddab;
                }
                else
                {
                    numGains = *(&r82xx_gains + 1) - r82xx_gains;
                    gains = r82xx_gains;
                }
                m_deviceDescription.device.name += " [R828D]";
                m_deviceDescription.device.tuner = "R828D";
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
    }
    else
    {  // this is connection to unknown server => lets try and cross the fingers
        qCWarning(rtlTcpInput) << "\"RTL0\" magic key not found. Unknown server.";

        m_deviceDescription.device.name = "TCP server";
        m_deviceDescription.device.model = "Unknown";
        m_deviceDescription.sample.sampleRate = 2048000;
        m_deviceDescription.sample.channelBits = 8;
        m_deviceDescription.sample.containerBits = 8;
        m_deviceDescription.sample.channelContainer = "uint8";

        dongleInfo.tunerGainCount = 0;
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
    for (int i = 1; i < m_gainList->count(); i++)
    {
        // up step + 0.5dB
        m_agcLevelMinFactorList->append(qPow(10.0, (m_gainList->at(i - 1) - m_gainList->at(i) - 5) / 200.0));
    }
    // last factor does not matter
    m_agcLevelMinFactorList->append(qPow(10.0, -5.0 / 20.0));

    // set automatic gain
    // setGainMode(RtlGainMode::Software);
    m_gainIdx = -1;

    // need to create worker, server is pushing samples
    m_worker = new RtlTcpWorker(m_streamSocket, this);
    connect(m_worker, &RtlTcpWorker::agcLevel, this, &RtlTcpInput::onAgcLevel, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::dataReady, this, [=]() { emit tuned(m_frequency); }, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::recordBuffer, this, &InputDevice::recordBuffer, Qt::DirectConnection);
    connect(m_worker, &RtlTcpWorker::finished, this, &RtlTcpInput::onReadThreadStopped, Qt::QueuedConnection);
    connect(m_worker, &RtlTcpWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &RtlTcpWorker::destroyed, this, [=]() { m_worker = nullptr; });

    m_streamSocket->moveToThread(m_worker);
    connect(m_worker, &RtlTcpWorker::finished, m_streamSocket, &QObject::deleteLater);

    // m_streamSocket belongs to worker thread now

    m_worker->start();

    m_worker->sendCommand(RtlTcpCommand::SET_SAMPLE_RATE, 2048000);

    m_watchdogTimer.start(1000 * INPUTDEVICE_WDOG_TIMEOUT_SEC);

    emit deviceReady();

    if (m_controlSocketEna)
    {  // truy to connect to control socket
        QTimer::singleShot(100, this, [this]() { initControlSocket(); });
    }

    return true;
}

void RtlTcpInput::tune(uint32_t frequency)
{
    m_frequency = frequency;

    if ((m_frequency > 0) && (nullptr != m_worker))
    {  // Tune to new frequency
        m_worker->sendCommand(RtlTcpCommand::SET_FREQ, m_frequency * 1000);

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
        m_worker->sendCommand(RtlTcpCommand::SET_GAIN_MODE, (RtlGainMode::Hardware != gainMode));
        setDAGC(RtlGainMode::Hardware == gainMode);  // enable for HW, disable otherwise

        m_gainMode = gainMode;

        // does nothing in (GainMode::Software != mode)
        resetAgc();
    }

    if (RtlGainMode::Manual == m_gainMode)
    {
        setGain(gainIdx);

        // always emit gain when switching mode to manual
        emit agcGain(m_gainList->at(gainIdx) * 0.1);
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

    // qDebug() << m_agcLevelMax << m_agcLevelMin;
}

void RtlTcpInput::setPPM(int ppm)
{
    if (ppm != m_ppm)
    {
        m_worker->sendCommand(RtlTcpCommand::SET_FREQ_CORR, ppm);
        qCInfo(rtlTcpInput) << "Frequency correction PPM:" << ppm;
        m_ppm = ppm;
        if (m_frequency != 0)
        {
            tune(m_frequency);
        }
    }
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
            m_worker->sendCommand(RtlTcpCommand::SET_GAIN_IDX, m_gainIdx);
            emit agcGain(m_gainList->at(m_gainIdx) * 0.1);
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
    m_worker->sendCommand(RtlTcpCommand::SET_AGC_MODE, ena);
}

void RtlTcpInput::onAgcLevel(float agcLevel)
{
    if (m_haveControlSocket && (RtlGainMode::Hardware != m_gainMode))
    {
        if (++m_levelCalcCntr > 2)
        {
            m_levelCalcCntr = 0;
            // qDebug() << agcLevel << m_deviceGain << 20 * std::log10f(agcLevel) - m_deviceGain - 46;
            emit rfLevel(m_20log10[static_cast<int>(std::roundf(agcLevel))] - m_deviceGain - 46, m_deviceGain);
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

    // fill buffer (artificially to avoid blocking of the DAB processing thread)
    inputBuffer.fillDummy();

    emit error(InputDevice::ErrorCode::DeviceDisconnected);
}

void RtlTcpInput::onWatchdogTimeout()
{
    if (nullptr != m_worker)
    {
        if (!m_worker->isRunning())
        {  // some problem in data input
            qCCritical(rtlTcpInput) << "Watchdog timeout";
            inputBuffer.fillDummy();
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
    m_worker->startStopRecording(start);
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

RtlTcpWorker::RtlTcpWorker(QTcpSocket *sock, QObject *parent) : QThread(parent)
{
    socket = sock;
    m_isRecording = false;
    m_enaCaptureIQ = false;
    m_commandFlag = false;
    m_quitFlag = false;
}

RtlTcpWorker::~RtlTcpWorker()
{
    qDebug() << Q_FUNC_INFO;
    qDeleteAll(m_cmdQueue);
}

void RtlTcpWorker::startStopRecording(bool ena)
{
    m_isRecording = ena;
}

void RtlTcpWorker::sendCommand(const RtlTcpCommand &cmd, uint32_t param)
{
    QByteArray *cmdBuffer = new QByteArray;
    cmdBuffer->append(uint8_t(cmd));
    cmdBuffer->append((param >> 24) & 0xFF);
    cmdBuffer->append((param >> 16) & 0xFF);
    cmdBuffer->append((param >> 8) & 0xFF);
    cmdBuffer->append(param & 0xFF);

    m_mutex.lock();
    m_cmdQueue.enqueue(cmdBuffer);
    m_commandFlag = true;
    m_mutex.unlock();
}

void RtlTcpWorker::run()
{
    m_dcI = 0.0;
    m_dcQ = 0.0;
    m_agcLevel = 0.0;
    m_watchdogFlag = false;  // first callback sets it to true

    while (!m_quitFlag)
    {
        size_t read = 0;
        do
        {
            if (!socket->waitForReadyRead(1000))
            {  //  error
                qCCritical(rtlTcpInput) << "socket read error:" << socket->errorString();
                return;
            }
            // data available
            read += socket->read(reinterpret_cast<char *>(m_bufferIQ) + read, RTLTCP_CHUNK_SIZE - read);
        } while (RTLTCP_CHUNK_SIZE > read);

        // reset watchDog flag, timer sets it to true
        m_watchdogFlag = true;

        if (m_commandFlag)
        {  // process commands
            m_mutex.lock();
            m_commandFlag = false;
            while (m_cmdQueue.count() > 0)
            {
                QByteArray *cmd = m_cmdQueue.dequeue();
                socket->write(*cmd);
                delete cmd;
            }
            m_mutex.unlock();
        }

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
                    // m_agcLevel = 0.0;

                    emit dataReady();
                }
                else
                {  // only reecord if recording
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

    if ((INPUT_FIFO_SIZE - count) < len * sizeof(float))
    {
        qCWarning(rtlTcpInput) << "Dropping" << len << "bytes...";
        return;
    }

    // input samples are IQ = [uint8_t uint8_t]
    // going to transform them to [float float] = float _Complex
    // on uint8_t will be transformed to one float

    // there is enough room in buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;
    uint8_t *inPtr = buf;
    if (bytesTillEnd >= len * sizeof(float))
    {
        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < len; k++)
        {  // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else                                           // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128;  // I or Q

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
            {  // Q
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
        inputBuffer.head = (inputBuffer.head + len * sizeof(float));
    }
    else
    {
        Q_ASSERT(sizeof(float) == 4);
        uint64_t samplesTillEnd = bytesTillEnd >> 2;  // / sizeof(float);

        float *outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
        for (uint64_t k = 0; k < samplesTillEnd; ++k)
        {  // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else                                           // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128;  // I or Q

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
            {  // Q
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
        for (uint64_t k = 0; k < len - samplesTillEnd; ++k)
        {  // convert to float
#if ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            *outPtr++ = float(*inPtr++ - 128);  // I or Q
#else                                           // ((RTLTCP_DOC_ENABLE == 0) && ((RTLTCP_AGC_ENABLE == 0)))
            int_fast8_t tmp = *inPtr++ - 128;  // I or Q

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
            {  // Q
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
    if (m_agcLevelCntr++ > 0)
    {
        m_agcLevelCntr = 0;
        emit agcLevel(agcLev);
    }
#endif

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count + len * sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
