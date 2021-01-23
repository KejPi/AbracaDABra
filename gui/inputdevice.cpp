#include "inputdevice.h"

//input FIFO
fifo_t inputBuffer;


void ComplexFifo::reset()
{
#if INPUT_USE_PTHREADS
    pthread_mutex_unlock(&countMutex);
#else
    mutex.unlock();
#endif

    count = 0;
    head = 0;
    tail = 0;

#if INPUT_USE_PTHREADS
    pthread_mutex_unlock(&countMutex);
#else
    active = true;
    mutex.unlock();
#endif

}

InputDevice::InputDevice(QObject *parent) : QObject(parent)
{
    // init empty fifo
    inputBuffer.count = 0;
    inputBuffer.head = 0;
    inputBuffer.tail = 0;
#if INPUT_USE_PTHREADS
    pthread_mutex_init(&inputBuffer.countMutex, NULL);
    pthread_cond_init(&inputBuffer.countCondition, NULL);
#else
    inputBuffer.active = true;
#endif
}

void getSamples(float _Complex buffer[], uint16_t numSamples)
{
    //qDebug() << Q_FUNC_INFO;

    // input read -> lets store it to FIFO
#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
    if (!inputBuffer.active)
    {   // --> do not wait for samples, buffer is not active (EOF) [this is for correct thread termination]
        qDebug() << "!inputBuffer.active";
        inputBuffer.mutex.unlock();
        return;
    }
#endif
    uint64_t count = inputBuffer.count;
    while (count < numSamples*sizeof(float _Complex))
    {
#if INPUT_USE_PTHREADS
        pthread_cond_wait(&inputBuffer.countCondition, &inputBuffer.countMutex);
        count = inputBuffer.count;
#else
        inputBuffer.countChanged.wait(&inputBuffer.mutex);
        count = inputBuffer.count;

        if (!inputBuffer.active)
        {   // --> do not wait for samples, buffer is not active (EOF) [this is for correct thread termination]
            qDebug() << "!inputBuffer.active 2";
            inputBuffer.mutex.unlock();
            return;
        }
#endif
    }
#if INPUT_USE_PTHREADS
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.unlock();
#endif

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

    //inputBuffer.tail = (inputBuffer.tail + numSamples) % INPUT_FIFO_SIZE;
#if INPUT_USE_PTHREADS
    pthread_mutex_lock(&inputBuffer.countMutex);
    inputBuffer.count = inputBuffer.count - numSamples*sizeof(float _Complex);
    pthread_cond_signal(&inputBuffer.countCondition);
    pthread_mutex_unlock(&inputBuffer.countMutex);
#else
    inputBuffer.mutex.lock();
    inputBuffer.count = inputBuffer.count - len;
    inputBuffer.countChanged.wakeAll();
    inputBuffer.mutex.unlock();
#endif

}
