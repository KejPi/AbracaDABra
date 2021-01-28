#include "audiofifo.h"

void AudioFifo::reset()
{
    mutex.lock();
    count = 0;
    head = 0;
    tail = 0;
    mutex.unlock();
};
