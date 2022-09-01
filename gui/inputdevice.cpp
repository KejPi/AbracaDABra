#include "inputdevice.h"

//input FIFO
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
    //qDebug() << Q_FUNC_INFO;

    // input read -> lets store it to FIFO
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    while (count < numSamples*2*sizeof(float))
    {
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
    }
    pthread_mutex_unlock(&inputBuffer.countMutex);

    // there is enough samples in input buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.tail;
    if (bytesTillEnd >= numSamples*2*sizeof(float))
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, numSamples*2*sizeof(float));
        inputBuffer.tail = (inputBuffer.tail + numSamples*2*sizeof(float));
    }
    else
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, bytesTillEnd);
        Q_ASSERT(2*sizeof(float) == 8);
        uint64_t samplesTillEnd = bytesTillEnd >> 3;
        memcpy(buffer+samplesTillEnd, inputBuffer.buffer, (numSamples - samplesTillEnd)*2*sizeof(float));
        inputBuffer.tail = (numSamples - samplesTillEnd) * 2*sizeof(float);
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count - numSamples*2*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}

void skipSamples(float buffer[], uint16_t numSamples)
{
    (void) buffer;

    // input read -> lets store it to FIFO
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    while (count < numSamples*2*sizeof(float))
    {
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
    }

    inputBuffer.tail = (inputBuffer.tail + numSamples*2*sizeof(float)) % INPUT_FIFO_SIZE;

    inputBuffer.count = inputBuffer.count - numSamples*2*sizeof(float);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
