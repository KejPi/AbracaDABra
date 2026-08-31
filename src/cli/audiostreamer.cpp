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

#include "audiostreamer.h"

#include <cstring>

#define AUDIOSTREAMER_CHUNK_BYTES (4 * 1024)
#define AUDIOSTREAMER_WAIT_MS (200)

AudioFifoReader::AudioFifoReader(QObject *parent) : QObject(parent) {}

void AudioFifoReader::setFifo(audioFifo_t *fifo)
{
    m_fifo.store(fifo);
}

void AudioFifoReader::clearFifo()
{
    m_fifo.store(nullptr);
}

void AudioFifoReader::requestStop()
{
    m_stop.store(true);
}

bool AudioFifoReader::readChunk(audioFifo_t *fifo, int maxBytes, int waitMs, QByteArray *data, uint32_t *sampleRate, uint8_t *numChannels)
{
    fifo->mutex.lock();
    if (0 == fifo->count)
    {
        fifo->countChanged.wait(&fifo->mutex, waitMs);
    }
    int64_t count = fifo->count;
    if (count <= 0)
    {
        fifo->mutex.unlock();
        return false;
    }

    int64_t bytesToRead = qMin<int64_t>(count, maxBytes);
    bytesToRead &= ~int64_t(1);
    if (0 == bytesToRead)
    {
        fifo->mutex.unlock();
        return false;
    }

    data->resize(int(bytesToRead));
    int64_t bytesToEnd = AUDIO_FIFO_SIZE - fifo->tail;
    if (bytesToEnd < bytesToRead)
    {
        std::memcpy(data->data(), fifo->buffer + fifo->tail, size_t(bytesToEnd));
        std::memcpy(data->data() + bytesToEnd, fifo->buffer, size_t(bytesToRead - bytesToEnd));
        fifo->tail = bytesToRead - bytesToEnd;
    }
    else
    {
        std::memcpy(data->data(), fifo->buffer + fifo->tail, size_t(bytesToRead));
        fifo->tail += bytesToRead;
    }
    *sampleRate = fifo->sampleRate;
    *numChannels = fifo->numChannels;
    fifo->mutex.unlock();

    fifo->mutex.lock();
    fifo->count -= bytesToRead;
    fifo->countChanged.wakeAll();
    fifo->mutex.unlock();

    return true;
}

void AudioFifoReader::run()
{
    while (!m_stop.load())
    {
        audioFifo_t *fifo = m_fifo.load();
        if (nullptr == fifo)
        {
            QThread::msleep(50);
            continue;
        }

        QByteArray data;
        uint32_t sampleRate;
        uint8_t numChannels;
        if (!readChunk(fifo, AUDIOSTREAMER_CHUNK_BYTES, AUDIOSTREAMER_WAIT_MS, &data, &sampleRate, &numChannels))
        {
            continue;
        }

        emit chunkReady(data, int(sampleRate), int(numChannels));
    }
}

AudioStreamer::AudioStreamer(QObject *parent) : QObject(parent)
{
    m_reader = new AudioFifoReader();
    m_thread = new QThread(this);
    m_thread->setObjectName("audioStreamerThr");
    m_reader->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_reader, &AudioFifoReader::run);
    connect(m_thread, &QThread::finished, m_reader, &QObject::deleteLater);
    connect(m_reader, &AudioFifoReader::chunkReady, this, &AudioStreamer::audioChunk);

    m_thread->start();
}

AudioStreamer::~AudioStreamer()
{
    m_reader->requestStop();
    m_thread->quit();
    m_thread->wait();
}

void AudioStreamer::onStartAudio(audioFifo_t *buffer)
{
    // called directly (not queued): run() never returns to the reader thread's event loop,
    // so a queued call would never be delivered. setFifo() is atomic-backed and thread-safe.
    m_reader->setFifo(buffer);
}

void AudioStreamer::onSwitchAudio(audioFifo_t *buffer)
{
    m_reader->setFifo(buffer);
}

void AudioStreamer::onStopAudio()
{
    m_reader->clearFifo();
}
