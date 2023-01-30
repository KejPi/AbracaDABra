/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
  * Copyright (c) 2019-2023 Petr Kopecký <xkejpi (at) gmail (dot) com>
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

#include <QDebug>
#include "inputdevicesrc.h"
#include <cmath>
#include <cstring>

InputDeviceSRC::InputDeviceSRC(float inputSampleRate)
{
    if (2048e3 == inputSampleRate)
    {
        m_filter = new InputDeviceSRCPassthrough();
    }
    else if (2*2048e3 == inputSampleRate)
    {
        m_filter = new InputDeviceSRCFilterDS2();
    }
    else
    {
        m_filter = new InputDeviceSRCFilterFarrow(inputSampleRate);
    }
}

InputDeviceSRC::~InputDeviceSRC()
{
    delete m_filter;
}

void InputDeviceSRC::reset()
{
    m_filter->reset();
}

void InputDeviceSRC::resetSignalLevel(float resetVal)
{
    m_filter->resetSignalLevel(resetVal);
}

float InputDeviceSRC::signalLevel() const
{
    return m_filter->signalLevel();
}

int InputDeviceSRC::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
    return m_filter->process(inDataIQ, numInDataIQ, outDataIQ);
}

//===================================================================================================
// DS2 filter designed for downsampling from 4096kHz to 2048kHz

InputDeviceSRCFilterDS2::InputDeviceSRCFilterDS2()
{
    m_bufferI = new float[2*m_taps];
    m_bufferQ = new float[2*m_taps];
    m_bufferPtrI = m_bufferI;
    m_bufferPtrQ = m_bufferQ;

    m_catt = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_ATTACK * 2048e3));
    m_crel = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_RELEASE * 2048e3));

    InputDeviceSRCFilterDS2::reset();
}

InputDeviceSRCFilterDS2::~InputDeviceSRCFilterDS2()
{
    delete [] m_bufferI;
    delete [] m_bufferQ;
}

void InputDeviceSRCFilterDS2::reset()
{
    resetSignalLevel();

    m_bufferPtrI = m_bufferI;
    m_bufferPtrQ = m_bufferQ;
    for (int n = 0; n < 2*m_taps; ++n)
    {
        m_bufferI[n] = 0;
        m_bufferQ[n] = 0;
    }
}

int InputDeviceSRCFilterDS2::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
    float level = m_signalLevel;

    float * bufPtrI = m_bufferPtrI;
    float * bufPtrQ = m_bufferPtrQ;

    for (int n = 0; n<numInDataIQ/2; ++n)
    {
        float * fwdI = bufPtrI;
        float * revI = bufPtrI + m_taps;
        float * fwdQ = bufPtrQ;
        float * revQ = bufPtrQ + m_taps;


        *fwdI++ = *inDataIQ;
        *revI = *inDataIQ++;
        *fwdQ++ = *inDataIQ;
        *revQ = *inDataIQ++;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (m_taps+1)/4; ++c)
        {
            accI += (*fwdI + *revI) * m_coef[c];
            fwdI += 2;  // every other coeff is zero
            revI -= 2;  // every other coeff is zero
            accQ += (*fwdQ + *revQ) * m_coef[c];
            fwdQ += 2;  // every other coeff is zero
            revQ -= 2;  // every other coeff is zero
        }

        accI += *(fwdI-1) * m_coef[(m_taps+1)/4];
        accQ += *(fwdQ-1) * m_coef[(m_taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

        bufPtrQ += 1;
        if (++bufPtrI == m_bufferI + m_taps)
        {
            bufPtrI = m_bufferI;
            bufPtrQ = m_bufferQ;
        }

#if (INPUTDEVICESRC_LEVEL_ESTIMATION > 0)
        float abs2 = (*inDataIQ) * (*inDataIQ);  // I*I

        // insert to delay line
        *(bufPtrI + m_taps) = *inDataIQ;           // I
        *bufPtrI++ = *inDataIQ++;                // I

        abs2 += (*inDataIQ) * (*inDataIQ);       // Q*Q

        // insert to delay line
        *(bufPtrQ + m_taps) = *inDataIQ;           // Q
        *bufPtrQ++ = *inDataIQ++;                // Q

        // calculate signal level (rectifier, fast attack slow release)
        float c = m_crel;
        if (abs2 > level)
        {
            c = m_catt;
        }
        level = c * abs2 + level - c * level;
#else
        // insert new samples to delay line
        *(bufPtrI + taps) = *inDataIQ;    // I
        *bufPtrI++ = *inDataIQ++;         // I
        *(bufPtrQ + taps) = *inDataIQ;    // Q
        *bufPtrQ++ = *inDataIQ++;         // Q
#endif
        if (bufPtrI == m_bufferI + m_taps)
        {
            bufPtrI = m_bufferI;
            bufPtrQ = m_bufferQ;
        }
    }

    m_bufferPtrI = bufPtrI;
    m_bufferPtrQ = bufPtrQ;

    // store signal level
    m_signalLevel = level;

    return numInDataIQ/2;
}

//===================================================================================================
// Transposed Farrow filter designed for downsampling from arbitrary rate to 2048kHz

InputDeviceSRCFilterFarrow::InputDeviceSRCFilterFarrow(float inputSampleRate)
{
    // output FS is fixed to 2048kHz
    m_R = 2048e3 / inputSampleRate;

    // calculate catt and crel
    m_catt = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_ATTACK * inputSampleRate));
    m_crel = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_RELEASE * inputSampleRate));

    InputDeviceSRCFilterFarrow::reset();
}

InputDeviceSRCFilterFarrow::~InputDeviceSRCFilterFarrow()
{

}

void InputDeviceSRCFilterFarrow::reset()
{
    resetSignalLevel();

    m_mu = 0;
    for (int m = 0; m < POLY_COEFS; ++m)
    {
        m_xI[m] = 0.0;
        m_xQ[m] = 0.0;
    }

    for (int n = 0; n < NUM_POLY; ++n)
    {
        m_yI[n] = 0.0;
        m_yQ[n] = 0.0;
    }
}

int InputDeviceSRCFilterFarrow::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
    float level = m_signalLevel;
    int numOutDataIQ = 0;

    // do for all input samples
    do
    {
        m_mu = m_mu - m_R;
        if (m_mu < 0)
        {   // dump condition
            m_mu = m_mu + 1.0;
            // calc FIR for each polynomial
            for (int n = 0; n<NUM_POLY; ++n)
            {
                float accI = 0;
                float accQ = 0;
                for (int m = 0; m < POLY_COEFS; ++m)
                {
                    accI = accI + m_xI[m] * m_coef[n][m];
                    accQ = accQ + m_xQ[m] * m_coef[n][m];
                }
                m_yI[n] = m_yI[n] + accI;
                m_yQ[n] = m_yQ[n] + accQ;
            }
            *outDataIQ++ = m_R * m_yI[0];
            *outDataIQ++ = m_R * m_yQ[0];
            numOutDataIQ += 1;

            // shift delay line -> prepare for next dump
            std::memmove(&m_yI[0], &m_yI[1], (NUM_POLY-1)*sizeof(float));
            std::memmove(&m_yQ[0], &m_yQ[1], (NUM_POLY-1)*sizeof(float));
            m_yI[NUM_POLY-1] = 0.0;
            m_yQ[NUM_POLY-1] = 0.0;

            // dump xIQ
            for (int m = 0; m < POLY_COEFS; ++m)
            {
                m_xI[m] = 0.0;
                m_xQ[m] = 0.0;
            }
        }
        else { /* continue with integration */ }

        float inI = *inDataIQ++;
        float inQ = *inDataIQ++;

#if (INPUTDEVICESRC_LEVEL_ESTIMATION > 0)
        float abs2 = inI * inI + inQ * inQ;

        // calculate signal level (rectifier, fast attack slow release)
        float c = m_crel;
        if (abs2 > level)
        {
            c = m_catt;
        }
        level = c * abs2 + level - c * level;
#endif

        // Integrate
        m_xI[0] += inI;
        m_xQ[0] += inQ;

        for (int m = 1; m < POLY_COEFS; ++m)
        {
            inI = inI*m_mu;
            inQ = inQ*m_mu;

            m_xI[m] += inI;
            m_xQ[m] += inQ;
        }
    } while (--numInDataIQ > 0);

    // store signal level
    m_signalLevel = level;

    return numOutDataIQ;
}

InputDeviceSRCPassthrough::InputDeviceSRCPassthrough()
{
    m_catt = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_ATTACK * 2048e3));
    m_crel = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_RELEASE * 2048e3));

    InputDeviceSRCPassthrough::reset();
}

InputDeviceSRCPassthrough::~InputDeviceSRCPassthrough()
{

}

void InputDeviceSRCPassthrough::reset()
{
    resetSignalLevel();
}

int InputDeviceSRCPassthrough::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
#if (INPUTDEVICESRC_LEVEL_ESTIMATION > 0)
    float level = m_signalLevel;

    for (int n = 0; n < numInDataIQ; ++n)
    {
        float abs2 = (*inDataIQ) * (*inDataIQ);
        *outDataIQ++ = *inDataIQ++;
        abs2 += (*inDataIQ) * (*inDataIQ);
        *outDataIQ++ = *inDataIQ++;

        // calculate signal level (rectifier, fast attack slow release)
        float c = m_crel;
        if (abs2 > level)
        {
            c = m_catt;
        }
        level = c * abs2 + level - c * level;
    }

    // store signal level
    m_signalLevel = level;
#else
    std::memcpy(outDataIQ, inDataIQ, numInDataIQ * 2*sizeof(float));
#endif

    return numInDataIQ;
}
