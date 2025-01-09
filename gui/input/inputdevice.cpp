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

#include "inputdevice.h"

// input FIFO
fifo_t inputBuffer;

void ComplexFifo::reset()
{
    pthread_mutex_lock(&countMutex);

    count = 0;
    head = 0;
    tail = 0;

    pthread_mutex_unlock(&countMutex);
}

void ComplexFifo::fillDummy()
{
    pthread_mutex_lock(&countMutex);

    count = INPUT_FIFO_SIZE;

    pthread_cond_signal(&countCondition);
    pthread_mutex_unlock(&countMutex);
}

InputDevice::InputDevice(QObject *parent) : QObject(parent)
{
    // init empty fifo
    inputBuffer.count = 0;
    inputBuffer.head = 0;
    inputBuffer.tail = 0;
    pthread_mutex_init(&inputBuffer.countMutex, NULL);
    pthread_cond_init(&inputBuffer.countCondition, NULL);
}

InputDevice::~InputDevice()
{
    pthread_mutex_destroy(&inputBuffer.countMutex);
    pthread_cond_destroy(&inputBuffer.countCondition);
}

void getSamples(float buffer[], uint16_t numSamples)
{
    // input read -> lets store it to FIFO
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    uint_fast32_t numIQ = numSamples * 2;
    while (count < numIQ * sizeof(float))
    {
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
    }
    pthread_mutex_unlock(&inputBuffer.countMutex);

    // there is enough samples in input buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.tail;
    if (bytesTillEnd >= numIQ * sizeof(float))
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, numIQ * sizeof(float));
        inputBuffer.tail = (inputBuffer.tail + numIQ * sizeof(float));
    }
    else
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, bytesTillEnd);
        Q_ASSERT(2 * sizeof(float) == 8);
        uint64_t numIQtillEnd = bytesTillEnd >> 2;
        memcpy(buffer + numIQtillEnd, inputBuffer.buffer, (numIQ - numIQtillEnd) * sizeof(float));
        inputBuffer.tail = (numIQ - numIQtillEnd) * sizeof(float);
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count - numIQ * sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

void skipSamples(float buffer[], uint16_t numSamples)
{
    (void)buffer;

    // input read -> lets store it to FIFO
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    while (count < numSamples * 2 * sizeof(float))
    {
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
    }

    inputBuffer.tail = (inputBuffer.tail + numSamples * 2 * sizeof(float)) % INPUT_FIFO_SIZE;

    inputBuffer.count = inputBuffer.count - numSamples * 2 * sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
