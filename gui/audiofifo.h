#ifndef AUDIOFIFO_H
#define AUDIOFIFO_H

#include <QMutex>
#include <QWaitCondition>

#define AUDIO_FIFO_CHUNK_MS   (120)
#define AUDIO_FIFO_MS         (16 * AUDIO_FIFO_CHUNK_MS)
#define AUDIO_FIFO_SIZE       (48 * AUDIO_FIFO_MS * 2 * sizeof(qint16))  // FS - 48kHz, stereo, qint16 samples


struct AudioFifo
{
    qint64 count;
    qint64 head;
    qint64 tail;
    quint8 bytesPerFrame;
    quint8 buffer[AUDIO_FIFO_SIZE];    
    QWaitCondition countChanged;
    QMutex mutex;    
    void reset();
};

typedef struct AudioFifo audioFifo_t;

extern audioFifo_t audioBuffer;

#endif // AUDIOFIFO_H
