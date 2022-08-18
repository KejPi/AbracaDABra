#ifndef INPUTDEVICESRC_H
#define INPUTDEVICESRC_H

#include <cstdint>

#define INPUTDEVICESRC_LEVEL_ESTIMATION 1

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


#endif // INPUTDEVICESRC_H
