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

#include "mp3encoder.h"

#include <lame/lame.h>

Mp3Encoder::Mp3Encoder() {}

Mp3Encoder::~Mp3Encoder()
{
    close();
}

void Mp3Encoder::close()
{
    if (nullptr != m_lame)
    {
        lame_close(m_lame);
        m_lame = nullptr;
    }
}

bool Mp3Encoder::init(int sampleRate, int numChannels)
{
    if (nullptr != m_lame && sampleRate == m_sampleRate && numChannels == m_numChannels)
    {
        return true;  // already configured for this format
    }
    close();

    m_lame = lame_init();
    if (nullptr == m_lame)
    {
        return false;
    }

    m_sampleRate = sampleRate;
    m_numChannels = numChannels;

    lame_set_in_samplerate(m_lame, sampleRate);
    lame_set_num_channels(m_lame, numChannels);
    lame_set_mode(m_lame, 1 == numChannels ? MONO : JOINT_STEREO);
    lame_set_brate(m_lame, 1 == numChannels ? 128 : 192);
    lame_set_quality(m_lame, 2);  // 2 = high quality, still fast enough for realtime encoding

    if (lame_init_params(m_lame) < 0)
    {
        close();
        return false;
    }
    return true;
}

QByteArray Mp3Encoder::encode(const QByteArray &pcm)
{
    if (nullptr == m_lame || pcm.isEmpty())
    {
        return QByteArray();
    }

    const int bytesPerFrame = 2 * m_numChannels;
    const int numFrames = pcm.size() / bytesPerFrame;
    if (0 == numFrames)
    {
        return QByteArray();
    }

    // Recommended output buffer size per the LAME API documentation.
    QByteArray out(int(1.25 * numFrames) + 7200, Qt::Uninitialized);

    const short *pcmData = reinterpret_cast<const short *>(pcm.constData());
    int written = lame_encode_buffer_interleaved(m_lame, const_cast<short *>(pcmData), numFrames,
                                                  reinterpret_cast<unsigned char *>(out.data()), out.size());
    if (written < 0)
    {
        return QByteArray();
    }
    out.resize(written);
    return out;
}

QByteArray Mp3Encoder::flush()
{
    if (nullptr == m_lame)
    {
        return QByteArray();
    }
    QByteArray out(7200, Qt::Uninitialized);
    int written = lame_encode_flush(m_lame, reinterpret_cast<unsigned char *>(out.data()), out.size());
    if (written < 0)
    {
        return QByteArray();
    }
    out.resize(written);
    return out;
}
