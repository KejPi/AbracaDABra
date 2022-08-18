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
    float xI[POLY_COEFS];
    float xQ[POLY_COEFS];
    float yI[NUM_POLY];
    float yQ[NUM_POLY];
    float R;   // FSout/FSin

    constexpr static const float coef[NUM_POLY][POLY_COEFS] =
    {
        {  -0.000283801603017060847904, -0.015652886800781532633531,  0.078771669727326673604573, -0.012667502641195725610057  },
        {   0.046936812324678875429917,  0.119082292883666396310360, -0.221949584306413483236753, -0.017445441021029812339593  },
        {  -0.067266576995316768039501, -0.364454257711597728874864,  0.202645930859332773499304,  0.324214290002784844002548  },
        {   0.088922422856251195910637,  0.916405887347327863245994,  0.724121296748523435304890, -0.821678190026099097842405  },
        {   0.908918056369206639466540,  0.046584364522949388287554, -1.702397108172292172767470,  0.821678190026093435704979  },
        {   0.079589452711745889423867, -0.976990160626492909479168,  1.160091256023807027020212, -0.324214290002784677469094  },
        {  -0.067549799171746804926642,  0.368594541609575832019630, -0.273468152321642177238203,  0.017445441021029805400699  },
        {   0.048554231277252828113955, -0.102604630467881846600520,  0.041362950990045860288902,  0.012667502641195510504346  },
    };
};

#endif // INPUTDEVICESRC_H
