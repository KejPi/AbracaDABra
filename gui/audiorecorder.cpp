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

#include <QStandardPaths>
#include <QDebug>
#include <QLoggingCategory>
#include <QRegularExpression>
#include "audiorecorder.h"

Q_LOGGING_CATEGORY(audioRecorder, "AudioRecorder", QtDebugMsg)

AudioRecorder::AudioRecorder(QObject *parent) : QObject{parent},
    m_sid(0),
    m_doOutputRecording(false),
    m_file(nullptr)
{    
    m_recordingPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
}

AudioRecorder::~AudioRecorder()
{
    stop();
}

QString AudioRecorder::recordingPath() const
{
    return m_recordingPath;
}

void AudioRecorder::setup(const QString &recordingPath, bool doOutputRecording)
{
    m_recordingPath = recordingPath;
    m_doOutputRecording = doOutputRecording;
}

void AudioRecorder::setAudioService(const RadioControlServiceComponent &s)
{
    if (s.isAudioService())
    {
        stop();
        m_sid = s.SId;
        m_serviceName = s.label;
    }
}

void AudioRecorder::setDataFormat(int sampleRateKHz, bool isAAC)
{
    if (RecordingState::Stopped != m_recordingState)
    {   // recording is ongoing -> this can happen during reconfiguration or during announcement on other service
        stop();
    }

    m_sampleRateKHz = sampleRateKHz;
    m_isAAC = isAAC;
}

void AudioRecorder::writeWavHeader()
{
    QDataStream out(m_file);
    out.setByteOrder(QDataStream::LittleEndian);

    // RIFF chunk
    out.writeRawData("RIFF", 4);
    out << quint32(0);                // Placeholder for the RIFF chunk size (filled by close())
    out.writeRawData("WAVE", 4);

    // Format description chunk
    out.writeRawData("fmt ", 4);
    out << quint32(16);               // "fmt " chunk size (always 16 for PCM)
    out << quint16(1);                // data format (1 => PCM)
    out << quint16(2);                // num channels
    out << quint32(m_sampleRateKHz * 1000);
    out << quint32(m_sampleRateKHz * 1000 * 2 * sizeof(int16_t));   // bytes per second
    out << quint16(2 * sizeof(int16_t));                                       // Block align
    out << quint16(sizeof(int16_t) * 8);                                       // Significant Bits Per Sample

    // Data chunk
    out.writeRawData("data", 4);
    out << quint32(0);                                     // Placeholder for the data chunk size (filled by close())

    Q_ASSERT(m_file->pos() == 44);                         // Must be 44 for WAV PCM
}

void AudioRecorder::writeWav(const int16_t *data, size_t numSamples)
{
    qint64 bytesWritten = m_file->write(reinterpret_cast<const char *>(data), sizeof(int16_t) * numSamples);
    if (bytesWritten != sizeof(uint16_t) * numSamples)
    {
        qCDebug(audioRecorder) << "Error while recoding WAV data";
        stop();
        return;
    }
    m_bytesWritten += bytesWritten;
    m_timeWrittenMs += bytesWritten / (2 * sizeof(int16_t) * m_sampleRateKHz);
    if (m_timeWrittenMs >= (m_timeSec + 1) * 1000) {
        m_timeSec += 1;
        emit recordingProgress(m_bytesWritten, m_timeSec);
    }
}

void AudioRecorder::writeMP2(const std::vector<uint8_t> &data)
{
    qint64 bytesWritten = m_file->write(reinterpret_cast<const char *>(data.data()), sizeof(uint8_t) * data.size());
    if (bytesWritten != sizeof(uint8_t) * data.size())
    {
        qCDebug(audioRecorder) << "Error while recoding MP2 data";
        stop();
        return;
    }
    m_bytesWritten += bytesWritten;
    m_timeWrittenMs += (m_sampleRateKHz == 24 ? 48 : 24);

    if (m_timeWrittenMs >= (m_timeSec + 1) * 1000) {
        m_timeSec += 1;
        emit recordingProgress(m_bytesWritten, m_timeSec);
    }
}

void AudioRecorder::writeAAC(const std::vector<uint8_t> &data, const dabsdrAudioFrameHeader_t &aacHeader)
{
    uint8_t adts_sfreqidx;
    uint8_t audioFs;
    int timeMs;
    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 1))
    {
        adts_sfreqidx = 0x8;  // 16 kHz
        timeMs = 2*30;
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 1))
    {
        adts_sfreqidx = 0x6;  // 24 kHz
        timeMs = 2*20;
    }
    if ((aacHeader.bits.dac_rate == 0) && (aacHeader.bits.sbr_flag == 0))
    {
        adts_sfreqidx = 0x5;  // 32 kHz
        timeMs = 1*30;
    }
    if ((aacHeader.bits.dac_rate == 1) && (aacHeader.bits.sbr_flag == 0))
    {
        adts_sfreqidx = 0x3;  // 48 kHz
        timeMs = 1*20;
    }
    uint16_t au_size = data.size();

    uint8_t aac_header[20];

    // [11 bits] 0x2B7 syncword
    aac_header[0] = 0x2B7 >> 3;          // (8 bits)
    aac_header[1] = (0x2B7 << 5) & 0xE0; // (3 bits)
    // [13 bits] audioMuxLengthBytes - written later
    // aac_header[1]                   // (5 bits)
    aac_header[2] = 0;                 // (8 bits)

    // [1 bit] useSameStreamMux = 0
    // [1 bit] audioMuxVersion = 0
    // [1 bit] allStreamsSameTimeFraming = 1
    // [6 bits] numSubFrames = 000000
    // [4 bits] numProgram  = 0000
    // [3 bits] numLayer = 000
    aac_header[3] = 0b00100000;
    aac_header[4] = 0b00000000;

    uint8_t * aac_header_ptr = &aac_header[5];
    uint_fast8_t sbr_flag = aacHeader.bits.sbr_flag;

    if (sbr_flag)
    {
        // [5 bits] // SBR
        *aac_header_ptr = 0b00101 << 3;   // SBR
        // [4 bits] sampling freq index
        *aac_header_ptr++ |= (adts_sfreqidx >> 1) & 0x7;
        *aac_header_ptr  = (adts_sfreqidx & 0x1) << 7;
        // [4 bits] channel configuration
        *aac_header_ptr |= (aacHeader.bits.aac_channel_mode + 1) << 3;
        // [4 bits] extension sample rate index = 3 or 5
        *aac_header_ptr++ |= (5 - 2 * aacHeader.bits.dac_rate) >> 1;
        *aac_header_ptr  = ((5 - 2 * aacHeader.bits.dac_rate) & 0x1) << 7;
        // [5 bits] AAC LC
        *aac_header_ptr |= 0b00010 << 2;
        // [3 bits] GASpecificConfig() with 960 transform = 0b100
        *aac_header_ptr++ |= 0b10;
        //aac_header[8]  = 0b0 << 7;
        // [3 bits] frameLengthType = 0b000
        *aac_header_ptr = 0b000 << 4;
        // [8 bits] latmBufferFullness = 0xFF
        *aac_header_ptr++ |= 0x0F;
        *aac_header_ptr  = 0xF0;
        // [1 bit] otherDataPresent = 0
        // [1 bit] crcCheckPresent = 0;

        // 2 bits remainig
    }
    else
    {
        // [5 bits] AAC LC
        *aac_header_ptr = 0b00010 << 3;
        // [4 bits] samplingFrequencyIndex
        *aac_header_ptr++ |= (adts_sfreqidx >> 1) & 0x7;
        *aac_header_ptr  = (adts_sfreqidx & 0x1) << 7;
        // [4 bits] channel configuration
        *aac_header_ptr |= (aacHeader.bits.aac_channel_mode + 1) << 3;
        // [3 bits] GASpecificConfig() with 960 transform = 0b100
        *aac_header_ptr++ |= 0b100;
        // [3 bits] frameLengthType = 0b000
        // aac_header[7] = 0b000 << 5;
        // [8 bits] latmBufferFullness = 0xFF
        *aac_header_ptr++ = 0x1F;
        *aac_header_ptr = 0xE0;
        // [1 bit] otherDataPresent = 0
        // [1 bit] crcCheckPresent = 0;

        // 3 bits remainig
    }
    //	PayloadLengthInfo()
    // 0xFF for each 255 bytes
    int au_size_255 = au_size / 255;

    for (int i = 0; i < au_size_255; i++)
    {
        *aac_header_ptr++ |= (uint8_t)(0xFF >> (5 + sbr_flag));
        *aac_header_ptr = (uint8_t)(0xFF << (3 - sbr_flag));
    }
    // the rest is (au_size % 255);
    *aac_header_ptr++ |= (au_size % 255) >> (5 + sbr_flag);
    *aac_header_ptr = (au_size % 255) << (3 - sbr_flag);

    // total length = total size - 3 (length is in byte 1-2, thus -3 bytes)
    int len = 9 + aacHeader.bits.sbr_flag + au_size_255 + 1 + au_size - 3;

    aac_header[1] |= (len >> 8) & 0x1F;
    aac_header[2] = len & 0xFF;

    qint64 bytesWritten = m_file->write(reinterpret_cast<const char *>(aac_header), sizeof(uint8_t) * (9 + aacHeader.bits.sbr_flag + au_size_255));

    uint8_t byte = *aac_header_ptr;
    const uint8_t * auPtr = &data[0]; //&buffer[mscDataPtr->au_start[r]];

    for (int i = 0; i < au_size; ++i)
    {
        byte |= (*auPtr) >> (5 + sbr_flag);
        bytesWritten += m_file->write(reinterpret_cast<const char *>(&byte), sizeof(uint8_t));
        byte = (uint8_t)*auPtr++ << (3 - sbr_flag);
    }
    bytesWritten += m_file->write(reinterpret_cast<const char *>(&byte), sizeof(uint8_t));

    if (bytesWritten != sizeof(uint8_t) * (9 + aacHeader.bits.sbr_flag + au_size_255 + au_size + 1))
    {
        qCDebug(audioRecorder) << "Error while recoding AAC data";
        stop();
        return;
    }
    m_bytesWritten += bytesWritten;

    m_timeWrittenMs += timeMs;

    if (m_timeWrittenMs >= (m_timeSec + 1) * 1000) {
        m_timeSec += 1;
        emit recordingProgress(m_bytesWritten, m_timeSec);
    }
}

void AudioRecorder::start()
{
    static const QRegularExpression regexp( "[" + QRegularExpression::escape("/:*?\"<>|") + "]");
    if (nullptr == m_file)
    {
        QString servicename = m_serviceName;
        servicename.replace(regexp, "_");
        QString fileName // = m_recordingPath + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hhmmss");
                    = m_recordingPath + QString("/%1_%2_%3")
                                                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-hhmmss"),
                                                                  QString("%1").arg(m_sid.value(), 6, 16, QChar('0')).toUpper(),
                                                                  servicename);
        if (m_doOutputRecording)
        {
            fileName += ".wav";
            m_recordingState = RecordingState::RecordingWav;
        }
        else
        {
            if (m_isAAC)
            {
                fileName += ".aac";
                m_recordingState = RecordingState::RecordingAAC;
            }
            else
            {
                fileName += ".mp2";
                m_recordingState = RecordingState::RecordingMP2;
            }
        }

        qCInfo(audioRecorder) << "Recording file:" << fileName;

        m_file = new QFile(fileName);
        if (m_file->open(QIODevice::WriteOnly))
        {
            m_bytesWritten = 0;
            m_timeWrittenMs = 0;
            m_timeSec = 0;
            if (m_doOutputRecording)
            {   // reserving WAV header space
                m_file->write(QByteArray(44, '\x55'));
                m_bytesWritten = 44;
            }
            emit recordingStarted(fileName);
        }
        else
        {
            qCCritical(audioRecorder) << "Unable to open file:" << fileName;
            delete m_file;
            m_file = nullptr;
            m_recordingState = RecordingState::Stopped;
            emit recordingStopped();
        }
    }
    else
    { /* file is already opened */ }
}

void AudioRecorder::stop()
{
    if (nullptr != m_file)
    {
        if (m_doOutputRecording)
        {
            m_file->seek(0);
            writeWavHeader();
        }
        m_file->flush();
        m_file->close();
        delete m_file;
        m_file = nullptr;
        m_recordingState = RecordingState::Stopped;
        emit recordingStopped();

        qCInfo(audioRecorder) << "Audio recording stopped";
    }
    else
    { /* file is not opened */ }
}

void AudioRecorder::recordData(const RadioControlAudioData *inData, const int16_t * outputData, size_t numOutputSamples)
{
    if (RecordingState::Stopped == m_recordingState)
    {
        return;
    }

    switch (m_recordingState) {
    case RecordingState::RecordingAAC:
        writeAAC(inData->data, inData->header);
        break;
    case RecordingState::RecordingMP2:
        writeMP2(inData->data);
        break;
    case RecordingState::RecordingWav:
        writeWav(outputData, numOutputSamples);
        break;
    default:
        // do nothing
        break;
    }
}

