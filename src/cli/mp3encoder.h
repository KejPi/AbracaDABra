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

#ifndef MP3ENCODER_H
#define MP3ENCODER_H

#include <QByteArray>

struct lame_global_struct;

// Thin wrapper around libmp3lame that turns interleaved 16-bit LE PCM chunks (as produced by
// AudioStreamer) into a continuous MP3 elementary stream. This lets the CLI's live audio be
// played back through a standard <audio> element instead of being manually decoded with the
// Web Audio API, and incidentally makes it a genuine MP3 stream for external players.
class Mp3Encoder
{
public:
    Mp3Encoder();
    ~Mp3Encoder();

    // (Re)initializes the encoder for the given input format.
    bool init(int sampleRate, int numChannels);

    // pcm must contain interleaved 16-bit LE samples matching the format passed to init().
    // Returns encoded MP3 bytes; may be empty while LAME is still buffering internally.
    QByteArray encode(const QByteArray &pcm);

    // Flushes any samples still buffered inside the encoder.
    QByteArray flush();

private:
    void close();

    lame_global_struct *m_lame = nullptr;
    int m_sampleRate = 0;
    int m_numChannels = 0;
};

#endif  // MP3ENCODER_H
