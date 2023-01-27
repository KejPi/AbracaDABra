#include <QDebug>
#include <QFile>
#include <QDomDocument>
#include <complex>
#include "rawfileinput.h"

RawFileInput::RawFileInput(QObject *parent) : InputDevice(parent)
{
    m_deviceDescription.id = InputDeviceId::RAWFILE;

    m_worker = nullptr;
    m_inputTimer = nullptr;
    m_inputFile = nullptr;
    m_sampleFormat = RawFileInputFormat::SAMPLE_FORMAT_U8;    
}

RawFileInput::~RawFileInput()
{
    if (nullptr != m_worker)
    {
        m_worker->stop();
        m_worker->quit();
        m_worker->wait();
    }

    if (nullptr != m_inputFile)
    {
        m_inputFile->close();
        delete m_inputFile;
    }   
}

bool RawFileInput::openDevice()
{
    if (nullptr != m_inputFile)
    {
        m_inputFile->close();
        delete m_inputFile;
    }
    m_inputFile = new QFile(m_fileName);
    if (!m_inputFile->open(QIODevice::ReadOnly))
    {
        qDebug() << "RAW-FILE: Unable to open file: " << m_fileName;
        delete m_inputFile;
        m_inputFile = nullptr;

        return false;
    }

    // check XML header
    QDataStream in(m_inputFile);
    QByteArray xml;
    int idx = 0;
    do
    {   // read no more than RAWFILEINPUT_XML_PADDING bytes
        char ch;
        in.readRawData(&ch, 1);
        if (0 == ch)
        {   // zero indicates header padding bytes
            break;
        }
        xml.append(ch);
    } while (idx++ < RAWFILEINPUT_XML_PADDING);

    if (idx < RAWFILEINPUT_XML_PADDING)
    {   // try to parse header
        parseXmlHeader(xml);
    }
    else
    {   // not found
        m_deviceDescription.rawFile.hasXmlHeader = false;
    }

    if (!m_deviceDescription.rawFile.hasXmlHeader)
    {   // header was not correctly parsed or not found
        m_inputFile->seek(0);
    }

    emit deviceReady();

    return true;
}

void RawFileInput::setFile(const QString & fileName, const RawFileInputFormat &sampleFormat)
{
    m_fileName = fileName;
    setFileFormat(sampleFormat);
}

void RawFileInput::setFileFormat(const RawFileInputFormat &sampleFormat)
{
    m_sampleFormat = sampleFormat;
}

void RawFileInput::tune(uint32_t freq)
{
    stop();    
    rewind();

    // Reset buffer here - worker thread it not running, DAB waits for new data
    inputBuffer.reset();

    if (0 != freq)
    {
        m_worker = new RawFileWorker(m_inputFile, m_sampleFormat, this);
        connect(m_worker, &RawFileWorker::endOfFile, this, &RawFileInput::onEndOfFile, Qt::QueuedConnection);
        connect(m_worker, &RawFileWorker::finished, m_worker, &QObject::deleteLater);
        m_worker->start();

        m_inputTimer = new QTimer(this);
        connect(m_inputTimer, &QTimer::timeout, m_worker, &RawFileWorker::trigger);
        m_inputTimer->start(INPUT_CHUNK_MS);
    }
    emit tuned(freq);
}

void RawFileInput::rewind()
{
    if (nullptr == m_worker)
    {   // avoid multiple access - if thread is running, then no action
        // go to file beginning
        if (nullptr != m_inputFile)
        {
            m_inputFile->seek(m_deviceDescription.rawFile.hasXmlHeader ? (RAWFILEINPUT_XML_PADDING) : (0));
        }
    }
}

void RawFileInput::parseXmlHeader(const QByteArray &xml)
{
    QDomDocument xmlHeader;
    if (xmlHeader.setContent(xml))
    {
        m_deviceDescription.rawFile.hasXmlHeader = true;

        // print out the element names of all elements that are direct children
        // of the outermost element.
        QDomElement docElem = xmlHeader.documentElement();

        QDomNode sdrNode = docElem.firstChild();
        while (!sdrNode.isNull())
        {
            QDomElement sdrElement = sdrNode.toElement(); // try to convert the node to an element
            if (!sdrElement.isNull())
            {
                if ("Recorder" == sdrElement.tagName())
                {
                    m_deviceDescription.rawFile.recorder = QString("%1 (%2)")
                                                               .arg(sdrElement.attribute("Name", "N/A"),
                                                                    sdrElement.attribute("Version", "N/A"));
                }
                else if ("Device" == sdrElement.tagName())
                {
                    m_deviceDescription.device.name = sdrElement.attribute("Name", "N/A");
                    m_deviceDescription.device.model = sdrElement.attribute("Model", "N/A");
                }
                else if ("Time" == sdrElement.tagName())
                {
                    m_deviceDescription.rawFile.time = QString("%1 %2")
                                                           .arg(sdrElement.attribute("Value", "N/A"),
                                                                sdrElement.attribute("Unit", "N/A"));
                }
                else if ("Sample" == sdrElement.tagName())
                {
                    QDomNode sampleNode = sdrElement.firstChild();
                    while (!sampleNode.isNull())
                    {
                        QDomElement sampleElement = sampleNode.toElement(); // try to convert the node to an element
                        if (!sampleElement.isNull())
                        {
                            if ("Samplerate" == sampleElement.tagName())
                            {
                                bool isOK = false;
                                int sampleRate = sampleElement.attribute("Value", "2048000").toInt(&isOK);
                                if (!isOK)
                                {
                                    qDebug() << "RAW-FILE: Error in reading XML header: Samplerate";
                                    sampleRate = 2048000;
                                }
                                else { /* OK */ }

                                QString unit = sampleElement.attribute("Unit", "Hz");
                                if ("hz" == unit.toLower())
                                {
                                    m_deviceDescription.sample.sampleRate = sampleRate;
                                }
                                else
                                {
                                    m_deviceDescription.sample.sampleRate = 1000 * sampleRate;
                                }
                            }
                            else if ("Channels" == sampleElement.tagName())
                            {
                                bool isOK = false;
                                int bits = sampleElement.attribute("Bits", "8").toInt(&isOK);
                                if (!isOK)
                                {
                                    qDebug() << "RAW-FILE: Error in reading XML header: Channels->Bits";
                                    bits = 8;
                                }
                                else { /* OK */ }
                                m_deviceDescription.sample.channelBits = bits;
                                m_deviceDescription.sample.channelContainer = sampleElement.attribute("Container", "uint8");
                                if ("uint8" == m_deviceDescription.sample.channelContainer.toLower())
                                {
                                    m_deviceDescription.sample.containerBits = 8;
                                    setFileFormat(RawFileInputFormat::SAMPLE_FORMAT_U8);
                                }
                                else if ("int16" == m_deviceDescription.sample.channelContainer.toLower())
                                {
                                    m_deviceDescription.sample.containerBits = 16;
                                    setFileFormat(RawFileInputFormat::SAMPLE_FORMAT_S16);
                                }
                                else
                                {
                                    qDebug() << QString("RAW-FILE: Channel container '%1' not supported").arg(m_deviceDescription.sample.channelContainer);
                                }
#if (Q_BYTE_ORDER == Q_LITTLE_ENDIAN)
                                if ("MSB" == sampleElement.attribute("Ordering", "LSB"))
                                {
                                    qDebug() << "RAW-FILE: Error in reading XML header: MSB ordering not supported on this platform";
                                }
#else
                                if ("LSB" == sampleElement.attribute("Ordering", "MSB"))
                                {
                                    qDebug() << "RAW-FILE: Error in reading XML header: LSB ordering not supported on this platform";
                                }
#endif

#if 0 // skipping this, QI is order is not supported
                                    QDomNode channelsNode = sampleElement.firstChild();
                                    while (!channelsNode.isNull())
                                    {
                                        QDomElement channelsElement = channelsNode.toElement(); // try to convert the node to an element
                                        if (!channelsElement.isNull())
                                        {
                                            if ("Channel" == channelsElement.tagName())
                                            {
                                                qDebug() << "Value:" << channelsElement.attribute("Value", "");
                                            }
                                        }
                                        channelsNode = channelsNode.nextSibling();
                                    }
#endif
                            }

                        }
                        sampleNode = sampleNode.nextSibling();
                    }
                }
                else if ("Datablocks" == sdrElement.tagName())
                {
                    QDomNode datablocksNode = sdrElement.firstChild();
                    if (!datablocksNode.isNull())
                    {
                        QDomElement datablocksElement = datablocksNode.toElement(); // try to convert the node to an element
                        if (!datablocksElement.isNull())
                        {
                            if ("Datablock" == datablocksElement.tagName())
                            {
                                //qDebug() << "Number:" << datablocksElement.attribute("Number", "1");
                                //qDebug() << "Unit:" << datablocksElement.attribute("Unit", "Channel");
                                bool isOK = false;
                                uint64_t numChannels = datablocksElement.attribute("Count", "0").toULongLong(&isOK);
                                if (!isOK)
                                {
                                    qDebug() << "RAW-FILE: Error in reading XML header: Datablock->Count";
                                    numChannels = 0;
                                }
                                else { /* OK */ }
                                m_deviceDescription.rawFile.numSamples = numChannels / 2;

                                QDomNode blockNode = datablocksElement.firstChild();
                                while (!blockNode.isNull())
                                {
                                    QDomElement blockElement = blockNode.toElement(); // try to convert the node to an element
                                    if (!blockElement.isNull())
                                    {
                                        if ("Frequency" == blockElement.tagName())
                                        {
                                            bool isOK = false;
                                            uint32_t freq = blockElement.attribute("Value", "0").toUInt(&isOK);
                                            if (!isOK)
                                            {
                                                qDebug() << "RAW-FILE: Error in reading XML header: Frequency";
                                                freq = 0;
                                            }
                                            else { /* OK */ }

                                            QString unit = blockElement.attribute("Unit", "kHz");
                                            if ("hz" == unit.toLower())
                                            {
                                                m_deviceDescription.rawFile.frequency_kHz = freq*1000;
                                            }
                                            else
                                            {
                                                m_deviceDescription.rawFile.frequency_kHz = freq;
                                            }

                                        }
                                    }
                                    blockNode = blockNode.nextSibling();
                                }
                            }
                        }
                    }

                }
            }
            sdrNode = sdrNode.nextSibling();
        }
    }
    else
    {   // error in parser
        m_deviceDescription.rawFile.hasXmlHeader = false;
    }
#if 0
    qDebug() << "Recorder:" << m_deviceDescription.rawFile.recorder;
    qDebug() << "Recording time:" << m_deviceDescription.rawFile.time;
    qDebug() << "Device name:" << m_deviceDescription.device.name;
    qDebug() << "Device model:" << m_deviceDescription.device.model;
    qDebug() << "Samplerate [Hz]:" << m_deviceDescription.sample.sampleRate;
    qDebug() << "Container:" << QString("%1 [%2 bits], channel data %3 bits")
                                    .arg(m_deviceDescription.sample.channelContainer)
                                    .arg(m_deviceDescription.sample.containerBits)
                                    .arg(m_deviceDescription.sample.channelBits);
    qDebug() << "Num samples:" << m_deviceDescription.rawFile.numSamples << "=>"
             << m_deviceDescription.rawFile.numSamples * 1.0 / m_deviceDescription.sample.sampleRate << "sec";
    qDebug() << "RF [kHz]:" << m_deviceDescription.rawFile.frequency_kHz;
#endif
}

void RawFileInput::stop()
{
    if (nullptr != m_inputTimer)
    {
        m_inputTimer->stop();
    }
    if (nullptr != m_worker)
    {
        m_worker->stop();
        m_worker->wait(INPUT_CHUNK_MS*2);
        m_worker = nullptr;
    }
}


RawFileWorker::RawFileWorker(QFile *inputFile, RawFileInputFormat sampleFormat, QObject *parent)
    : QThread(parent)
    , m_sampleFormat(sampleFormat)
    , m_inputFile(inputFile)
{
    m_stopRequest = false;
    m_elapsedTimer.start();
}

void RawFileWorker::trigger()
{
    m_semaphore.release();
}

void RawFileWorker::stop()
{
    m_stopRequest = true;
    m_semaphore.release();
}

void RawFileWorker::run()
{
    while(1)
    {
        m_semaphore.acquire();

        if (m_stopRequest)
        {   // stop request
            return;
        }

        qint64 elapsed = m_elapsedTimer.elapsed();
        int period = elapsed - m_lastTriggerTime;
        m_lastTriggerTime = elapsed;

        uint64_t samplesRead = 0;
        uint64_t input_chunk_iq_samples = period * 2048;

        // get FIFO space
        pthread_mutex_lock(&inputBuffer.countMutex);
        uint64_t count = inputBuffer.count;
        Q_ASSERT(count <= INPUT_FIFO_SIZE);

        while ((INPUT_FIFO_SIZE - count) < input_chunk_iq_samples*sizeof(float)*2)
        {

            pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
            count = inputBuffer.count;
        }
        pthread_mutex_unlock(&inputBuffer.countMutex);

        // there is enough room in buffer
        uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.head;

        switch (m_sampleFormat)
        {
        case RawFileInputFormat::SAMPLE_FORMAT_S16:
        {
            int16_t * tmpBuffer = new int16_t[input_chunk_iq_samples*2];
            uint64_t bytesRead = m_inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(int16_t));

            samplesRead = bytesRead >> 1;  // one sample is int16 (I or Q) => 2 bytes

            int16_t * inPtr = tmpBuffer;
            if (bytesTillEnd >= samplesRead * sizeof(float))
            {
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesRead; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
            }
            else
            {
                Q_ASSERT(sizeof(float) == 4);
                uint64_t samplesTillEnd = (bytesTillEnd >> 2);
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
                outPtr = (float *)(inputBuffer.buffer);
                for (uint64_t k=0; k<samplesRead-samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++);  // I or Q
                }
            }
            delete [] tmpBuffer;
        }
        break;
        case RawFileInputFormat::SAMPLE_FORMAT_U8:
        {
            uint8_t * tmpBuffer = new uint8_t[input_chunk_iq_samples*2];
            uint64_t bytesRead = m_inputFile->read((char *) tmpBuffer, input_chunk_iq_samples * 2 * sizeof(uint8_t));

            samplesRead = bytesRead;  // one sample is uint8 => 1 byte

            uint8_t * inPtr = tmpBuffer;
            if (bytesTillEnd >= samplesRead * sizeof(float))
            {
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesRead; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
            }
            else
            {
                Q_ASSERT(sizeof(float) == 4);
                uint64_t samplesTillEnd = (bytesTillEnd >> 2);
                float * outPtr = (float *)(inputBuffer.buffer + inputBuffer.head);
                for (uint64_t k=0; k < samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
                outPtr = (float *)(inputBuffer.buffer);
                for (uint64_t k=0; k<samplesRead-samplesTillEnd; k++)
                {   // convert to float
                    *outPtr++ = float(*inPtr++ - 128);  // I or Q
                }
            }
            delete [] tmpBuffer;
        }
        break;
        }

        inputBuffer.head = (inputBuffer.head + samplesRead*sizeof(float)) % INPUT_FIFO_SIZE;
        pthread_mutex_lock(&inputBuffer.countMutex);
        inputBuffer.count = inputBuffer.count + samplesRead*sizeof(float);
        pthread_cond_signal(&inputBuffer.countCondition);
        pthread_mutex_unlock(&inputBuffer.countMutex);

        if (samplesRead < input_chunk_iq_samples*2)
        {
            qDebug() << "RAW-FILE: End of file";
            m_inputFile->seek(0);
            emit endOfFile();
        }
    }
}
