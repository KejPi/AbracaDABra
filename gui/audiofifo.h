#ifndef AUDIOFIFO_H
#define AUDIOFIFO_H

#include <QMutex>
#include <QWaitCondition>

#define AUDIO_FIFO_CHUNK_MS   (60)
#define AUDIO_FIFO_MS         (32 * AUDIO_FIFO_CHUNK_MS)
#define AUDIO_FIFO_SIZE       (48 * AUDIO_FIFO_MS * 2 * sizeof(int16_t))  // FS - 48kHz, stereo, int16_t samples


struct AudioFifo
{
    std::atomic<int64_t> count;
    int64_t head;
    int64_t tail;
    uint8_t buffer[AUDIO_FIFO_SIZE];    
    QWaitCondition countChanged;
    QMutex mutex;    
    void reset();
};

typedef struct AudioFifo audioFifo_t;

extern audioFifo_t audioBuffer;

#endif // AUDIOFIFO_H
