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

    pthread_cond_signal(&inputBuffer.countCondition);
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

void getSamples(float _Complex buffer[], uint16_t numSamples)
{
    //qDebug() << Q_FUNC_INFO;

    // input read -> lets store it to FIFO
    pthread_mutex_lock(&inputBuffer.countMutex);
    uint64_t count = inputBuffer.count;
    while (count < numSamples*sizeof(float _Complex))
    {
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
    }
    pthread_mutex_unlock(&inputBuffer.countMutex);

    // there is enough samples in input buffer
    uint64_t bytesTillEnd = INPUT_FIFO_SIZE - inputBuffer.tail;
    if (bytesTillEnd >= numSamples*sizeof(float _Complex))
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, numSamples*sizeof(float _Complex));
        inputBuffer.tail = (inputBuffer.tail + numSamples*sizeof(float _Complex));
    }
    else
    {
        memcpy(buffer, inputBuffer.buffer + inputBuffer.tail, bytesTillEnd);
        Q_ASSERT(sizeof(float _Complex) == 8);
        uint64_t samplesTillEnd = bytesTillEnd >> 3;
        memcpy(buffer+samplesTillEnd, inputBuffer.buffer, (numSamples - samplesTillEnd)*sizeof(float _Complex));
        inputBuffer.tail = (numSamples - samplesTillEnd) * sizeof(float _Complex);
    }

    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count - numSamples*sizeof(float _Complex);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
}
