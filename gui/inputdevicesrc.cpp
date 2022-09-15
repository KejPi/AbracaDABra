#include <QDebug>
#include "inputdevicesrc.h"
#include <cmath>
#include <cstring>

InputDeviceSRC::InputDeviceSRC(float inputSampleRate)
{
    if (2*2048e3 == inputSampleRate)
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
    bufferI = new float[2*taps];
    bufferQ = new float[2*taps];
    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;

    catt = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_ATTACK * 2048e3));
    crel = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_RELEASE * 2048e3));

    InputDeviceSRCFilterDS2::reset();
}

InputDeviceSRCFilterDS2::~InputDeviceSRCFilterDS2()
{
    delete [] bufferI;
    delete [] bufferQ;
}

void InputDeviceSRCFilterDS2::reset()
{
    resetSignalLevel();

    bufferPtrI = bufferI;
    bufferPtrQ = bufferQ;
    for (int n = 0; n < 2*taps; ++n)
    {
        bufferI[n] = 0;
        bufferQ[n] = 0;
    }
}

int InputDeviceSRCFilterDS2::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
    float level = m_signalLevel;

    float * bufPtrI = bufferPtrI;
    float * bufPtrQ = bufferPtrQ;

    for (int n = 0; n<numInDataIQ/2; ++n)
    {
        float * fwdI = bufPtrI;
        float * revI = bufPtrI + taps;
        float * fwdQ = bufPtrQ;
        float * revQ = bufPtrQ + taps;


        *fwdI++ = *inDataIQ;
        *revI = *inDataIQ++;
        *fwdQ++ = *inDataIQ;
        *revQ = *inDataIQ++;

        float accI = 0;
        float accQ = 0;

        for (int c = 0; c < (taps+1)/4; ++c)
        {
            accI += (*fwdI + *revI) * coef[c];
            fwdI += 2;  // every other coeff is zero
            revI -= 2;  // every other coeff is zero
            accQ += (*fwdQ + *revQ) * coef[c];
            fwdQ += 2;  // every other coeff is zero
            revQ -= 2;  // every other coeff is zero
        }

        accI += *(fwdI-1) * coef[(taps+1)/4];
        accQ += *(fwdQ-1) * coef[(taps+1)/4];

        *outDataIQ++ = accI;
        *outDataIQ++ = accQ;

        bufPtrQ += 1;
        if (++bufPtrI == bufferI + taps)
        {
            bufPtrI = bufferI;
            bufPtrQ = bufferQ;
        }

#if (INPUTDEVICESRC_LEVEL_ESTIMATION > 0)
        float abs2 = (*inDataIQ) * (*inDataIQ);  // I*I

        // insert to delay line
        *(bufPtrI + taps) = *inDataIQ;           // I
        *bufPtrI++ = *inDataIQ++;                // I

        abs2 += (*inDataIQ) * (*inDataIQ);       // Q*Q

        // insert to delay line
        *(bufPtrQ + taps) = *inDataIQ;           // Q
        *bufPtrQ++ = *inDataIQ++;                // Q

        // calculate signal level (rectifier, fast attack slow release)
        float c = crel;
        if (abs2 > level)
        {
            c = catt;
        }
        level = c * abs2 + level - c * level;
#else
        // insert new samples to delay line
        *(bufPtrI + taps) = *inDataIQ;    // I
        *bufPtrI++ = *inDataIQ++;         // I
        *(bufPtrQ + taps) = *inDataIQ;    // Q
        *bufPtrQ++ = *inDataIQ++;         // Q
#endif
        if (bufPtrI == bufferI + taps)
        {
            bufPtrI = bufferI;
            bufPtrQ = bufferQ;
        }
    }

    bufferPtrI = bufPtrI;
    bufferPtrQ = bufPtrQ;

    // store signal level
    m_signalLevel = level;

    return numInDataIQ/2;
}

//===================================================================================================
// Transposed Farrow filter designed for downsampling from arbitrary rate to 2048kHz

InputDeviceSRCFilterFarrow::InputDeviceSRCFilterFarrow(float inputSampleRate)
{
    // output FS is fixed to 2048kHz
    R = 2048e3 / inputSampleRate;

    // calculate catt and crel
    catt = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_ATTACK * inputSampleRate));
    crel = 1 - std::exp(-1/(INPUTDEVICESRC_LEVEL_RELEASE * inputSampleRate));

    InputDeviceSRCFilterFarrow::reset();
}

InputDeviceSRCFilterFarrow::~InputDeviceSRCFilterFarrow()
{

}

void InputDeviceSRCFilterFarrow::reset()
{
    resetSignalLevel();

    mu = 0;
    for (int m = 0; m < POLY_COEFS; ++m)
    {
        xI[m] = 0.0;
        xQ[m] = 0.0;
    }

    for (int n = 0; n < NUM_POLY; ++n)
    {
        yI[n] = 0.0;
        yQ[n] = 0.0;
    }
}

int InputDeviceSRCFilterFarrow::process(float inDataIQ[], int numInDataIQ, float outDataIQ[])
{
    float level = m_signalLevel;
    int numOutDataIQ = 0;

    // do for all input samples
    do
    {
        mu = mu - R;
        if (mu < 0)
        {   // dump condition
            mu = mu + 1.0;
            // calc FIR for each polynomial
            for (int n = 0; n<NUM_POLY; ++n)
            {
                float accI = 0;
                float accQ = 0;
                for (int m = 0; m < POLY_COEFS; ++m)
                {
                    accI = accI + xI[m] * coef[n][m];
                    accQ = accQ + xQ[m] * coef[n][m];
                }
                yI[n] = yI[n] + accI;
                yQ[n] = yQ[n] + accQ;
            }
            *outDataIQ++ = R * yI[0];
            *outDataIQ++ = R * yQ[0];
            numOutDataIQ += 1;

            // shift delay line -> prepare for next dump
            std::memmove(&yI[0], &yI[1], (NUM_POLY-1)*sizeof(float));
            std::memmove(&yQ[0], &yQ[1], (NUM_POLY-1)*sizeof(float));
            yI[NUM_POLY-1] = 0.0;
            yQ[NUM_POLY-1] = 0.0;

            // dump xIQ
            for (int m = 0; m < POLY_COEFS; ++m)
            {
                xI[m] = 0.0;
                xQ[m] = 0.0;
            }
        }
        else { /* continue with integration */ }

        float inI = *inDataIQ++;
        float inQ = *inDataIQ++;

#if (INPUTDEVICESRC_LEVEL_ESTIMATION > 0)
        float abs2 = inI * inI + inQ * inQ;

        // calculate signal level (rectifier, fast attack slow release)
        float c = crel;
        if (abs2 > level)
        {
            c = catt;
        }
        level = c * abs2 + level - c * level;
#endif

        // Integrate
        xI[0] += inI;
        xQ[0] += inQ;

        for (int m = 1; m < POLY_COEFS; ++m)
        {
            inI = inI*mu;
            inQ = inQ*mu;

            xI[m] += inI;
            xQ[m] += inQ;
        }
    } while (--numInDataIQ > 0);

    // store signal level
    m_signalLevel = level;

    return numOutDataIQ;
}
