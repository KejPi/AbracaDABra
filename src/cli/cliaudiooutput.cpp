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

#include "cliaudiooutput.h"

#include <QDebug>
#include <QLoggingCategory>
#include <algorithm>
#include <cstring>

#include "audiostreamer.h"

Q_LOGGING_CATEGORY(cliAudioOutput, "CliAudioOutput")

#define CLIAUDIOOUTPUT_CHUNK_BYTES (4 * 1024)
#define CLIAUDIOOUTPUT_WAIT_MS (200)

CliAudioWorker::CliAudioWorker(QObject *parent) : QObject(parent) {}

void CliAudioWorker::setFifo(audioFifo_t *fifo)
{
    m_fifo.store(fifo);
}

void CliAudioWorker::clearFifo()
{
    m_fifo.store(nullptr);
}

void CliAudioWorker::requestStop()
{
    m_stop.store(true);
}

void CliAudioWorker::setVolume(float linear)
{
    m_volume.store(std::clamp(linear, 0.0f, 1.0f));
}

void CliAudioWorker::ensureStream(uint32_t sampleRate, uint8_t numChannels)
{
    if (nullptr != m_stream && m_streamSampleRate == sampleRate && m_streamNumChannels == numChannels)
    {  // already open with matching parameters
        return;
    }

    closeStream();

    PaError err = Pa_OpenDefaultStream(&m_stream, 0 /* no input channels */, numChannels, paInt16, sampleRate,
                                        paFramesPerBufferUnspecified, nullptr /* blocking API, no callback */, nullptr);
    if (paNoError != err)
    {
        qCWarning(cliAudioOutput, "Pa_OpenDefaultStream() failed: %s", Pa_GetErrorText(err));
        m_stream = nullptr;
        return;
    }

    err = Pa_StartStream(m_stream);
    if (paNoError != err)
    {
        qCWarning(cliAudioOutput, "Pa_StartStream() failed: %s", Pa_GetErrorText(err));
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        return;
    }

    m_streamSampleRate = sampleRate;
    m_streamNumChannels = numChannels;
}

void CliAudioWorker::closeStream()
{
    if (nullptr != m_stream)
    {
        if (!Pa_IsStreamStopped(m_stream))
        {
            Pa_StopStream(m_stream);
        }
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
}

void CliAudioWorker::run()
{
    while (!m_stop.load())
    {
        audioFifo_t *fifo = m_fifo.load();
        if (nullptr == fifo)
        {
            closeStream();
            QThread::msleep(50);
            continue;
        }

        QByteArray data;
        uint32_t sampleRate;
        uint8_t numChannels;
        if (!AudioFifoReader::readChunk(fifo, CLIAUDIOOUTPUT_CHUNK_BYTES, CLIAUDIOOUTPUT_WAIT_MS, &data, &sampleRate, &numChannels))
        {
            continue;
        }
        int64_t bytesToRead = data.size();

        ensureStream(sampleRate, numChannels);
        if (nullptr != m_stream)
        {
            const float volume = m_volume.load();
            if (volume < 0.999f)
            {  // skip the multiply/clamp pass entirely at full volume (the common case)
                int16_t *samples = reinterpret_cast<int16_t *>(data.data());
                const int numSamples = int(bytesToRead / sizeof(int16_t));
                for (int i = 0; i < numSamples; ++i)
                {
                    samples[i] = int16_t(std::clamp(int(samples[i] * volume), -32768, 32767));
                }
            }

            int bytesPerFrame = int(numChannels) * int(sizeof(int16_t));
            unsigned long frames = (unsigned long)(bytesToRead / bytesPerFrame);
            PaError err = Pa_WriteStream(m_stream, data.constData(), frames);
            if (paNoError != err && paOutputUnderflowed != err)
            {
                qCWarning(cliAudioOutput, "Pa_WriteStream() failed: %s", Pa_GetErrorText(err));
            }
        }
    }
    closeStream();
}

CliAudioOutput::CliAudioOutput(QObject *parent) : QObject(parent)
{
    PaError err = Pa_Initialize();
    if (paNoError != err)
    {
        qCCritical(cliAudioOutput, "Pa_Initialize() failed: %s", Pa_GetErrorText(err));
    }
    else
    {
        m_paInitialized = true;
    }

    m_worker = new CliAudioWorker();
    m_thread = new QThread(this);
    m_thread->setObjectName("cliAudioOutThr");
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &CliAudioWorker::run);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
}

CliAudioOutput::~CliAudioOutput()
{
    m_worker->requestStop();
    m_thread->quit();
    m_thread->wait();

    if (m_paInitialized)
    {
        Pa_Terminate();
    }
}

void CliAudioOutput::onStartAudio(audioFifo_t *buffer)
{
    m_worker->setFifo(buffer);
}

void CliAudioOutput::onSwitchAudio(audioFifo_t *buffer)
{
    m_worker->setFifo(buffer);
}

void CliAudioOutput::onStopAudio()
{
    m_worker->clearFifo();
}

void CliAudioOutput::setVolume(float linear)
{
    m_worker->setVolume(linear);
}
