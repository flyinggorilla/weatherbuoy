#ifndef INCLUDE_VELOCITY_VECTOR_H_
#define INCLUDE_VELOCITY_VECTOR_H_

class VelocityVector
{
public:
    VelocityVector();

    void clear();
    void add(float speed, short direction);
    short getDir();
    float getSpeed();
    float getU() { return mfSumU; };
    float getV() { return mfSumV; };
    unsigned int getCount() { return muiCount; };
    void operator= (const VelocityVector& v);
    void operator+= (const VelocityVector& v);

protected:
    unsigned int muiCount;
    float mfSumU;
    float mfSumV;
    friend class VelocityVectorMovingAverage;
};

class VelocityVectorMovingAverage
{
public:
    VelocityVectorMovingAverage(unsigned short intervals); // matches AvgLong intervals from Maximet
    virtual ~VelocityVectorMovingAverage();
    void add(VelocityVector& rVelocityVector);
    short getDir();
    float getSpeed();
    float getU();
    float getV();
    unsigned int getCount();

private:
    short unsigned int musEntryPos = 0;
    short unsigned int musEntries = 0;
    short unsigned int musIntervals = 0;
    /*unsigned int *mpuiCounts = nullptr;
    float* mpSumsU = nullptr;
    float* mpSumsV = nullptr;*/
    VelocityVector* mpVelocityVectorArray = nullptr;
    VelocityVector mVelocityVectorAvg;
};

#endif