#ifndef INPUTDEVICESRC_H
#define INPUTDEVICESRC_H

#include <cstdint>

#define INPUTDEVICESRC_LEVEL_ESTIMATION 1
#define INPUTDEVICESRC_LEVEL_ATTACK  5e-5    // 50 usec
#define INPUTDEVICESRC_LEVEL_RELEASE 5e-2    // 50 msec

class InputDeviceSRCFilter;

// this class is used by input devices
class InputDeviceSRC
{
public:
    InputDeviceSRC(int inputSampleRate);
    ~InputDeviceSRC();
    void reset();
    void resetSignalLevel(float resetVal = 0.0);
    float signalLevel() const;

    // processing - returns number of output samples
    int process(float inDataIQ[], int numInDataIQ, float outDataIQ[]);
private:
    int m_inputSampleRate;
    InputDeviceSRCFilter * m_filter = nullptr;
};

class InputDeviceSRCFilter
{
public:
    virtual ~InputDeviceSRCFilter() {}   // virtual destructor to avoid undefined behavior

    // reset filter and signal level
    virtual void reset() = 0;

    // reset signal level memory
    void resetSignalLevel(float resetVal = 0.0) { m_signalLevel = resetVal; }

    // retunr current signal level
    float signalLevel() const { return m_signalLevel; }

    // processing - returns number of output samples
    virtual int process(float inDataIQ[], int numInDataIQ, float outDataIQ[]) = 0;

protected:
    float m_signalLevel;
};

//===================================================================================================
// DS2 filter designed for downsampling from 4096kHz to 2048kHz
class InputDeviceSRCFilterDS2 : public InputDeviceSRCFilter
{
public:
    InputDeviceSRCFilterDS2();
    ~InputDeviceSRCFilterDS2();
    void reset() override;

    // processing - returns signal level
    int process(float inDataIQ[], int numInDataIQ, float outDataIQ[]) override;
private:
    enum { FILTER_ORDER = 42 };

    float * bufferPtrI;
    float * bufferPtrQ;
    float * bufferI;
    float * bufferQ;

    // level filter
    float catt;
    float crel;

    // Halfband FIR, fixed coeffs, designed for downsampling 4096kHz -> 2048kHz
    static const int_fast8_t taps = FILTER_ORDER + 1;
    constexpr static const float coef[(FILTER_ORDER+2)/4 + 1] =
    {
         0.000223158782894952853123604619156594708,
        -0.00070774549637065342286290636764078954,
         0.001735782601167994458266075064045708132,
        -0.003619832275410410967614316390950079949,
         0.006788741778432844271862212082169207861,
        -0.01183550169261274320753329902800032869,
         0.019680477383812611941182879604639310855,
        -0.032073581325677551212560700832909788005,
         0.053382280107447499517547839786857366562,
        -0.099631117404426483563639749263529665768,
         0.316099577146216947909351802081800997257,
         0.5
    };
};

//===================================================================================================
// Transposed Farrow filter designed for downsampling from arbitrary rate to 2048kHz
class InputDeviceSRCFilterFarrow : public InputDeviceSRCFilter
{
public:
    InputDeviceSRCFilterFarrow(int inputSampleRate);
    ~InputDeviceSRCFilterFarrow();
    void reset() override;

    // processing - returns signal level
    int process(float inDataIQ[], int numInDataIQ, float outDataIQ[]) override;
private:
    enum { POLY_COEFS = 4, NUM_POLY = 6 };  // M = 4, N = 6

    // level filter
    float catt;
    float crel;

    float mu = 0;
    float xI[POLY_COEFS];
    float xQ[POLY_COEFS];
    float yI[NUM_POLY];
    float yQ[NUM_POLY];
    float R;   // FSout/FSin

    constexpr static const float coef[NUM_POLY][POLY_COEFS] =
    {
        {   0.001667349914006070960362,  0.032712194697834547085780, -0.146457831613232558609639,  0.004040531324696360060411  },
        {  -0.103347648141097675500433, -0.244367915078825215235980,  0.233146907266815583970043,  0.243745693669456003904727  },
        {   0.123959393981824803065983,  0.873574620095563081356715,  0.586104518066954516264389, -0.711183949124104208827646  },
        {   0.873450879200179830519346,  0.039931348783534291457809, -1.514110581690161660972649,  0.711183949124100878158572  },
        {   0.114518381640217964401174, -0.923204505507555395205088,  0.952958408884428287421997, -0.243745693669455892882425  },
        {  -0.104194288733202022889657,  0.243880907754789599817258, -0.134525637544989168370435, -0.004040531324696002707375  },
    };
};

#endif // INPUTDEVICESRC_H
