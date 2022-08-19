#ifndef INPUTDEVICESRC_H
#define INPUTDEVICESRC_H

#include <cstdint>

#define HAVE_SSE3 0

#define INPUTDEVICESRC_LEVEL_ESTIMATION 1
#define INPUTDEVICESRC_LEVEL_ATTACK  5e-5    // 50 usec
#define INPUTDEVICESRC_LEVEL_RELEASE 5e-2    // 50 msec

#if HAVE_SSE3
#ifdef __GNUC__
#define INPUTDEVICESRC_ALIGN(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#define INPUTDEVICESRC_ALIGN(x) __declspec(align(x))
#else
#define INPUTDEVICESRC_ALIGN(x)
#endif
#else
#define INPUTDEVICESRC_ALIGN(x)
#endif // HAVE_SSE2


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
    enum { POLY_COEFS = 4, NUM_POLY = 8 };  // M = 4, N = 8

    // level filter
    float catt;
    float crel;

    float mu = 0;
    float xI[POLY_COEFS] INPUTDEVICESRC_ALIGN(16);
    float xQ[POLY_COEFS] INPUTDEVICESRC_ALIGN(16);
    float yI[NUM_POLY];
    float yQ[NUM_POLY];
    float R;   // FSout/FSin

    constexpr static const float coef[NUM_POLY][POLY_COEFS] INPUTDEVICESRC_ALIGN(16) =
    {
        {  -0.000281562528833651036075, -0.015899103853663387742046,  0.085935707278077533288752, -0.016679464303555484316899  },
        {   0.049747825207855991824779,  0.121785659855580680188680, -0.235273593230387656483060, -0.011278968636526921776042  },
        {  -0.068802546715671847321616, -0.370426695852706489020534,  0.216866047583218951588790,  0.318055100751438113437786  },
        {   0.089402054026202826264580,  0.920599194901912332156257,  0.713940595635137675856186, -0.816239342690488145493077  },
        {   0.908848299086838062876836,  0.046623678685464998994181, -1.696516213247709847777855,  0.816239342690486591180843  },
        {   0.080078609095998773415026, -0.981108922398868821268536,  1.156122516989809945187062, -0.318055100751437780370878  },
        {  -0.069106634075037753905946,  0.374196990380008032150982, -0.268581797485131057445784,  0.011278968636526814223187  },
        {   0.051429186746400096241771, -0.104799910313324529109735,  0.036679164256640521546426,  0.016679464303555300436210  },
    };
};

#endif // INPUTDEVICESRC_H
