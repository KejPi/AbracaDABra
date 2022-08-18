#include "inputdevicesrc.h"

InputDeviceSRC::InputDeviceSRC(int inputSampleRate)
{
    m_inputSampleRate = inputSampleRate;
    if (2*2048000 == inputSampleRate)
    {
        m_filter = new InputDeviceSRCFilterDS2();
    }
    else
    {   // not supported currently
        m_filter = new InputDeviceSRCFilterDS2();
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
#define LEV_CATT 0.01
#define LEV_CREL 0.00001

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

        // insert new samples to delay line
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
        float c = LEV_CREL;
        if (abs2 > level)
        {
            c = LEV_CATT;
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
